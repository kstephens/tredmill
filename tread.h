#ifndef _tredmill_TREAD_H
#define _tredmill_TREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "list.h"
#include "node.h"
#include "color.h"
#include <assert.h>


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
   * Include total node count at n[tm_TOTAL].
   */
  size_t  n[tm__LAST];

  /**
   * Current color mapping.
   */
  tm_colors *c;
} tm_tread;


static __inline
void tm_tread_flip(tm_tread *t);
static __inline
void tm_tread_scan(tm_tread *t);
int tm_tread_more_white(tm_tread *t);
void tm_tread_mark_roots(tm_tread *t);

/**
 * Initialize a tread.
 */
static __inline
void tm_tread_init(tm_tread *t)
{
  t->free = t->bottom = t->top = t->scan = 0;
}


#define WHITE t->c->c[tm_WHITE]
#define ECRU  t->c->c[tm_ECRU]
#define GREY  t->c->c[tm_GREY]
#define BLACK t->c->c[tm_BLACK]


static __inline
void tm_tread_add_white(tm_tread *t, tm_node *n)
{
  if ( ! t->n[tm_TOTAL] ) {
    tm_list_init(n);
    t->free = t->bottom = t->top = t->scan = n;
  }
  else {
    tm_list_append(t->bottom, n);
    if ( ! t->n[WHITE] ) {
      t->free = n;
    }
  }
  tm_list_set_color(n, WHITE);
  ++ t->n[WHITE];
  ++ t->n[tm_TOTAL];


#if 0
  if ( t->n[WHITE] == 2 ) {
    assert(tm_node_next(t->free) == t->bottom);
    assert(tm_node_prev(t->free) == t->bottom);
    assert(tm_node_next(t->bottom) == t->free);
    assert(tm_node_prev(t->bottom) == t->free);
  }
#endif
}


static __inline
tm_node *tm_tread_allocate(tm_tread *t)
{
  tm_node *n;

  tm_tread_scan(t);

  if ( ! t->n[WHITE] && ! t->n[GREY] ) {
    tm_tread_flip(t);
  }
  
  if ( ! t->n[WHITE] ) {
    tm_tread_more_white(t);
  }

  if ( ! t->n[WHITE] ) {
    return 0;
  }

  n = t->free;
  t->free = tm_list_next(t->free);

  tm_list_set_color(n, BLACK);

  -- t->n[WHITE];
  ++ t->n[BLACK];

  return n;
}


static __inline
void tm_tread_mark(tm_tread *t, tm_node *n)
{
  assert(tm_node_color(n) != WHITE);
  if ( tm_node_color(n) != ECRU ) {
    if ( t->top == n ) {
      t->top = tm_node_prev(n);
    } else {
      tm_list_remove(n);
      tm_list_insert(t->top, n);
    }

    tm_list_set_color(n, GREY);

    if ( ! t->n[GREY] ) {
      t->scan = n;
    }

    -- t->n[ECRU];
    ++ t->n[GREY];
  }
}


void tm_node_scan(tm_node *n);

static __inline
void tm_tread_scan(tm_tread *t)
{
  tm_node *n = t->scan;
  if ( t->scan != t->top ) {
    t->scan = tm_node_prev(t->scan);
    tm_list_set_color(n, BLACK);
    -- t->n[GREY];
    ++ t->n[BLACK];
  }
}



static __inline
void tm_tread_mutation(tm_tread *t, tm_node *n)
{
  if ( tm_node_color(n) == BLACK ) {
    if ( t->top == n ) {
      t->top = tm_node_prev(n);
    } else {
      tm_list_remove(n);
      tm_list_insert(t->top, n);
    }

    tm_list_set_color(n, GREY);

    if ( ! t->n[GREY] ) {
      t->scan = n;
    }

    -- t->n[BLACK];
    ++ t->n[GREY];
  }
}


/**
 * Flip.
 *
 * "when the free pointer meets the bottom pointer, we must "flip". At this point, we have cells of only two colors--black and ecru. To flip, we make ecru into white and black into ecru; bottom and top are then exchanged. The root pointers are now "scanned" by making them grey; the cells they point to are unlinked from the ecru region and linked into the grey region.(between scan and top). We can restart the collector, as it now has grey cells to scan."
 */
static __inline
void tm_tread_flip(tm_tread *t)
{
#if 0
  fprintf(stderr, "   before flip\n\t n  %d %d %d %d %d\n\tc  %d %d %d %d\n\tc1 %d %d %d %d\n",
	  t->n[0], t->n[1], t->n[2], t->n[3], t->n[4],
	  t->c->c[0], t->c->c[1], t->c->c[2], t->c->c[3],
	  t->c->c1[0], t->c->c1[1], t->c->c1[2], t->c->c1[3]);
#endif

#ifdef tm_tread_UNIT_TEST
  tm_colors_flip(t->c);
#endif

  /* Swap bottom and top. */
  {
    void *p;
    
    p = t->bottom;
    t->bottom = t->top;
    t->top = p;
  }

#if 0
  fprintf(stderr, "   flipped\n\t n  %d %d %d %d %d\n\tc  %d %d %d %d\n\tc1 %d %d %d %d\n",
	  t->n[0], t->n[1], t->n[2], t->n[3], t->n[4],
	  t->c->c[0], t->c->c[1], t->c->c[2], t->c->c[3],
	  t->c->c1[0], t->c->c1[1], t->c->c1[2], t->c->c1[3]);
#endif 

  /* Force marking to occur before WHITE. */
  if ( tm_node_color(t->top) == WHITE ) {
    t->top = tm_node_prev(t->top);
  }

  /* Start scanning at top. */
  t->scan = t->top;

  /* Mark roots. */
  tm_tread_mark_roots(t);

  /* Do not allocate into WHITE. */
  if ( tm_node_color(t->bottom) == WHITE ) {
    t->bottom = tm_node_next(t->bottom);
  }

  /* If there was no WHITE, assume more_white(). */
  if ( ! t->n[WHITE] ) {
    t->bottom = t->free = tm_node_next(t->scan);
  }

}


void tm_tread_render_dot(FILE *fp, tm_tread *t, const char *desc, int markn, tm_node **marks);

#endif
