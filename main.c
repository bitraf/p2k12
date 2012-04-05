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

#include "queue.h"

#define COLOR_ON "\033[32;1m"
#define COLOR_OFF "\033[00m"

char *
suggest_user_name (const char *name)
{
  char result[8];
  unsigned int i = 0, mode = 0;

  while (*name && i < sizeof (result) - 1)
    {
      switch (mode)
        {
        case 0:

          if (strchr ("bcdfghjklmnpqrstvwx", tolower (*name)))
            {
              result[i++] = tolower (*name);
              mode = 1;
            }

          break;

        case 1:

          if (strchr ("aeiouy", tolower (*name)))
            {
              result[i++] = tolower (*name);
              mode = 0;
            }

          break;
        }

      ++name;
    }

  result[i] = 0;

  return strdup (result);
}

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
register_member ()
{
  char *name;
  char *suggested_user_name;
  char *user_name;
  char *email;
  char *type;

  FILE *output;

  struct termios t;

  tcgetattr(0, &t);

  t.c_lflag = 0xa3b;

  tcsetattr(0, TCSANOW, &t);

  setlocale (LC_CTYPE, "en_US.UTF-8");

  printf ("This is the interactive form for registering a new member\n"
          "\n"
          "Press Ctrl-C at any time to discard all input\n"
          "\n");

  name = trim (readline (COLOR_ON "Your full name (e.g. Ærling Øgilsblå): " COLOR_OFF));

  if (!name || !*name)
    exit (EXIT_FAILURE);

  suggested_user_name = suggest_user_name (name);

  if (strlen (suggested_user_name) > 2)
    printf ("Suggested user name: %s\n", suggested_user_name);

  user_name = trim (readline (COLOR_ON "Your desired user name: " COLOR_OFF));

  if (!user_name || !*user_name)
    exit (EXIT_FAILURE);

  email = trim (readline (COLOR_ON "Your current e-mail address: " COLOR_OFF));

  if (!email || !*email)
    exit (EXIT_FAILURE);

  printf ("Membership types\n");
  printf ("  støtte     300 kr/month (occasional/poor member)\n");
  printf ("  aktiv      500 kr/month (regular member)\n");
  printf ("  filantrop 1000 kr/month (well off member)\n");

  for (;;)
    {
      type = trim (readline (COLOR_ON "Membership type (default is aktiv): " COLOR_OFF));

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
              && strcmp (type, "filantrop"))
            {
              printf ("Specify either \"aktiv\", \"støtte\" or \"filantrop\"\n");

              continue;
            }
        }

      break;
    }

  output = new_queue_entry ();

  if (!output)
    err (EXIT_FAILURE, "Failed to create queue entry");

  fprintf (output, "Method: register-new-member\n");
  fprintf (output, "Full-Name: %s\n", name);
  fprintf (output, "User-Name: %s\n", user_name);
  fprintf (output, "Email: %s\n", email);
  fprintf (output, "Type: %s\n", type);

  if (-1 == commit_queue_entry (output))
    err (EXIT_FAILURE, "Failed to commit queue entry");

  printf ("\n");
  printf ("Payment information is in the mail.  You will be a member once the first payment is received\n");
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
  enable_icanon ();
  enable_echo ();

  printf ("\033[00m\033[H\033[2J");
  printf ("Welcome to P2K12!\n");
  printf ("\n");

  register_member ();

  return EXIT_SUCCESS;
}
