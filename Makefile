# $Id: Makefile,v 1.3 1999-06-16 08:35:05 stephensk Exp $

INCLS += ..

H_FILES = \
	tm.h \
	list.h \
	internal.h

C_FILES = \
	tm.c \
	user.c

#################################################################

include $(MAKS)/pre.mak

#################################################################
# Create

LIB_NAME:=tredmill
include $(MAKS)/lib.mak

TOOL_NAME:=tmtest
TOOL_LIBS:=tredmill
include $(MAKS)/tool.mak

#################################################################
# Basic
include $(MAKS)/basic.mak

#################################################################

