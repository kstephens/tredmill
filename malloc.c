/* $Id: malloc.c,v 1.1 2000-01-13 11:38:10 stephensk Exp $ */

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

