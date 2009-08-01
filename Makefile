# $Id: Makefile,v 1.11 2009-08-01 10:47:31 stephens Exp $

MAKS=../maks

OPTIMIZE=YES
#OPTIMIZE=NO

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

#################################################################

# Make this part of maks.
doc/html : doc/html/index.html
doc/html/index.html : $(C_FILES) $(H_FILES) doc/doxygen.conf
	doxygen doc/doxygen.conf

GARBAGE_DIRS += doc/latex

