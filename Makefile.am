bin_PROGRAMS = p2k12

P2K12_MODE ?= dev

AM_CFLAGS = -Wall
AM_CFLAGS += -D P2K12_MODE=$(P2K12_MODE)

p2k12_SOURCES = array.h array.c postgresql.c main.c postgresql.h
p2k12_LDADD = -lreadline -lpq -lcrypt

install-exec-hook:
	chown root "$(DESTDIR)$(bindir)/p2k12"
	chmod u+s "$(DESTDIR)$(bindir)/p2k12"
