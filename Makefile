# $Id: Makefile,v 1.7 2000-01-13 11:18:59 stephensk Exp $

INCLS += ..

H_FILES = \
	tm.h \
	list.h \
	internal.h

C_FILES = \
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

