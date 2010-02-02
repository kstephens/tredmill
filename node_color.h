#ifndef _tm_NODE_COLOR_H
#define _tm_NODE_COLOR_H

#include "tredmill/ptr.h"   /* tm_node_to_block() */

/**
 * Sets the color of a tm_node.
 *
 */
static __inline
void _tm_node_set_color(tm_node *n, tm_block *b, tm_type *t, tm_color c)
{
  int nc = tm_node_color(n);

  tm_assert_test(b);
  tm_assert_test(t);
  tm_assert_test(c < tm_TOTAL);

  /*! Increment cumulative global stats. */
  ++ tm.alloc_n[c];
  ++ tm.alloc_n[tm_TOTAL];

  /*! Increment global color transitions. */
  if ( ! tm.parceling ) {
    tm_assert_test(c != nc);
    ++ tm.n_color_transitions[nc][c];
    ++ tm.n_color_transitions[nc][tm_TOTAL];
    ++ tm.n_color_transitions[tm_TOTAL][c];
    ++ tm.n_color_transitions[tm_TOTAL][tm_TOTAL];
  }

  /*! Adjust global stats. */
  ++ tm.n[c];
  tm_assert_test(tm.n[nc]);
  -- tm.n[nc];

  /*! Adjust type stats. */
  ++ t->n[c];
  tm_assert_test(t->n[nc]);
  -- t->n[nc];

  /*! Adjust block stats. */
  ++ b->n[c];
  tm_assert_test(b->n[nc]);
  -- b->n[nc];

  /*! Set the tm_code color. */
  tm_list_set_color(n, c);
}


static __inline
void *tm_type_prepare_allocated_node(tm_type *t, tm_node *n)
{
  void *ptr;

  /*! Assert its located in a valid tm_block. */
  tm_assert_test(tm_node_to_block(n)->type == t);
  
  /*! Get the ptr the tm_node's data space. */
  ptr = tm_node_to_ptr(n);
  
  /*! Clear the data space. */
  memset(ptr, 0, t->size);
  
  /*! Mark the tm_node's page as used. */
  _tm_page_mark_used(ptr);
  
  /*! Keep track of allocation amounts. */
  ++ tm.nodes_allocated_since_gc;
  tm.bytes_allocated_since_gc += t->size;
  tm.n[tm_b] += t->size;
  
#if 0
  tm_msg("N a n%p[%lu] t%p\n", 
	 (void*) n, 
	 (unsigned long) t->size,
	 (void*) t);
#endif

  /*! Return the pointer to the data space. */
  return ptr;
}


#endif
