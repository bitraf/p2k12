#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "array.h"
#include "postgresql.h"

#define GREEN_ON "\033[32;1m"
#define GREEN_OFF "\033[00m"
#define YELLOW_ON "\033[33;1m"
#define YELLOW_OFF "\033[00m"

typedef ARRAY (char *) stringlist;

char *
trim (char *string)
{
  size_t length;

  if (!string)
    return 0;

  length = strlen (string);

  while (length)
    {
      if (isspace (*string))
        memmove (string, string + 1, --length);
      else if (isspace (string[length - 1]))
        string[--length] = 0;
      else
        break;
    }

  return string;
}

static int
argv_parse (stringlist *result, char *str)
{
  char *option = 0, *option_start;
  int ch, escape_char = 0;

  ARRAY_INIT (result);

  for (;;)
    {
      ch = *str;

      if (ch == '\\')
        {
          if (escape_char != '\'' && !*++str)
            break;

          switch (*str)
            {
            case 'a': ch = '\a'; break;
            case 'b': ch = '\b'; break;
            case 't': ch = '\t'; break;
            case 'n': ch = '\n'; break;
            case 'v': ch = '\v'; break;
            case 'f': ch = '\f'; break;
            case 'r': ch = '\r'; break;
            default: ch = *str;
            }
        }
      else if (escape_char)
        {
          if (!ch)
            {
              fprintf (stderr, "Parse error: Missing %c\n", escape_char);

              ARRAY_FREE (result);

              return -1;
            }

          if (ch == escape_char)
            {
              escape_char = 0;
              ++str;

              continue;
            }
        }
      else if (ch == '\'' || ch == '"')
        {
          escape_char = ch;
          ++str;

          continue;
        }
      else if (ch == 0 || isspace (ch))
        {
          if (option)
            {
              *option = 0;

              ARRAY_ADD (result, option_start);

              if (-1 == ARRAY_RESULT (result))
                {
                  fprintf (stderr, "ARRAY_ADD failed: %s\n", strerror (errno));

                  ARRAY_FREE (result);

                  return -1;
                }

              option = 0;
            }

          if (!ch)
            return 0;

          ++str;

          continue;
        }

      if (!option)
        option_start = option = str;

      ++str;

      *option++ = ch;
    }

  return 0;
}

static void
disable_icanon (void)
{
  struct termios t;

  tcgetattr(0, &t);

  t.c_lflag &= ~ICANON;

  tcsetattr(0, TCSANOW, &t);
}

static void
disable_echo (void)
{
  struct termios t;

  tcgetattr(0, &t);

  t.c_lflag &= ~ECHO;

  tcsetattr(0, TCSANOW, &t);
}

static void
enable_icanon (void)
{
  struct termios t;

  tcgetattr(0, &t);

  t.c_lflag |= ICANON;

  tcsetattr(0, TCSANOW, &t);
}

static void
enable_echo (void)
{
  struct termios t;

  tcgetattr(0, &t);

  t.c_lflag |= ECHO;

  tcsetattr(0, TCSANOW, &t);
}

static long long
sql_last_id (void)
{
  if (-1 == SQL_Query ("SELECT LASTVAL()"))
    return -1;

  return strtoll (SQL_Value (0, 0), 0, 0);
}


static void
cmd_addproduct (const char *product_name)
{
  SQL_Query ("INSERT INTO accounts (name, type) VALUES (%s, 'product')", product_name);
}

static void
cmd_become (int user_id, const char *price)
{
  if (!strcmp (price, "300"))
    price = "300";

  if (strcmp (price, "500")
      && strcmp (price, "1500")
      && strcmp (price, "1000")
      && strcmp (price, "300")
      && strcmp (price, "0"))
    {
      fprintf (stderr, "Unknown membership type\n");

      return;
    }

  SQL_Query ("INSERT INTO members (full_name, email, price, account) SELECT full_name, email, %s, account FROM members WHERE account = %d ORDER BY date DESC LIMIT 1", price, user_id);
}

static void
cmd_addstock (int user_id, const char *product_id, const char *sum_value, const char *stock)
{
  char *endptr;

  if (0 >= strtol (product_id, &endptr, 0) || *endptr)
    fprintf (stderr, "Invalid product ID.  Must be a positive integer\n");
  else if (0 >= strtol (stock, &endptr, 0) || *endptr)
    fprintf (stderr, "Invalid stock count.  Must be a positive integer\n");
  else if (!strchr (sum_value, '.'))
    fprintf (stderr, "Sum value MUST contain a decimal separator (.)\n");
  else if (0 >= strtod (sum_value, &endptr) || *endptr)
    fprintf (stderr, "Invalid sum value.  Must be a positive number\n");
  else
    {
      if (-1 != SQL_Query ("BEGIN")
          && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('add stock')")
          && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %s::INTEGER, %d, %s::NUMERIC, 'NOK', %s::INTEGER)", product_id, user_id, sum_value, stock)
          && -1 != SQL_Query ("COMMIT"))
        {
          fprintf (stderr, "Commited to transaction log\n");
        }
      else
        {
          SQL_Query ("ROLLBACK");
          fprintf (stderr, "SQL Error; Did not commit anything\n");
        }
    }
}

static void
cmd_lastlog (int user_id)
{
  size_t i;

  SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account)", user_id);

  printf ("%19s %-7s %-7s %8s %5s %-20s %-20s\n",
          "Date", "TID", "Amount", "Currency", "Items", "Debit", "Credit");

  for (i = 0; i < SQL_RowCount(); ++i)
    {
      printf ("%19.*s %7s %7s %8s %5s %-20s %-20s\n",
              19, SQL_Value (i, 8), SQL_Value (i, 0), SQL_Value (i, 3), SQL_Value (i, 4), SQL_Value (i, 5), SQL_Value (i, 6), SQL_Value (i, 7));
    }
}

static void
gensalt (char *salt)
{
  FILE *f;
  unsigned int i;

  if (0 == (f = fopen("/dev/urandom", "r")))
    err(EXIT_FAILURE, "Failed to open /dev/urandom for reading");

  strcpy(salt, "$6$");

  fread(salt + 3, 1, 9, f);

  for (i = 0; i < 9; ++i)
    {
      salt[i + 3] = (salt[i + 3] & 0x7f) | 0x40;

      if (!isalpha(salt[i + 3]))
        salt[i + 3] = 'a' + rand() % ('z' - 'a');
    }

  salt[12] = '$';
  salt[13] = 0;

  fclose(f);
}

static void
read_password (char password[256])
{
  disable_echo();

  password[255] = 0;

  if (!fgets(password, 255, stdin))
    {
      enable_echo();

      printf("\n");

      if (errno == 0)
        errx(EXIT_FAILURE, "End of file while reading password");

      err(EXIT_FAILURE, "Error reading password: %s", strerror(errno));
    }

  password[strlen(password) - 1] = 0; /* Remove \n */

  enable_echo();

  printf("\n");
}

static void
cmd_passwd (int user_id, const char *realm)
{
  char password[256];
  char salt[256], *password_hash;
  size_t i, chars, minchars = 5;

  if (strcmp (realm, "login") && strcmp (realm, "door"))
    {
      printf ("Unknown realm\n");

      return;
    }

  printf("Password for realm \"%s\": ", realm);

  read_password (password);

  chars = strlen (password); /* test123 */

  for (i = 1; password[i]; ++i)
    {
      if (password[i] == password[i - 1] + 1)
        --chars;
    }

  if (strstr (password, "hest"))
    chars -= 2; /* st already counted as -1 */

  if (strstr (password, "test"))
    chars -= 2; /* st already counted as -1 */

  if (!strcmp (realm, "login"))
    minchars = 9;

  if (chars < minchars)
    {
      printf ("Password too short (%zu of minimum %zu chars)\n"
              "These patterns are counted as a single char:\n"
              "  * Consecutive ASCII codes (e.g. 123, ABC, xyz)\n"
              "  * \"hest\"\n"
              "  * \"test\"\n",
              chars, minchars);

      return;
    }

  SQL_Query ("BEGIN");

  gensalt (salt);

  password_hash = crypt (password, salt);

  if (0 == SQL_Query ("UPDATE auth SET data = %s WHERE realm = %s AND account = %d", password_hash, realm, user_id))
    {

      SQL_Query ("INSERT INTO auth (account, realm, data) VALUES (%d, %s, %s)", user_id, realm, password_hash);
    }

  SQL_Query ("COMMIT");
}

static void
cmd_ls (void)
{
  size_t i;

  SQL_Query ("SELECT *, (amount / stock)::NUMERIC(10,2) AS unit_price FROM product_stock WHERE stock > 0 ORDER BY name");

  printf (YELLOW_ON "%-5s %-5s %7s %-20s\n" YELLOW_OFF, "ID", "Count", "Price", "Name");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%-5s %-5s %7s %-20s\n", SQL_Value(i, 0), SQL_Value(i, 2), SQL_Value (i, 4), SQL_Value(i, 1));
    }
}

static void
cmd_products (void)
{
  size_t i;

  SQL_Query ("SELECT * FROM product_stock ORDER BY name");

  printf (YELLOW_ON "%-5s %-5s %7s %-20s\n" YELLOW_OFF, "ID", "Count", "Value", "Name");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%-5s %-5s %7s %-20s\n", SQL_Value(i, 0), SQL_Value(i, 2), SQL_Value (i, 3), SQL_Value(i, 1));
    }
}

static void
cmd_retdeposit (int user_id, const char *amount)
{
  char *endptr;

  if (0 >= strtod (amount, &endptr) || *endptr)
    fprintf (stderr, "Invalid amount.  Must be a positive number\n");
  else
    {
      if (-1 != SQL_Query ("BEGIN")
          && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('return deposit')")
          && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %d, (SELECT id FROM accounts WHERE name = 'deposit' LIMIT 1), %s::NUMERIC, 'NOK', 1)", user_id, amount)
          && -1 != SQL_Query ("COMMIT"))
        {
          fprintf (stderr, "Commited to transaction log\n");
        }
      else
        {
          SQL_Query ("ROLLBACK");
          fprintf (stderr, "SQL Error; Did not commit anything\n");
        }
    }
}

static void
cmd_undo (const char *transaction)
{
  long long int undo_transaction;

  if (-1 != SQL_Query ("BEGIN")
      && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('undo ' || %s)", transaction)
      && -1 != (undo_transaction = sql_last_id ())
      && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) SELECT %l, credit_account, debit_account, amount, currency, stock FROM transaction_lines WHERE transaction = %s::INTEGER",
                          undo_transaction, transaction)
      && -1 != SQL_Query ("COMMIT"))
    {
      fprintf (stderr, "Commited to transaction log.\n");
    }
  else
    {
      SQL_Query ("ROLLBACK");
      fprintf (stderr, "SQL Error; Did not commit anything\n");
    }
}

static void
cmd_checkin (const char *user_name, int user_id, int checkin_type)
{
  if (checkin_type == 0)
    {
      if (strcmp (user_name, "deficit"))
        SQL_Query ("INSERT INTO checkins (account, type) VALUES (%d, 'checkout')", user_id);
    }
  else if (checkin_type == 1)
    {
      if (strcmp (user_name, "deficit"))
        SQL_Query ("INSERT INTO checkins (account, type) VALUES (%d, 'checkin')", user_id);
    }

  printf("You're now checked %s.\n", checkin_type == 0 ? "out" : "in");
}

static void
log_in (const char *user_name, int user_id, int register_checkin)
{
  char *command;

  clear_history ();

  printf ("Bam, you're logged in!  (No password authentication for now)\n"
          "Press Ctrl-D to terminate session.  Type \"help\" for help\n"
          "\n");

  if (register_checkin)
    cmd_checkin(user_name, user_id, 1);

#if 0
  SQL_Query ("SELECT COALESCE(type, 'aktiv') FROM active_members WHERE account = %d ORDER BY id DESC", user_id);

  if (SQL_RowCount () && !strcmp (SQL_Value (0, 0), "none"))
    {
      SQL_Query ("SELECT * FROM transactions t INNER JOIN transaction_lines tl ON tl.transaction = t.id WHERE tl.credit_account IN (SELECT id FROM accounts WHERE type = 'p2k12') AND tl.debit_account = %d AND date > NOW() - INTERVAL '24 hour'", user_id);

      if (!SQL_RowCount ())
        {
          char *response;

          printf ("You need to pay 35 NOK to use p2k12 for the next 24 hours\n"
                  "\n");

          response = trim (readline (GREEN_ON "Type \"pay\" (without quotes) to pay> " GREEN_OFF));

          if (!response || strcmp (response, "pay"))
            return;

          if (-1 != SQL_Query ("BEGIN")
              && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('p2k12 day user')")
              && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %d, (SELECT id FROM accounts WHERE name = 'p2k12 day users' LIMIT 1), 35, 'NOK', 1)", user_id)
              && -1 != SQL_Query ("COMMIT"))
            {
              printf ("Commited to transaction log: %s buys 24 hour p2k12 membership\n", user_name);
            }
          else
            {
              SQL_Query ("ROLLBACK");
              printf ("SQL Error; Did not commit anything\n");
            }

        }
    }
#endif

  cmd_ls ();

  SQL_Query ("SELECT price FROM active_members WHERE account = %d", user_id);

  int membership_price;
  membership_price = strtol(SQL_Value(0, 0), 0, 0);

  for (;;)
    {
      char *prompt, *argv0, *endptr;
      stringlist argv;
      int argc;

      SQL_Query ("SELECT -balance FROM user_balances WHERE id = %d", user_id);

      asprintf (&prompt, GREEN_ON "%s (%s)> " GREEN_OFF, user_name, SQL_Value (0, 0));

      alarm (120);

      if (!(command = trim (readline (prompt))))
        break;

      add_history(command);

      alarm (0);

      free (prompt);

      if (-1 == argv_parse (&argv, command))
        {
          free (command);

          continue;
        }

      argc = ARRAY_COUNT (&argv);

      if (!argc)
        {
          ARRAY_FREE (&argv);
          free (command);

          continue;
        }

      argv0 = ARRAY_GET (&argv, 0);

      if (strcmp (argv0, "become") && membership_price < 100 && strcmp(argv0, "help"))
        {
          fprintf(stderr, "p2k12 is a members only system.\nUse the become command to get more privileges.\nThe help command lists commands.\n");

          ARRAY_FREE (&argv);
          free (command);
          continue;
        }

      if (!strcmp (argv0, "give") && argc == 3)
        {
          char *target, *amount;

          target = ARRAY_GET (&argv, 1);
          amount = ARRAY_GET (&argv, 2);

          if (amount[0] == '-')
            {
              fprintf (stderr, "You cannot give away negative amounts\n");
            }
          else if (-1 != SQL_Query ("BEGIN")
                   && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('give')")
                   && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency) VALUES (LASTVAL(), %d, (SELECT id FROM accounts WHERE name = %s), %s::NUMERIC, 'NOK')", user_id, target, amount)
                   && -1 != SQL_Query ("COMMIT"))
            {
              fprintf (stderr, "Commited to transaction log: %s gives %s %s NOK\n", user_name, target, amount);
            }
          else
            {
              SQL_Query ("ROLLBACK");

              fprintf (stderr, "Not ok\n");
            }
        }
      else if (!strcmp (argv0, "take") && argc == 3)
        {
          char *target, *amount;

          target = ARRAY_GET (&argv, 1);
          amount = ARRAY_GET (&argv, 2);

          if (amount[0] == '-')
            {
              fprintf (stderr, "You cannot take negative amounts\n");
            }
          else if (-1 != SQL_Query ("BEGIN")
                   && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('take')")
                   && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency) VALUES (LASTVAL(), (SELECT id FROM accounts WHERE name = %s), %d, %s::NUMERIC, 'NOK')", target, user_id, amount)
                   && -1 != SQL_Query ("COMMIT"))
            {
              fprintf (stderr, "Commited to transaction log: %s takes %s NOK from %s\n", user_name, amount, target);
            }
          else
            {
              SQL_Query ("ROLLBACK");

              fprintf (stderr, "Not ok\n");
            }
        }
      else if (!strcmp (argv0, "become"))
        {
          if (argc == 2)
            cmd_become (user_id, ARRAY_GET (&argv, 1));
          else
            fprintf (stderr, "Usage: %s <PRICE>\n", argv0);
        }
      else if (!strcmp (argv0, "addproduct"))
        {
          if (argc == 2)
            cmd_addproduct (ARRAY_GET (&argv, 1));
          else
            fprintf (stderr, "Usage: %s <NAME>\n", argv0);
        }
      else if (!strcmp (argv0, "addstock"))
        {
          if (argc == 4)
            cmd_addstock (user_id, ARRAY_GET (&argv, 1), ARRAY_GET (&argv, 2), ARRAY_GET (&argv, 3));
          else
            fprintf (stderr, "Usage: %s <PRODUCT-ID> <SUM-VALUE> <STOCK>\n", argv0);
        }
      else if (!strcmp (argv0, "lastlog"))
        {
          if (argc == 1)
            cmd_lastlog (user_id);
          else
            fprintf (stderr, "Usage: %s\n", argv0);
        }
      else if (!strcmp (argv0, "passwd"))
        {
          if (argc == 2)
            cmd_passwd (user_id, ARRAY_GET (&argv, 1));
          else
            fprintf (stderr, "Usage: %s <REALM>\n", argv0);
        }
      else if (!strcmp (argv0, "ls"))
        {
          if (argc == 1)
            cmd_ls ();
          else
            fprintf (stderr, "Usage: %s\n", argv0);
        }
      else if (!strcmp (argv0, "products"))
        {
          if (argc == 1)
            cmd_products ();
          else
            fprintf (stderr, "Usage: %s\n", argv0);
        }
      else if (!strcmp (argv0, "retdeposit"))
        {
          if (argc == 2)
            cmd_retdeposit (user_id, ARRAY_GET (&argv, 1));
          else
            fprintf (stderr, "Usage: %s <AMOUNT>\n", argv0);
        }
      else if (!strcmp (argv0, "undo"))
        {
          if (argc == 2)
            cmd_undo (ARRAY_GET (&argv, 1));
          else
            fprintf (stderr, "Usage: %s <TRANSACTION>\n", argv0);
        }
      else if (!strcmp (argv0, "help"))
        {
          fprintf (stderr,
                   "become PRICE                 switch membership price to PRICE\n"
                   "                                prices: 0, 300, 500, 1000, 1500\n"
                   "checkin                      register arrival to space\n"
                   "checkout                     register departure from space\n"
                   "give USER AMOUNT             give AMOUNT to USER from own account\n"
                   "take USER AMOUNT             take AMOUNT from USER to own account\n"
                   "addproduct NAME              adds PRODUCT to the inventory\n"
                   "addstock PRODUCT-ID SUM-VALUE STOCK\n"
                   "                             adds STOCK items of product with ID PRODUCT-ID\n"
                   "                               and total value SUM-VALUE to stock\n"
                   "lastlog                      list all transactions involving you\n"
                   "passwd REALM                 set password for given realm\n"
                   "                               realms: door, login\n"
                   "products                     list all products and their IDs\n"
                   "retdeposit AMOUNT            return deposit taken from storage to p2k12\n"
                   "undo TRANSACTION             undo a transaction\n"
                   "help                         display this help text\n"
                   "[0-9]+ COUNT                 buy a product\n");
        }
      else if (strtol (argv0, &endptr, 0) && !*endptr)
        {
          int count = 1;

          if (argc > 2)
            fprintf (stderr, "Usage: <PRODUCT-ID> [COUNT]\n");
          else if (-1 == SQL_Query ("SELECT name FROM accounts WHERE (id = %s::INTEGER OR name = %s) AND type = 'product'", argv0, argv0)
                   || !SQL_RowCount ())
            {
              fprintf (stderr, "Bad product ID\n");
            }
          else if (argc == 2
                   && (0 >= (count = strtol (ARRAY_GET (&argv, 1), &endptr, 0))
                       || *endptr))
            {
              fprintf (stderr, "Invalid count '%s'\n", ARRAY_GET (&argv, 1));
            }
          else
            {
              char *product_name;
              long long transaction;

              product_name = strdup (SQL_Value (0, 0));

              if (-1 != SQL_Query ("BEGIN")
                  && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('buy')")
                  && -1 != (transaction = sql_last_id ())
                  && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (%l, %d, %s::INTEGER, (SELECT %d * amount / stock FROM product_stock WHERE id = %s::INTEGER), 'NOK', %d)", transaction, user_id, command, count, command, count)
                  && -1 != SQL_Query ("COMMIT"))
                {
                  fprintf (stderr, "Commited to transaction log: %s buys %d %s.  To undo, type undo %lld\n", user_name, count, product_name, transaction);
                }
              else
                {
                  SQL_Query ("ROLLBACK");
                  fprintf (stderr, "SQL Error; Did not commit anything\n");
                }

              free (product_name);
            }
        }
      else if (!strcmp (argv0, "checkin"))
        {
          if (argc == 1)
            cmd_checkin (user_name, user_id, 1);
          else
            fprintf (stderr, "Usage: %s\n", argv0);
        }
      else if (!strcmp (argv0, "checkout"))
        {
          if (argc == 1)
            cmd_checkin (user_name, user_id, 0);
          else
            fprintf (stderr, "Usage: %s\n", argv0);
        }
      else
        fprintf (stderr, "Unknown command '%s'.  Try 'help'\n", argv0);

      ARRAY_FREE (&argv);
      free (command);
    }
}

void
register_member ()
{
  char *name;
  char *user_name;
  char *email;
  char *price;

  struct termios t;

  tcgetattr(0, &t);

  t.c_lflag = 0xa3b;

  tcsetattr(0, TCSANOW, &t);

  setlocale (LC_CTYPE, "en_US.UTF-8");

  printf ("Press Ctrl-C at any time to discard all input\n"
          "\n");

  user_name = trim (readline (GREEN_ON "Your (desired) user name: " GREEN_OFF));

  if (!user_name || !*user_name)
    exit (EXIT_FAILURE);

  if (-1 == SQL_Query("SELECT id FROM accounts WHERE name = %s", user_name))
    errx (EXIT_FAILURE, "SQL query failed");

  if (SQL_RowCount ())
    {
      log_in (user_name, atoi (SQL_Value (0, 0)), 1);

      return;
    }

  name = trim (readline (GREEN_ON "Your full name (e.g. Ærling Øgilsblå): " GREEN_OFF));

  if (!name || !*name)
    exit (EXIT_FAILURE);


  email = trim (readline (GREEN_ON "Your current e-mail address: " GREEN_OFF));

  if (!email || !*email || !strchr (email, '@') || !strchr (email, '.'))
    exit (EXIT_FAILURE);

  printf ("Membership price\n");
  printf ("     aktiv      500 kr per month\n");
  printf ("  OR filantrop 1000 kr per month\n");
  printf ("  OR støtte     300 kr per month\n");
  printf ("  OR none         0 kr per month\n");
  for (;;)
    {
      price = trim (readline (GREEN_ON "Membership price (default is 500): " GREEN_OFF));

      if (!price)
        exit (EXIT_FAILURE);

      if (!*price)
        {
          free (price);
          price = strdup ("aktiv");
        }
      else
        {
          if (strcmp (price, "500")
              && strcmp (price, "1000")
              && strcmp (price, "300")
              && strcmp (price, "0")
          )
            {
              printf ("Specify either \"500\", \"1000\", \"300\", or \"0\"\n");

              continue;
            }
        }

      break;
    }

  SQL_Query ("BEGIN");
  SQL_Query ("INSERT INTO accounts (name, type) VALUES (%s, 'user')", user_name);

  SQL_Query ("INSERT INTO checkins (account) VALUES (CURRVAL('accounts_id_seq'::REGCLASS))");
  if (-1 == SQL_Query ("INSERT INTO members (full_name, email, price, account) VALUES (%s, %s, %s, CURRVAL('accounts_id_seq'::REGCLASS))", name, email, price))
    {
      SQL_Query ("ROLLBACK");
      printf ("\n"
              "Failed to store member information\n");
    }
  else if (!strcmp (price, "none"))
    {
      SQL_Query ("COMMIT");
      printf ("\n"
              "Okay\n");
    }
  else
    {
      SQL_Query ("COMMIT");
      printf ("\n"
              "Payment information is in the mail\n");
    }

  printf ("\n");
  printf ("Press a key to clear the screen\n");

  disable_icanon ();
  disable_echo ();
  getchar ();
  enable_icanon ();
  enable_echo ();
}

int
main (int argc, char** argv)
{
  uid_t uid;

  setenv ("TZ", "CET", 1);

  SQL_Init ("dbname=p2k12 user=p2k12");
  SQL_Query ("SET TIME ZONE 'CET'");

  enable_icanon ();
  enable_echo ();

  printf ("\033[00m\033[H\033[2J");
  printf ("Welcome to P2K12!\n");
  printf ("\n");

  using_history ();

  uid = getuid();

  if (uid != 0)
    {
      struct passwd *pw;

      if (NULL != (pw = getpwuid(uid)))
        {
          if (-1 == SQL_Query("SELECT id FROM accounts WHERE name = %s", pw->pw_name))
            errx (EXIT_FAILURE, "SQL query failed");

          if (SQL_RowCount ())
            {
              log_in (pw->pw_name, atoi (SQL_Value (0, 0)), 0);

              return EXIT_SUCCESS;
            }
        }
    }

  register_member ();

  return EXIT_SUCCESS;
}
