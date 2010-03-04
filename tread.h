#ifndef _tredmill_TREAD_H
#define _tredmill_TREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "list.h"
#include "node.h"
#include "color.h"

/**
 * The tread for a type.
 *
 * <pre>
 * </pre>
 */
typedef
struct tm_tread {
  /**
   * The next free node.
   * prev is BLACK.
   * next is NON-BLACK.
   */
  tm_node *free;

  /**
   * The last free node.
   * When free == bottom,
   * there are no free nodes to allocated.
   */
  tm_node *bottom;

  /**
   *
   */
  tm_node *top;

  /**
   * The next node to scan for interior pointers.
   * prev is BLACK.
   * next is GREY.
   */
  tm_node *scan;

  /**
   * Current count of nodes by color.
   * Includes tm_TOTAL, tm_B, tm_NU, tm_b, tm_b_NU stats.
   */
  size_t n[tm__LAST2];
} tm_tread;


void tm_tread_validate(tm_tread *t);
#define tm_tread_VALIDATE(t) (void)(t)
#ifndef tm_tread_VALIDATE
#define tm_tread_VALIDATE(t) tm_tread_validate(t)
#endif

void tm_tread_render_dot(FILE *fp, tm_tread *t, const char *desc, int markn, tm_node **marks);

int tm_tread_more_white (tm_tread *t);
void tm_tread_mark_roots(tm_tread *t);

/**
 * Initialize a tread.
 */
static __inline
void tm_tread_init(tm_tread *t)
{
  t->free = t->bottom = t->top = t->scan = 0;
  memset(t->n, 0, sizeof(t->n));
  tm_tread_VALIDATE(t);
}

#endif
