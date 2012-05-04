#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
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
#include "queue.h"

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

  t.c_lflag |= ICANON;

  tcsetattr(0, TCSANOW, &t);
}

static void
cmd_addproduct (const char *product_name)
{
  SQL_Query ("INSERT INTO accounts (name, type) VALUES (%s, 'product')", product_name);
}

static void
cmd_become (int user_id, const char *type)
{
  if (strcmp (type, "kontor")
      && strcmp (type, "aktiv")
      && strcmp (type, "støtte")
      && strcmp (type, "filantrop"))
    {
      fprintf (stderr, "Unknown membership type\n");
    }
  else
    SQL_Query ("INSERT INTO members (full_name, email, type, account) SELECT full_name, email, %s, account FROM members WHERE account = %d ORDER BY date DESC LIMIT 1", type, user_id);
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
          fprintf (stderr, "SQL Error: Did not commit anything\n");
        }
    }
}

static void
cmd_lastlog (int user_id)
{
  size_t i;

  SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account)", user_id);

  printf ("%-7s %8s %5s %-20s %-20s\n",
          "Amount", "Currency", "Items", "Debit", "Credit");

  for (i = 0; i < SQL_RowCount(); ++i)
    {
      printf ("%7s %8s %5s %-20s %-20s\n",
              SQL_Value (i, 3), SQL_Value (i, 4), SQL_Value (i, 5), SQL_Value (i, 6), SQL_Value (i, 7));
    }
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
          && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %d, (SELECT id FROM accounts WHERE type = 'deposit' LIMIT 1), %s::NUMERIC, 'NOK', 1)", user_id, amount)
          && -1 != SQL_Query ("COMMIT"))
        {
          fprintf (stderr, "Commited to transaction log\n");
        }
      else
        {
          SQL_Query ("ROLLBACK");
          fprintf (stderr, "SQL Error: Did not commit anything\n");
        }
    }
}

static void
log_in (const char *user_name, int user_id)
{
  char *command;

  printf ("Bam, you're logged in!  (No password authentication for now)\n"
          "Press Ctrl-D to terminate session\n"
          "\n");

  SQL_Query ("INSERT INTO checkins (account) VALUES (%d)", user_id);

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
              printf ("SQL Error: Did not commit anything\n");
            }

        }
    }

  cmd_ls ();

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
            fprintf (stderr, "Usage: %s <TYPE>\n", argv0);
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
      else if (!strcmp (argv0, "help"))
        {
          fprintf (stderr,
                   "become TYPE                  switch membership type to TYPE\n"
                   "                                types: støtte, aktiv, filantrop\n"
                   "give USER AMOUNT             give AMOUNT to USER from own account\n"
                   "take USER AMOUNT             take AMOUNT from USER to own account\n"
                   "addproduct NAME              adds PRODUCT to the inventory\n"
                   "addstock PRODUCT-ID SUM-VALUE STOCK\n"
                   "                             adds STOCK items of product with ID PRODUCT-ID\n"
                   "                               and total value SUM-VALUE to stock\n"
                   "lastlog                      list all transactions involving you\n"
                   "products                     list all products and their IDs\n"
                   "retdeposit AMOUNT            return deposit taken from storage to p2k12\n"
                   "help                         display this help text\n"
                   "[0-9]+                       buy a product\n");
        }
      else if (strtol (argv0, &endptr, 0) && !*endptr)
        {
          if (-1 == SQL_Query ("SELECT name FROM accounts WHERE (id = %s::INTEGER OR name = %s) AND type = 'product'", argv0, argv0)
              || !SQL_RowCount ())
            {
              fprintf (stderr, "Bad product ID\n");
            }
          else
            {
              char *product_name;

              product_name = strdup (SQL_Value (0, 0));

              if (-1 != SQL_Query ("BEGIN")
                  && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('buy')")
                  && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %d, %s::INTEGER, (SELECT amount / stock FROM product_stock WHERE id = %s::INTEGER), 'NOK', 1)", user_id, command, command)
                  && -1 != SQL_Query ("COMMIT"))
                {
                  fprintf (stderr, "Commited to transaction log: %s buys 1 %s\n", user_name, product_name);
                }
              else
                {
                  SQL_Query ("ROLLBACK");
                  fprintf (stderr, "SQL Error: Did not commit anything\n");
                }

              free (product_name);
            }
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
  char *type;

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
      log_in (user_name, atoi (SQL_Value (0, 0)));

      return;
    }

  name = trim (readline (GREEN_ON "Your full name (e.g. Ærling Øgilsblå): " GREEN_OFF));

  if (!name || !*name)
    exit (EXIT_FAILURE);

  email = trim (readline (GREEN_ON "Your current e-mail address: " GREEN_OFF));

  if (!email || !*email)
    exit (EXIT_FAILURE);

  printf ("Membership types\n");
  printf ("     støtte     300 kr/month (occasional/poor member)\n");
  printf ("  OR aktiv      500 kr/month (regular member)\n");
  printf ("  OR filantrop 1000 kr/month (well off member)\n");
  printf ("  OR none         0\n");

  for (;;)
    {
      type = trim (readline (GREEN_ON "Membership type (default is aktiv): " GREEN_OFF));

      if (!type)
        exit (EXIT_FAILURE);

      if (!*type)
        {
          free (type);
          type = strdup ("aktiv");
        }
      else
        {
          if (strcmp (type, "aktiv")
              && strcmp (type, "støtte")
              && strcmp (type, "stotte")
              && strcmp (type, "stoette")
              && strcmp (type, "none")
              && strcmp (type, "filantrop"))
            {
              printf ("Specify either \"aktiv\", \"støtte\", \"filantrop\" or \"none\"\n");

              continue;
            }
        }

      break;
    }

  SQL_Query ("BEGIN");
  SQL_Query ("INSERT INTO accounts (name, type) VALUES (%s, 'user')", user_name);

  if (-1 == SQL_Query ("INSERT INTO members (full_name, email, type, account) VALUES (%s, %s, %s, CURRVAL('accounts_id_seq'::REGCLASS))", name, email, type))
    {
      SQL_Query ("ROLLBACK");
      printf ("\n"
              "Failed to store member information\n");
    }
  else if (!strcmp (type, "none"))
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
  SQL_Init("dbname=p2k12 user=p2k12");

  enable_icanon ();
  enable_echo ();

  printf ("\033[00m\033[H\033[2J");
  printf ("Welcome to P2K12!\n");
  printf ("\n");

  register_member ();

  return EXIT_SUCCESS;
}
