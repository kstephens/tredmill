# $Id: Makefile,v 1.1 1999-06-09 07:00:50 stephensk Exp $

CFLAGS += -Wall -g -O3

tmtest.exe : tmtest.c tm.o
	$(CC) $(CFLAGS) -o $@ tmtest.c tm.o

test : tmtest.exe tmtest.gdb
	gdb tmtest -x tmtest.gdb

tm.o : tm.h
