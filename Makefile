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
  init.c \
  user.c \
  malloc.c

#CFLAGS += -m64
CFLAGS += -m32

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
doc/html/index.html : Makefile $(C_FILES) $(H_FILES) doc/doxygen.conf doc/*.png
	mkdir -p doc/html
	cp -p doc/*.png doc/html/
	doxygen doc/doxygen.conf 2>&1 | tee doc/doxygen.log

GARBAGE_DIRS += doc/latex

