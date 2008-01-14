# $Id: Makefile,v 1.8 2008-01-14 00:08:02 stephens Exp $

MAKS=../maks

INCLS += ..

H_FILES = \
  debug.h \
  stats.h \
  os.h \
  root.h \
  ptr.h \
  barrier.h \
  page.h \
  mark.h \
  tm.h \
  list.h \
  internal.h

C_FILES = \
  debug.c \
  stats.c \
  os.c \
  root.c \
  barrier.c \
  mark.c \
  tm.c \
  user.c \
  malloc.c

#################################################################
# Pre
include $(MAKS)/pre.mak

#################################################################
# Create

LIB_NAME:=tredmill
include $(MAKS)/lib.mak

TOOL_NAME:=tmtest
TOOL_LIBS:=tredmill
TOOL_TEST:=YES
include $(MAKS)/tool.mak

TOOL_NAME:=wb
#TOOL_LIBS:=tredmill
TOOL_TEST:=YES
include $(MAKS)/tool.mak

#################################################################
# Basic
include $(MAKS)/basic.mak

#################################################################

$(O_FILES) : $(H_FILES)

