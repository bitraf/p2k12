#define _GNU_SOURCE

#include <err.h>
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

#include "postgresql.h"
#include "queue.h"

#define GREEN_ON "\033[32;1m"
#define GREEN_OFF "\033[00m"
#define YELLOW_ON "\033[33;1m"
#define YELLOW_OFF "\033[00m"

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

void
log_in (const char *user_name, int user_id)
{
  char *command;
  unsigned int i;

  printf ("Bam, you're logged in!  (No password authentication for now)\n"
          "Press Ctrl-D to terminate session\n"
          "\n");

  SQL_Query ("SELECT * FROM pretty_transaction_lines WHERE %d IN (debit_account, credit_account)", user_id);

  printf ("%-7s %8s %5s %-20s %-20s\n",
          "Amount", "Currency", "Items", "Debit", "Credit");

  for (i = 0; i < SQL_RowCount(); ++i)
    {
      printf ("%7s %8s %5s %-20s %-20s\n",
              SQL_Value (i, 3), SQL_Value (i, 4), SQL_Value (i, 5), SQL_Value (i, 6), SQL_Value (i, 7));
    }

  printf ("\n");

  SQL_Query ("SELECT COALESCE(type, 'aktiv') FROM members WHERE account = %d ORDER BY id DESC", user_id);

  if (SQL_RowCount () && !strcmp (SQL_Value (0, 0), "p2k12"))
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
              && -1 != SQL_Query ("INSERT INTO transactions DEFAULT VALUES")
              && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %d, (SELECT id FROM accounts WHERE type = 'p2k12' LIMIT 1), 35, 'NOK', 1)", user_id)
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

  SQL_Query ("SELECT *, (amount / stock)::NUMERIC(10,2) AS unit_price FROM product_stock WHERE stock > 0");

  printf (YELLOW_ON "%-5s %-5s %7s %-20s\n" YELLOW_OFF, "ID", "Count", "Price", "Name");

  for (i = 0; i < SQL_RowCount (); ++i)
    {
      printf ("%-5s %-5s %7s %-20s\n", SQL_Value(i, 0), SQL_Value(i, 2), SQL_Value (i, 4), SQL_Value(i, 1));
    }

    for (;;)
    {
      char *prompt;
      char *product_name;

      SQL_Query ("SELECT -balance FROM user_balances WHERE id = %d", user_id);

      asprintf (&prompt, GREEN_ON "%s (%s)> " GREEN_OFF, user_name, SQL_Value (0, 0));

      alarm (30);

      if (!(command = trim (readline (prompt))))
        break;

      alarm (0);

      free (prompt);


        {
          char target[256];
          float amount;

          if (2 == sscanf (command, "give %s %f", target, &amount))
            {
              if (-1 != SQL_Query ("BEGIN")
                  && -1 != SQL_Query ("INSERT INTO transactions (reason) VALUES ('give')")
                  && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency) VALUES (LASTVAL(), %d, (SELECT id FROM accounts WHERE name = %s), %f, 'NOK')", user_id, target, amount)
                  && -1 != SQL_Query ("COMMIT"))
                {
                  printf ("Commited to transaction log: %s gives %s %.2f NOK\n", user_name, target, amount);
                }
              else
                {
                  SQL_Query ("ROLLBACK");

                  printf ("Not ok\n");
                }

              continue;
            }
        }

      if (-1 == SQL_Query ("SELECT name FROM accounts WHERE (id = %s::INTEGER OR name = %s) AND type = 'product'", command, command))
        {
          printf ("Bad command or product ID\n");

          continue;
        }

      if (!SQL_RowCount ())
        {
          printf ("Bad command or product ID\n");

          free (command);

          continue;
        }

      product_name = strdup (SQL_Value (0, 0));

      if (-1 != SQL_Query ("BEGIN")
          && -1 != SQL_Query ("INSERT INTO transactions DEFAULT VALUES")
          && -1 != SQL_Query ("INSERT INTO transaction_lines (transaction, debit_account, credit_account, amount, currency, stock) VALUES (LASTVAL(), %d, %s::INTEGER, (SELECT amount / stock FROM product_stock WHERE id = %s::INTEGER), 'NOK', 1)", user_id, command, command)
          && -1 != SQL_Query ("COMMIT"))
        {
          printf ("Commited to transaction log: %s buys 1 %s\n", user_name, product_name);
        }
      else
        {
          SQL_Query ("ROLLBACK");
          printf ("SQL Error: Did not commit anything\n");
        }

      free (product_name);
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
  printf ("  p2k12       35 kr/day   (pay to use p2k12 for 24 hours)\n");
  printf ("  støtte     300 kr/month (occasional/poor member)\n");
  printf ("  aktiv      500 kr/month (regular member)\n");
  printf ("  filantrop 1000 kr/month (well off member)\n");

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
              && strcmp (type, "p2k12")
              && strcmp (type, "filantrop"))
            {
              printf ("Specify either \"aktiv\", \"støtte\", \"filantrop\" or \"p2k12\"\n");

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
  else if (!strcmp (type, "p2k12"))
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
