/* $Id: malloc.c,v 1.2 2002-05-11 02:33:27 stephens Exp $ */

#include <stdlib.h>
#include "tm.h"


void *malloc (size_t s)
{
  return tm_alloc(s);
}


void *realloc (void *p, size_t s)
{
  return tm_realloc(p, s);
}


void free (void *p)
{
  tm_free(p);
}


void *calloc (size_t s1, size_t s2)
{
  return tm_alloc(s1 * s2);
}

