AC_INIT(p2k12,2012.0)
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CHECK_LIB([pq], [PQconnectdb], [], [AC_MSG_ERROR([libpq required (apt-get install libpq-dev)])])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_OUTPUT(Makefile)
