/** \file internal.h
 * \brief Internals.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_INTERNAL_H
#define _tredmill_INTERNAL_H

/****************************************************************************/

#include "tm.h"

#include "tredmill/list.h"
#include "tredmill/config.h"
#include "tredmill/color.h"
#include "tredmill/phase.h"
#include "tredmill/debug.h"
#include "tredmill/log.h"
#include "tredmill/root.h"
#include "tredmill/node.h"
#include "tredmill/block.h"
#include "tredmill/type.h"
#include "tredmill/stats.h"
#include "tredmill/barrier.h"
#include "tredmill/tm_data.h"


/*@}*/



/**************************************************************************/
/*! \defgroup node_iterator Node: Iterator */
/*@{*/


/*! Initializes a colored node iterator. */
static __inline
void tm_node_iterator_init(tm_node_iterator *ni)
{
  ni->type = (void *) &tm.types;
#if 0
  ni->node_next = (void *) &ni->type->color_list[ni->color];
#endif
  ni->node = 0;
  ni->scan_node = 0;
  ni->scan_ptr = 0;
  ni->scan_end = 0;
  ni->scan_size = 0;
}

/*! Returns the next node from the iterator. */
static __inline
tm_node * tm_node_iterator_next(tm_node_iterator *ni)
{
  size_t i = 0;

  while ( i ++ < tm.n[ni->color] ) {
    // fprintf(stderr, "  t%p n%p i%ul\n", ni->type, ni->node_next, i);
  
    /* Wrap around on types? */
    if ( (void *) ni->type == (void *) &tm.types ) {
      ni->type = (void*) tm_list_next(ni->type);

      /*
       * There are no tm_type objects at all.
       * This should never happen!
       */
      if ( (void *) ni->type == (void *) &tm.types ) {
	tm_abort();
	return 0;
      }
      
    next_type:
#if 0
      /* Start on type node color_list. */
      ni->node_next = (void *) &ni->type->color_list[ni->color];
#endif
      tm_assert(tm_list_color(ni->node_next) == ni->color);

      /* Move iterator to first node. */
      ni->node_next = (void *) tm_list_next(ni->node_next);
    }

#if 0
    /* At end of type color list? */
    if ( ! ni->node_next || (void *) ni->node_next == (void *) &ni->type->color_list[ni->color] ) {
      ni->type = (void*) tm_list_next(ni->type);
      goto next_type;
    }
#endif

    if ( tm_node_color(ni->node_next) != ni->color ) {
      fprintf(stderr, "  tm_node_iterator %p: node_color_iter[%s] derailed at node_next %p color %s, t#%d\n",
	      ni, 
	      tm_color_name[ni->color], 
	      ni->node_next, 
	      tm_color_name[tm_node_color(ni->node_next)],
	      ni->type->id
	      );
      // tm_abort();
      ni->type = (void*) tm_list_next(ni->type);
      goto next_type;
    }
    else {
      /* Return current node. */
      ni->node = ni->node_next;
      
      /* Move iterator to next node. */
      ni->node_next = (void*) tm_list_next(ni->node_next);
      
      return ni->node;
    }
  }

  return 0;
}

/*! Initialize the iterator of all nodes of a given color. */
#define tm_node_LOOP_INIT(C) \
  tm_node_iterator_init(&tm.node_color_iter[C])

/*! Begin a loop over all nodes of a given color. */
#define tm_node_LOOP(C) {						\
  tm_node *n;								\
  tm_type *t = 0;							\
  while ( (n = tm_node_iterator_next(&tm.node_color_iter[C])) ) {	\
    t = tm.node_color_iter[C].type;					\
    {

/*! Break out of a loop over all nodes of a given color. */
#define tm_node_LOOP_BREAK(C) break

/*! End a loop over all nodes of a given color. */
#define tm_node_LOOP_END(C)			\
    }						\
  }						\
}


/*@}*/

/****************************************************************************/
/*! \defgroup internal_routine Internal: Routine */
/*@{*/


/*! Sets the color of a tm_node, in a tm_block. */
void tm_node_set_color(tm_node *n, tm_block *b, tm_color c);

void _tm_set_stack_ptr(void* ptr);

/*! Clears the stack and initializes register root set. */
void __tm_clear_some_stack_words();
#define _tm_clear_some_stack_words() \
setjmp(tm.jb); \
__tm_clear_some_stack_words()


void *_tm_alloc_type_inner(tm_type *type);
void *_tm_alloc_inner(size_t size);
void *_tm_alloc_desc_inner(tm_adesc *desc);
void *_tm_realloc_inner(void *ptr, size_t size);
void _tm_free_inner(void *ptr);
void _tm_gc_full_inner();

/*@}*/

/****************************************************************************/
/* Internals */

#include "tredmill/os.h"
#include "tredmill/root.h"
#include "tredmill/page.h"
#include "tredmill/ptr.h"
#include "tredmill/mark.h"
#include "tredmill/node_color.h"

/****************************************************************************/
/* Support. */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#endif
