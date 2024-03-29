/** \file mark.h
 * \brief Marking Primitives.
 */
#ifndef tm_MARK_H
#define tm_MARK_H

#include "tread_inline.h"

/***************************************************************************/
/*! \defgroup marking Marking */
/*@}*/

/**
 * Marks a node as in-use.
 */
static __inline
int _tm_node_mark(tm_node *n)
{
  int c = tm_node_color(n);

  if ( c == ECRU ) {
    /**
     * If ECRU,
     * the node has not been scheduled for marking;
     * Schedule it for marking.
     * Return true to alert caller that work is to be done.
     */
#if 0
    tm_node_set_color(n, tm_node_to_block(n), GREY);
#else
    tm_block *b = tm_node_to_block(n);
    tm_tread_mark(tm_type_tread(tm_node_to_type(n)), n);
    -- b->n[ECRU];
    ++ b->n[GREY];
#endif
    return 1;
  } else if ( c == WHITE ) {
    /** If WHITE, we have a spurious pointer into a free node?
     * ABORT!
     */
    tm_abort();
  }

  /**
   * If node is BLACK or GREY
   * the node has already been marked.
   * DO NOTHING.
   */

  /*! Otherwise, return false: there is nothing to do. */
  return 0;
}


/**
 * Mark a possible pointer.
 * Returns true if something was marked.
 */
static __inline 
tm_node * _tm_mark_possible_ptr(void *p)
{
  tm_node *n;
  
  if ( p && (n = tm_ptr_to_node(p)) && _tm_node_mark(n) )
    return n;
    
  return 0;
}

void _tm_root_loop_init();

void _tm_register_scan();

void _tm_set_stack_ptr(void *stackvar);
void _tm_stack_scan();

void tm_root_scan_all();
int _tm_root_scan_some();

__inline
void _tm_range_scan(const void *b, const void *e);

void _tm_node_scan(tm_node *n);

size_t _tm_node_scan_some(size_t amount);
void _tm_node_scan_all();

/*@}*/

#endif
