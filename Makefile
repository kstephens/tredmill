# $Id: Makefile,v 1.2 1999-06-09 23:39:59 stephensk Exp $

OPT=-O3
OPT=
CFLAGS += -Wall -g $(OPT)

OFILES = tm.o user.o

tmtest.exe : tmtest.c $(OFILES) tm.h list.h Makefile
	$(CC) $(CFLAGS) -o $@ tmtest.c $(OFILES)

test : tmtest.exe tmtest.gdb
	gdb tmtest -x tmtest.gdb

$(OFILES) : Makefile tm.h list.h internal.h


