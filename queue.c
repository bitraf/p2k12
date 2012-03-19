#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

FILE *
new_queue_entry (void)
{
  char path[256];
  int fd;

  sprintf (path, "/var/lib/bitraf-queue/new-entry.XXXXXX");

  if (-1 == (fd = mkstemp (path)))
    return 0;

  return fdopen (fd, "r+");
}

int
commit_queue_entry (FILE *entry)
{
  char new_path[256], old_path[256];
  int fd, len;

  if (-1 == (fd = fileno (entry)))
    return -1;

  if (-1 == fsync (fd))
    return -1;

  sprintf (old_path, "/proc/self/fd/%d", fd);

  if (-1 == (len = readlink (old_path, old_path, sizeof (old_path))))
    return -1;

  old_path[len] = 0;

  for (;;)
    {
      struct timeval now;

      gettimeofday (&now, 0);

      sprintf (new_path, "/var/lib/bitraf-queue/entry.%lld.%06u.XXXXXX", (long long) now.tv_sec, (unsigned int) now.tv_usec);

      mktemp (new_path);

      if (-1 == link (old_path, new_path))
        {
          if (errno == EEXIST)
            continue;

          return -1;
        }

      break;
    }

  unlink (old_path);

  fclose (entry);

  return 0;
}
