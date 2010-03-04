/** \file internal.c
 * \brief Internals
 */
#include "internal.h"
#include "tread.h"
#include "tread_inline.h"


/**
 * Scans a node for internal pointers.
 *
 * If the node's type has user-defined allocation descriptor scan function,
 * call it.
 * Otherwise, scan the entire node's data for potential pointers.
 */
void _tm_node_scan(tm_node *n)
{
  tm_type *type = tm_node_type(n);

  if ( type->desc && type->desc->scan ) {
    type->desc->scan(type->desc, tm_node_ptr(n));
  } else {
    _tm_range_scan(tm_node_ptr(n), tm_node_ptr(n) + type->size);
  }

#if 0
  fprintf(stderr, "[B]");
  fflush(stderr);
#endif
}

/**
 * Marks a tm_node as mutated.
 */
void tm_node_mutation(tm_node *n)
{
  tm_block *block = tm_node_to_block(n);
  tm_type *type = tm_block_type(block);
  if ( tm_tread_mutation(tm_type_tread(type), n) ) {
    fprintf(stderr, "*");
  }
}


/**
 *
 * Scan a GREY node of tm_type t,
 * and/or scan a GREY node of any other tm_types.
 */
static __inline
void _tm_alloc_scan_any(tm_type *t)
{
  int i;

  tm_tread_scan(&t->tread);

  for ( i = 0; i < tm.type_id; ++ i ) {
    if ( tm.type_scan != t ) {
      tm_tread_scan(tm_type_tread(tm.type_scan));
    }
    
    tm.type_scan = tm_list_next(tm.type_scan);
    if ( (void*) tm.type_scan == (void*) &tm.types ) {
      tm.type_scan = tm_list_next(tm.type_scan);
    }
  }
}


/**
 * Scan all GREY nodes in all types.
 */
static __inline
void _tm_alloc_scan_all()
{
  while ( tm.n[GREY] ) {
    int i;
 
    for ( i = 0; i < tm.type_id; ++ i ) {
      tm_tread_scan(tm_type_tread(tm.type_scan));
    
      tm.type_scan = tm_list_next(tm.type_scan);
      if ( (void*) tm.type_scan == (void*) &tm.types ) {
	tm.type_scan = tm_list_next(tm.type_scan);
      }
    }
  }
}


/**
 * Flip colors, globaly.
 * Flip each tm_type.
 * Mark all roots.
 * Notify each tm_type of root mark.
 * Reset alloc_since_flip counter.
 */
static __inline
void _tm_alloc_flip_all()
{
  tm_type *type;

  fprintf(stderr, "#### _tm_alloc_flip_all %lu: %lu %lu %lu %lu %lu\n", 
	  (unsigned long) tm.alloc_id,
	  (unsigned long) tm.n[0],
	  (unsigned long) tm.n[1],
	  (unsigned long) tm.n[2],
	  (unsigned long) tm.n[3],
	  (unsigned long) tm.n[4]);

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

  tm.alloc_since_flip = 0;
}



/**
 * Allocates a node of a given type.
 *
 * Algorithm:
 *
 * A node is taken from the tm_type's free list, or a new
 * block is allocated and parcelled into the tm_type's free list.
 *
 */
void *_tm_alloc_type_inner(tm_type *t)
{
  void *ptr;

  /*! Increment the allocation id. */
  ++ tm.alloc_id;

  /*! Reset during tm_alloc() stats: tm_data.alloc_n. */
  tm.alloc_pass = 0;
  memset(tm.alloc_n, 0, sizeof(tm.alloc_n));

  /*! Keep track of how many allocs since last sweep. */
  ++ tm.alloc_since_flip;
  ++ tm.alloc_since_sweep;

  // tm_validate_lists();

  /* Try to allocate again? */
  ++ tm.alloc_pass;

  /* BEGIN CRITICAL SECTION */

  fprintf(stderr, "%lu %lu %lu %lu: %lu %lu\n", 
	  (unsigned long) tm.n[WHITE],
	  (unsigned long) tm.n[ECRU],
	  (unsigned long) tm.n[GREY],
	  (unsigned long) tm.n[BLACK],
	  (unsigned long) tm.alloc_since_flip,
	  (unsigned long) tm.n[tm_TOTAL]);

  /* HACK!!! */
  if ( ! tm.n[WHITE] ) {
    if ( tm.alloc_since_flip > tm.n[tm_TOTAL] / 2 ) {
      fprintf(stderr, "[SCANALL]");
      _tm_alloc_scan_all();
    }
  }

  /*! If no WHITE or GREY nodes, maybe flip? */
  if ( ! tm.n[WHITE] && ! tm.n[GREY] ) {
    _tm_alloc_flip_all();
  }

  /* Allocate some more nodes. */
  if ( ! t->n[WHITE] ) {
    tm_type_parcel_or_alloc_node(t);
  }

  /*! If some nodes were were parcelled, */
  if ( t->n[WHITE] ) {
    /*! Take node from tread's free list. */
    tm_node *n = tm_tread_alloc_node_from_free_list(&t->tread);
    
    /*! Update the stats for the allocated node. */
    ptr = tm_type_prepare_allocated_node(t, n);
  } else {
    /*! Otherwise, Out of memory! */
    ptr = 0;
  }

  /* END CRITICAL SECTION */

  fprintf(stderr, "A");

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


