# $Id: Makefile,v 1.11 2009-08-01 10:47:31 stephens Exp $

MAKS=../maks

OPTIMIZE=YES
#OPTIMIZE=NO

INCLS += ..

H_FILES = \
  tm_data.h \
  debug.h \
  stats.h \
  os.h \
  color.h \
  phase.h \
  type.h \
  tread.h \
  tread_inline.h \
  block.h \
  node.h \
  node_color.h \
  root.h \
  ptr.h \
  barrier.h \
  page.h \
  mark.h \
  tm.h \
  tm_data.h \
  list.h \
  internal.h \
  log.h

C_FILES = \
  tm_data.c \
  debug.c \
  stats.c \
  os.c \
  color.c \
  type.c \
  tread.c \
  block.c \
  root.c \
  barrier.c \
  mark.c \
  tm.c \
  tm_data.c \
  internal.c \
  init.c \
  user.c \
  malloc.c \
  alloc.c \
  log.c

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

TOOL_NAME:=tread_test
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
doc/html/index.html : Makefile $(C_FILES) $(H_FILES) doc/doxygen.conf doc/*.png doc/data_structure.png doc/tredmil_states.png
	mkdir -p doc/html
	cp -p doc/*.png doc/html/
	doxygen doc/doxygen.conf 2>&1 | tee doc/doxygen.log

doc/data_structure.dot : doc/data_structure.rb
	ruby "$<" "$@"

doc/data_structure.png : doc/data_structure.dot
	dot -Tpng:cairo:cairo -o "$@" "$<"

doc/tredmil_states.png : doc/tredmil_states.dot
	dot -Tpng:cairo:cairo -o "$@" "$<"

pub-docs: doc/html
	rsync -ruzv doc/html/ kscom:kurtstephens.com/pub/tredmill/current/doc/html

GARBAGE_DIRS += doc/latex

#################################################################

RUN=gdb --args
RUN=

TESTS = test1 test2 test3 test4 test5 test6 test7 test8 test9
test: all run-tests

run-test : run-tread_test run-tmtest

run-tmtest : mak_gen/Linux/t/tmtest
	export TM_ALLOC_LOG=/tmp/tm_alloc.log ;\
	for t in $(TESTS) ;\
	do \
	  $(RUN) $< $$t ;\
	  gnuplot alloc_log.gp - ;\
	done

run-tread_test : mak_gen/Linux/t/tread_test
	set -e ;\
	for t in 1265080291 ''; \
	do \
	  mkdir -p images ;\
	  rm -f images/*.* ;\
	  $(RUN) $< $$t ;\
	done

debug: all
	gdb mak_gen/Linux/t/tmtest

/tmp/tm_alloc.log : mak_gen/Linux/t/tmtest
	- mak_gen/Linux/t/tmtest
