/** \file internal.c
 * \brief Internals
 */
#include "internal.h"
#include "tread.h"
#include "tread_inline.h"


void tm_node_mutation(tm_node *n)
{
  tm_block *block = tm_node_to_block(n);
  tm_type *type = tm_block_type(block);
  tm_tread_mutation(tm_type_tread(type), n);
}


static __inline
void _tm_alloc_scan_any(tm_type *t)
{
  tm_tread_scan(&t->tread);

  if ( tm.type_scan != t ) {
    tm_tread_scan(tm_type_tread(tm.type_scan));
  }
  tm.type_scan = tm_list_next(tm.type_scan);
  if ( (void*) tm.type_scan == (void*) &tm.types ) {
    tm.type_scan = tm_list_next(tm.type_scan);
  }
}

static __inline
void _tm_alloc_flip_all()
{
  tm_type *type;

  /* Flip the colors, globally. */
  tm_colors_flip(&tm.colors);

  /* Flip each type. */
  tm_list_LOOP(&tm.types, type) {
    tm_tread_flip(&type->tread);
  }
  tm_list_LOOP_END;

  /* Mark roots. */
  tm_root_scan_all();

  /* Flip each type. */
  tm_list_LOOP(&tm.types, type) {
    tm_tread_after_roots(&type->tread);
  }
  tm_list_LOOP_END;
}



/**
 * Allocates a node of a given type.
 *
 * Algorithm:
 *
 * Depending on tm.phase, the allocator will do some other work
 * before returning an data ptr from a tm_node:
 * - Unmark some allocated nodes (BLACK->ECRU).
 * - Scan roots (stacks or globals) for pointers to nodes.
 * - Scan marked nodes for pointers to other nodes (GREY->BLACK).
 * - Sweep some unused nodes to their free lists (ECRU->WHITE).
 * .
 *
 * A node is taken from the tm_type's free list, or a new
 * block is allocated and parcelled into the tm_type's free list.
 *
 */
void *_tm_alloc_type_inner(tm_type *t)
{
  void *ptr;

  /*! Reset during tm_alloc() stats: tm_data.alloc_n. */
  tm.alloc_pass = 0;
  memset(tm.alloc_n, 0, sizeof(tm.alloc_n));

  /*! Keep track of how many allocs since last sweep. */
  ++ tm.alloc_since_sweep;

  // tm_validate_lists();

  /* Try to allocate again? */
  ++ tm.alloc_pass;

  /* BEGIN CRITICAL SECTION */

  /* "Flip" if there are no WHITE and no GREY nodes. */
  if ( ! tm.n[WHITE] && ! tm.n[GREY] ) {
    _tm_alloc_flip_all();
  }

  /* Allocate some more nodes. */
  if ( ! t->n[WHITE] ) {
    tm_type_parcel_or_alloc_node(t);
  }

  /* Check if some was allocated. */
  if ( t->n[WHITE] ) {
    /*! Take node from tread's free list. */
    tm_node *n = tm_tread_alloc_node_from_free_list(&t->tread);
    
    /*! Updates its stats. */
    ptr = tm_type_prepare_allocated_node(t, n);
  } else {
    /* Out of memory! */
    ptr = 0;
  }

  /* END CRITICAL SECTION */

#if tm_ptr_to_node_TEST
  /* Validate tm_ptr_to_node() */
  if ( ptr ) {
    char *p = ptr;
    tm_node *n = (void*) (p - tm_node_HDR_SIZE);
    tm_assert(tm_ptr_to_node(n) == 0);
    tm_assert(tm_ptr_to_node(p) == n);
    tm_assert(tm_ptr_to_node(p + 1) == n);
    tm_assert(tm_ptr_to_node(p + t->size / 2) == n);
    tm_assert(tm_ptr_to_node(p + t->size - 1) == n);
#if tm_ptr_AT_END_IS_VALID
    tm_assert(tm_ptr_to_node(p + t->size) == n);
#else
    tm_assert(tm_ptr_to_node(p + t->size) == 0);
#endif
  }
#endif

  tm_alloc_log(ptr);

#if 0
  tm_msg("a %p[%lu]\n", ptr, (unsigned long) t->size);
#endif

  /*! Return a new data ptr or 0. */
  return (void*) ptr;
}


/**
 * Manually returns a node back to its tm_type free list.
 *
 * Assumes that ptr is pure pointer to the beginning of a tm_node's data space.
 */
void _tm_free_inner(void *ptr)
{
  tm_block *b;
  tm_node *n;

  n = tm_pure_ptr_to_node(ptr);
  b = tm_node_to_block(n);
  // _tm_block_validate(b);
  tm_assert(tm_node_color(n) != WHITE);
  tm_abort();
#if 0
  /* FIXME! */
  tm_node_set_color(n, b, WHITE);
#endif
}


/***************************************************************************/
/*! \defgroup gc GC */
/*@{*/


/**
 * Force a full GC for a specific type.
 *
 * - Unmark all nodes.
 * - Scan all roots.
 * - Scan all marked nodes.
 * - Sweep all unmarked nodes.
 * - Sweep all unused blocks.
 * .
 */
void _tm_gc_full_type_inner(tm_type *type)
{
  tm_msg("gc {\n");

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_gc_inner);
#endif

  _tm_sweep_is_error = 0;

  /* IMPLEMENT. */

  /*! Clear GC stats. */
  tm_gc_clear_stats();

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_gc_inner);
#endif

  tm_msg("gc }\n");
}


/**
 * Force a full GC sequence.
 */
void _tm_gc_full_inner()
{
  _tm_gc_full_type_inner(0);
}


/*@}*/


