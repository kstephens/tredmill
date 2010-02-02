#ifndef _tm_TREAD_INLINE_H
#define _tm_TREAD_INLINE_H


#include "tredmill/tm_data.h"
#include "tredmill/ptr.h"


static __inline
void tm_tread_flip(tm_tread *t);
static __inline
void tm_tread_scan(tm_tread *t);

static __inline
void tm_tread_add_white(tm_tread *t, tm_node *n)
{
  tm_block *b = tm_node_to_block(n);

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

  ++ b->n[WHITE];
  ++ t->n[WHITE];
  ++ tm.n[WHITE];

  ++ b->n[tm_TOTAL];
  ++ t->n[tm_TOTAL];
  ++ tm.n[tm_TOTAL];

  tm_tread_VALIDATE(t);
}


static __inline
tm_node *tm_tread_alloc_node_from_free_list(tm_tread *t)
{
  tm_node *n = t->free;
  tm_block *b = tm_node_to_block(n);

  assert(tm_node_color(n) == WHITE);

  t->free = tm_list_next(t->free);

  tm_list_set_color(n, BLACK);

  -- b->n[WHITE];
  -- t->n[WHITE];
  -- tm.n[WHITE];

  ++ b->n[BLACK];
  ++ t->n[BLACK];
  ++ tm.n[BLACK];

  tm_tread_VALIDATE(t);

  return n;
}


#ifdef tm_tread_UNIT_TEST

static __inline
tm_node *tm_tread_allocate(tm_tread *t)
{
  tm_tread_scan(t);

  /* "Flip" if there are no WHITE and no GREY */
  if ( ! t->n[WHITE] && ! t->n[GREY] ) {
    tm_tread_flip(t);
  }
  
  if ( ! t->n[WHITE] ) {
    tm_tread_more_white(t);
  }

  if ( ! t->n[WHITE] ) {
    return 0;
  }

  return tm_tread_alloc_node_from_free_list(t);
}

#endif


static __inline
void tm_tread_mark_grey(tm_tread *t, tm_node *n)
{
  tm_block *b = tm_node_to_block(n);

  if ( t->top == n ) {
    t->top = tm_node_prev(n);
  } else {
    tm_list_remove(n);
    tm_list_insert(t->top, n);
  }
  
  tm_list_set_color(n, GREY);
  
  if ( ! t->n[GREY] ) {
    t->scan = n;
    //    t->top = tm_node_next(t->scan);
  }

  ++ b->n[GREY];
  ++ t->n[GREY];
  ++ tm.n[GREY];
}


static __inline
void tm_tread_mark(tm_tread *t, tm_node *n)
{
  assert(tm_node_color(n) != WHITE);
  if ( tm_node_color(n) == ECRU ) {
    tm_block *b = tm_node_to_block(n);

    if ( t->bottom == n ) {
      t->bottom = tm_node_next(n);
    }

    tm_tread_mark_grey(t, n);

    -- b->n[ECRU];
    -- t->n[ECRU];
    -- tm.n[ECRU];

    tm_tread_VALIDATE(t);
  }
}


void _tm_node_scan (tm_node *n);

static __inline
void tm_tread_scan(tm_tread *t)
{
  if ( t->scan != t->top ) {
    tm_node *n = t->scan;
    tm_block *b = tm_node_to_block(n);

    t->scan = tm_node_prev(n);

    assert(tm_list_color(n) == GREY);

    tm_list_set_color(n, BLACK);

    -- b->n[GREY];
    -- t->n[GREY];
    -- tm.n[GREY];

    ++ b->n[BLACK];
    ++ t->n[BLACK];
    ++ tm.n[BLACK];

    _tm_node_scan (n);

    assert(tm_list_color(n) == BLACK);

    // tm_tread_VALIDATE(t);
  }
}



static __inline
void tm_tread_mutation(tm_tread *t, tm_node *n)
{
  if ( tm_node_color(n) == BLACK ) {
    tm_block *b = tm_node_to_block(n);

    tm_tread_mark_grey(t, n);

    -- b->n[BLACK];
    -- t->n[BLACK];
    -- tm.n[BLACK];

    tm_tread_VALIDATE(t);
  }
}


static __inline
void tm_tread_after_roots(tm_tread *t);


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
  tm_colors_flip(&tm.colors);
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

  /* Do not allocate into WHITE. */
  if ( tm_node_color(t->bottom) == WHITE ) {
    t->bottom = tm_node_next(t->bottom);
  }

  /* Start scanning at top. */
  t->scan = t->top;

  tm_tread_VALIDATE(t);

#ifdef tm_tread_UNIT_TEST
  /* Mark roots. */
  tm_tread_mark_roots(t);
  tm_tread_after_roots(t);
#endif

}

static __inline
void tm_tread_after_roots(tm_tread *t)
{
  /* If there was no WHITE, assume more_white(). */
  if ( ! t->n[WHITE] ) {
    t->bottom = t->free = tm_node_next(t->scan);
  }

  tm_tread_VALIDATE(t);
}


#endif
