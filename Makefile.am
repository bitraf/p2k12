bin_PROGRAMS = p2k12

AM_CFLAGS = -Wall

p2k12_SOURCES = array.h array.c postgresql.c main.c postgresql.h
p2k12_LDADD = -lreadline -lpq -lcrypt

install-exec-hook:
	chown root "$(bindir)/p2k12"
	chmod u+s "$(bindir)/p2k12"
