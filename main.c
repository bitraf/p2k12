#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define CLEAR_SCREEN "\033[00m\033[H\033[2J"

typedef ARRAY (char *) stringlist;

#ifdef P2K12_MODE_LIVE
const int allow_user_creation = 0;
const int persistent_history = 0;
#else
static int allow_user_creation = 1;
static int persistent_history = 1;
#endif

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
  char ch, escape_char = 0;

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

  tcgetattr (0, &t);

  t.c_lflag &= ~ECHO;

  tcsetattr (0, TCSANOW, &t);
}

static void
enable_icanon (void)
{
  struct termios t;

  tcgetattr (0, &t);

  t.c_lflag |= ICANON;

  tcsetattr (0, TCSANOW, &t);
}

static void
enable_echo (void)
{
  struct termios t;

  tcgetattr (0, &t);

  t.c_lflag |= ECHO;

  tcsetattr (0, TCSANOW, &t);
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
  int i;

  SQL_Query ("INSERT INTO accounts (name, type) VALUES (%s, 'product') RETURNING id", product_name);

  if (1 != SQL_RowCount ())
    {
      return;
    }

  SQL_Query ("SELECT * FROM product_stock WHERE id = %s", SQL_Value (0, 0));

  printf (YELLOW_ON "%-5s %-5s %7s %-20s\n" YELLOW_OFF, "ID", "Count", "Value", "Name");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%-5s %-5s %7s %-20s\n", SQL_Value (i, 0), SQL_Value (i, 2), SQL_Value (i, 3), SQL_Value (i, 1));
    }
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

  SQL_Query ("SELECT p2k12_become_member(%d, %s)", user_id, price);
  printf ("Your membership has been changed to type: %s\n", price);
}

static void
cmd_officeuser (int user_id)
{
  SQL_Query ("INSERT INTO members (full_name, email, price, account, flag) SELECT full_name, email, price, account, %s  FROM members WHERE account = %d ORDER BY date DESC LIMIT 1", "m_office", user_id);
  printf ("m_office flag set\n");
}

static void
cmd_dns_usage()
{
  fprintf (stderr, "Usage: dns add <FQDN> [4 6] <IP>\n");
  fprintf (stderr, "Usage: dns add <FQDN> C <CNAME>\n");
  fprintf (stderr, "Usage: dns rm <FQDN>\n");
  fprintf (stderr, "Usage: dns list\n");
}

static void
cmd_dns (int user_id, size_t argc, stringlist argv)
{
  const char *cmd;

  if (argc < 2)
    {
      cmd_dns_usage();
      return;
    }

  cmd = ARRAY_GET (&argv, 1);
  if (!strcmp("add", cmd) && argc == 5)
    {
      const char *fqdn = ARRAY_GET (&argv, 2);
      const char *af = ARRAY_GET (&argv, 3);
      const char *arg4 = ARRAY_GET (&argv, 4);
      const char *ip4, *ip6, *cname;

      ip4 = NULL;
      ip6 = NULL;
      cname = NULL;

      if (!strcmp("4", af))
        {
          ip4 = arg4;
        }
      else if (!strcmp("6", af))
        {
          ip6 = arg4;
        }
      else if (!strcmp("C", af))
        {
          cname = arg4;
        }
      else
        {
          cmd_dns_usage();
          return;
        }

      if (-1 != SQL_Query("BEGIN")
          && -1 != SQL_Query("INSERT INTO dns_entries(account, fqdn, ip4, ip6, cname) VALUES(%d, %s::TEXT, %s::CIDR, %s::CIDR, %s) RETURNING id", user_id, fqdn, ip4, ip6, cname)
          && -1 != SQL_Query("COMMIT"))
        {
          fprintf (stderr, "Entry added. Now talk to ops about reloading the DNS server.\n");
        }
      else
        {
          SQL_Query ("ROLLBACK");
        }
    }
  else if (!strcmp("rm", cmd) && argc == 3)
    {
      const char *fqdn = ARRAY_GET (&argv, 2);

      if (-1 != SQL_Query("BEGIN")
          && -1 != SQL_Query("DELETE FROM dns_entries WHERE account=%d AND fqdn=%s", user_id, fqdn)
          && -1 != SQL_Query("COMMIT"))
        {
          fprintf (stderr, "Entry removed. Now talk to ops about reloading the DNS server.\n");
        }
      else
        {
          SQL_Query ("ROLLBACK");
        }
    }
  else if (!strcmp("list", cmd) && argc == 2)
    {
      if (SQL_Query("SELECT host, zone, account_name, ip4, ip6, cname FROM pretty_dns_entries ORDER BY zone, host"))
        {
          int rowCount = SQL_RowCount ();

          printf ("%-10s %-15s %-15s %s\n",
                  "Host", "Zone", "Owner", "Value");
          int i;
          for (i = 0; i < rowCount; ++i)
            {
              const char *ip4 = SQL_Value (i, 3);
              const char *ip6 = SQL_Value (i, 4);
              const char *cname = SQL_Value (i, 5);

              printf ("%-10s %-15s %-15s ",
                      SQL_Value (i, 0), SQL_Value (i, 1), SQL_Value (i, 2));
              if (*ip4)
                {
                  printf ("%s ", ip4);
                }
              if (*ip6)
                {
                  printf ("%s ", ip6);
                }
              if (*cname)
                {
                  printf ("%s ", cname);
                }
              printf ("\n");
            }
        }
    }
  else
    {
      cmd_dns_usage();
    }
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
cmd_lastlog (int user_id, const char *variant)
{

  if (variant != NULL)
    {
      if (!strcmp (variant, "d") || !strcmp (variant, "day"))
        {
          SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account) AND date > CURRENT_TIMESTAMP - INTERVAL '1 day'", user_id);
        }
      else if (!strcmp (variant, "w") || !strcmp (variant, "week"))
        {
          SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account) AND date > CURRENT_TIMESTAMP - INTERVAL '7 days'", user_id);
        }
      else if (!strcmp (variant, "y") || !strcmp (variant, "year"))
        {
          SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account) AND EXTRACT(YEAR FROM date)=EXTRACT(YEAR FROM NOW())", user_id);
        }
      else
        {
          fprintf (stderr, "Usage: lastlog [day, week, year]\n");
          return;
        }
    }
  else
    {
      SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account)", user_id);
    }

  int rowCount = SQL_RowCount ();
  if (rowCount > 0)
    {
      printf ("%19s %-7s %-7s %8s %5s %-20s %-20s\n",
              "Date", "TID", "Amount", "Currency", "Items", "Debit", "Credit");
      int i;
      for (i = 0; i < rowCount; ++i)
        {
          printf ("%19.*s %7s %7s %8s %5s %-20s %-20s\n",
                  19, SQL_Value (i, 8), SQL_Value (i, 0), SQL_Value (i, 3), SQL_Value (i, 4), SQL_Value (i, 5), SQL_Value (i, 6), SQL_Value (i, 7));
        }
    }
  else
    printf ("No transactions found.\n");
}

static void
cmd_checkins (int user_id)
{
  int i;

  SQL_Query ("SELECT date, type FROM checkins WHERE account=%d", user_id);

  printf ("%-19s %-7s\n",
          "Date", "Type");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%19.*s %7s\n", 19, SQL_Value (i, 0), SQL_Value (i, 1));
    }
}

static void
gensalt (char *salt)
{
  FILE *f;
  unsigned int i;

  if (0 == (f = fopen ("/dev/urandom", "r")))
    err (EXIT_FAILURE, "Failed to open /dev/urandom for reading");

  strcpy (salt, "$6$");

  fread (salt + 3, 1, 9, f);

  for (i = 0; i < 9; ++i)
    {
      salt[i + 3] = (char) ((salt[i + 3] & 0x7f) | 0x40);

      if (!isalpha (salt[i + 3]))
        salt[i + 3] = (char) ('a' + rand () % ('z' - 'a'));
    }

  salt[12] = '$';
  salt[13] = 0;

  fclose (f);
}

static void
read_password (char password[256])
{
  disable_echo ();

  password[255] = 0;

  if (!fgets (password, 255, stdin))
    {
      enable_echo ();

      printf ("\n");

      if (errno == 0)
        errx (EXIT_FAILURE, "End of file while reading password");

      err (EXIT_FAILURE, "Error reading password: %s", strerror (errno));
    }

  password[strlen (password) - 1] = 0; /* Remove \n */

  enable_echo ();

  printf ("\n");
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

  printf ("Password for realm \"%s\": ", realm);

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
  int i;

  SQL_Query ("SELECT *, (amount / stock)::NUMERIC(10,2) AS unit_price FROM product_stock WHERE stock > 0 ORDER BY name");

  printf (YELLOW_ON "%-5s %-5s %7s %-20s\n" YELLOW_OFF, "ID", "Count", "Price", "Name");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%-5s %-5s %7s %-20s\n", SQL_Value (i, 0), SQL_Value (i, 2), SQL_Value (i, 4), SQL_Value (i, 1));
    }
}

static void
cmd_products (const char *pattern)
{
  int i;

  SQL_Query ("SELECT * FROM product_stock WHERE name ILIKE '%%' || %s || '%%' ORDER BY id", pattern);

  printf (YELLOW_ON "%-5s %-5s %7s %-20s\n" YELLOW_OFF, "ID", "Count", "Value", "Name");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%-5s %-5s %7s %-20s\n", SQL_Value (i, 0), SQL_Value (i, 2), SQL_Value (i, 3), SQL_Value (i, 1));
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

  printf ("You're now checked %s.\n", checkin_type == 0 ? "out" : "in");
}

static void
log_in (const char *user_name, int user_id, int register_checkin)
{
  char *command;

  if (persistent_history)
    {
      read_history (".p2k12_history");
    }
  else
    {
      clear_history ();
    }

  SQL_SetP2k12Account (user_name);

  printf ("Bam, you're logged in!  (No password authentication for now)\n"
          "Press Ctrl-D to terminate session.  Type \"help\" for help\n"
          "\n");

  if (register_checkin)
    cmd_checkin (user_name, user_id, 1);

  cmd_ls ();

  for (; ;)
    {
      char *prompt, *argv0, *endptr;
      stringlist argv;
      size_t argc;

      SQL_Query ("SELECT -balance FROM user_balances WHERE id = %d", user_id);

      asprintf (&prompt, GREEN_ON "%s (%s)> " GREEN_OFF, user_name, SQL_Value (0, 0));

      alarm (120);

      if (!(command = trim (readline (prompt))))
        break;

      add_history(command);

      if (persistent_history)
        {
          write_history (".p2k12_history");
        }

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

      if (strcmp (user_name, "deficit") != 0 && strcmp (user_name, "deposit") != 0)
        {
          SQL_Query ("SELECT price, flag FROM active_members WHERE account = %d", user_id);

          int membership_price;

          const char *flag;

          if (SQL_RowCount() > 0)
            {
              membership_price = (int) strtol(SQL_Value(0, 0), 0, 0);
              flag = SQL_Value(0, 1);
            }
          else
            {
              membership_price = 0;
              flag = "";
            }

          if (strcmp (argv0, "become") != 0 && membership_price < 100 && strcmp (argv0, "help") != 0 && strcmp (flag, "m_office") != 0
              && strcmp (argv0, "officeuser") != 0 && strcmp (argv0, "lastlog") != 0)
            {
              fprintf (stderr, "p2k12 is a members only system.\nUse the become command to get more privileges.\nThe help command lists public commands.\n");

              ARRAY_FREE (&argv);
              free (command);
              continue;
            }
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
      else if (!strcmp (argv0, "dns"))
        {
          cmd_dns (user_id, argc, argv);
        }
      else if (!strcmp (argv0, "officeuser"))
        {
          cmd_officeuser (user_id);
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
          if (argc == 2)
            cmd_lastlog (user_id, ARRAY_GET(&argv, 1));
          else if (argc == 1)
            cmd_lastlog (user_id, 0);
          else
            fprintf (stderr, "Usage: %s [day, week, year]\n", argv0);
        }
      else if (!strcmp (argv0, "checkins"))
        {
          if (argc == 1)
            cmd_checkins (user_id);
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
          if (argc == 2)
            cmd_products (ARRAY_GET(&argv, 1));
          else if (argc == 1)
            cmd_products ("");
          else
            fprintf (stderr, "Usage: %s [PATTERN]\n", argv0);
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
                   "checkins                     list all your registered checkins\n"
                   "checkout                     register departure from space\n"
                   "give USER AMOUNT             give AMOUNT to USER from own account\n"
                   "take USER AMOUNT             take AMOUNT from USER to own account\n"
                   "addproduct NAME              adds PRODUCT to the inventory\n"
                   "addstock PRODUCT-ID SUM-VALUE STOCK\n"
                   "                             adds STOCK items of product with ID PRODUCT-ID\n"
                   "                               and total value SUM-VALUE to stock\n"
                   "lastlog [day, week, year]    list all transactions involving you\n"
                   "passwd REALM                 set password for given realm\n"
                   "                               realms: door, login\n"
                   "products [PATTERN]           list all products and their IDs\n"
                   "                             or if supplied, only those that match PATTERN\n"
                   "retdeposit AMOUNT            return deposit taken from storage to p2k12\n"
                   "undo TRANSACTION             undo a transaction\n"
                   "help                         display this help text\n"
                   "[0-9]+ COUNT                 buy a product\n"
                   "\n\nUse SHIFT+[PAGE_UP, PAGE_DOWN] too see previous commands or output\n");
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
                   && (0 >= (count = (int) strtol (ARRAY_GET (&argv, 1), &endptr, 0))
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
read_price(char *price)
{
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
          price = "500";
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
}

void
create_user (const char *user_name)
{
  char *name, *email;

  name = trim (readline (GREEN_ON "Your full name (e.g. Ærling Øgilsblå): " GREEN_OFF));

  if (!name || !*name)
    exit (EXIT_FAILURE);

  email = trim (readline (GREEN_ON "Your current e-mail address: " GREEN_OFF));

  if (!email || !*email || !strchr (email, '@') || !strchr (email, '.'))
    exit (EXIT_FAILURE);

  SQL_Query ("BEGIN");
  if (-1 == SQL_Query ("SELECT p2k12_create_member (%s, %s, %s)", user_name, name, email))
    {
      SQL_Query ("ROLLBACK");
      printf ("\n"
                  "Failed to store member information\n");
    }
  else
    {
      SQL_Query ("COMMIT");
      printf ("\nCongratulations you are now member of Oslo's biggest hackerspace.\n");
    }

  printf ("\n");
  printf ("Press a key to clear the screen\n");

  disable_icanon ();
  disable_echo ();
  getchar ();
  enable_icanon ();
  enable_echo ();
  printf (CLEAR_SCREEN);
}

void
register_member ()
{
  for (;;)
    {
      char *user_name;

      struct termios t;

      tcgetattr (0, &t);

      t.c_lflag = 0xa3b;

      tcsetattr (0, TCSANOW, &t);

      setlocale (LC_CTYPE, "en_US.UTF-8");

      printf ("Go to https://bitraf.no/join to register a new member\n"
                  "\n");

      user_name = trim (readline (GREEN_ON "Your user name: " GREEN_OFF));

      if (!user_name || !*user_name)
        exit (EXIT_FAILURE);

      if (-1 == SQL_Query ("SELECT id, name FROM accounts WHERE LOWER(name) = LOWER(%s)", user_name))
        errx (EXIT_FAILURE, "SQL query failed");

      if (SQL_RowCount ())
        {
          user_name = strdup (SQL_Value (0, 1));

          log_in (user_name, atoi (SQL_Value (0, 0)), 1);

          return;
        }

      if (allow_user_creation)
        {
          create_user (user_name);
        }
      else
        {
          printf ("Username not recognized.\n\n");
        }
    }
}

int
main (int argc, char **argv)
{
  // TODO(mastensg): authenticate with p2k16
  printf (CLEAR_SCREEN);
  printf ("New membership system!\n");
  printf (" New membership system!\n");
  printf ("  New membership system!\n");
  printf ("   New membership system!\n");
  printf ("    New membership system!\n");
  printf ("\n");
  printf ("Old members:\n");
  printf ("\n");
  printf ("Log in to https://p2k16.bitraf.no with your username and door password.\n");
  printf ("\n");
  printf ("New members:\n");
  printf ("\n");
  printf ("1. Sign up at https://p2k16.bitraf.no.\n");
  printf ("2. Speak with a door access administrator to gain door access.\n");
  printf ("\n");
  printf ("Door access:\n");
  printf ("\n");
  printf ("For help with unlocking the door, ask one of the door access administrators:\n");
  printf ("\n");
  printf ("- eliasbakken\n");
  printf ("- haavares\n");
  printf ("- jensa\n");
  printf ("- jonnor\n");
  printf ("- mastensg\n");
  printf ("- thomas\n");
  printf ("- trygvis\n");
  printf ("\n");
  printf ("P2K12:\n");
  printf ("\n");
  printf ("The refrigerator is temporarily closed for business. ");

  disable_echo ();

  for (;;)
    {
      getchar ();
    }

#if 0
  uid_t uid;

  (void) argc;
  (void) argv;

  setenv ("TZ", "CET", 1);

  // The certificate from bomba.bitraf.no needs to exist in
  // $HOME/.postgresql/root.crt.
  //
  // The password should be listed in $HOME/.pgpass.
#ifdef P2K12_MODE_LIVE
  SQL_Init ("user=p2k12_pos dbname=p2k12 host=bomba.bitraf.no sslmode=verify-full");
#else
  SQL_Init ("user=p2k12_pos dbname=p2k12 host=localhost");
#endif

  SQL_Query ("SET TIME ZONE 'CET'");

  enable_icanon ();
  enable_echo ();

  printf (CLEAR_SCREEN);
  printf ("Welcome to P2K12!\n");
  printf ("\n");

  using_history ();

  uid = getuid ();

  if (uid != 0)
    {
      struct passwd *pw;

      if (NULL != (pw = getpwuid (uid)))
        {
          if (-1 == SQL_Query ("SELECT id FROM accounts WHERE name = %s", pw->pw_name))
            errx (EXIT_FAILURE, "SQL query failed");

          if (SQL_RowCount ())
            {
              log_in (pw->pw_name, atoi (SQL_Value (0, 0)), 0);

              return EXIT_SUCCESS;
            }
        }
    }

  register_member ();
#endif
  return EXIT_SUCCESS;
}
