# $Id: Makefile,v 1.4 1999-06-28 13:58:20 stephensk Exp $

INCLS += ..

H_FILES = \
	tm.h \
	list.h \
	internal.h

C_FILES = \
	tm.c \
	user.c

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

#################################################################
# Basic
include $(MAKS)/basic.mak

#################################################################

