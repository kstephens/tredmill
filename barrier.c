/** \file barrier.c
 * \brief Write barriers.
 */
#include "internal.h"

/***************************************************************************/
/*! \defgroup write_barrier_hook Write Barrier: Hook */
/*@{*/

/**
 * Write barrier hook for a "pure pointer".
 * A "pure pointer" must point to beginning of tm_alloc()'ed address.
 * A pure pointer should never be 0.
 */
void (*_tm_write_barrier_pure)(void *referent) = __tm_write_barrier_ignore;

/**
 * Write barrier hook for pointer into stack or data segment.
 */
void (*_tm_write_barrier_root)(void *referent) = __tm_write_barrier_ignore;

/**
 * Write barrier hook for unknown pointer.
 * Pointer may be in stack, data segment or within tm_alloc() node.
 * Pointer may be 0.
 */
void (*_tm_write_barrier)(void *referent) = __tm_write_barrier_ignore;

/*@}*/


/*******************************************************************************/
/*! \defgroup write_barrier_function Write Barrier: Function */
/*@{*/

/**
 * Must be called after any ptrs
 * within the tm_node n are mutated.
 */
static __inline
void tm_write_barrier_node(tm_node *n)
{
  int c = tm_node_color(n);

  if ( c == GREY ) {
    /**
     * If node is GREY,
     * it is already marked for scanning.
     */

    /**
     * If this node is already being scanned,
     * resume scanning on another node.
     *
     * This should be very rare.
     *
     * See _tm_node_scan_some().
     */
    if ( tm.node_color_iter[GREY].scan_node == n ) {
#if 0
      fprintf(stderr, "*");
      fflush(stderr);
#endif
      tm.trigger_full_gc = 1;
    }
  } else if ( c == BLACK ) {
    /**
     * If node is BLACK,
     * The node has already been marked and scanned.
     * The mutator may have introduced new pointers
     * to ECRU nodes.
     *
     * Reschedule it to be scanned.
     */
#if tm_TIME_STAT
    tm_time_stat_begin(&tm.ts_barrier_black);
#endif

    tm_node_mutation(n);

#if 0
    fprintf(stderr, "G");
    fflush(stderr);
#endif

#if tm_TIME_STAT
    tm_time_stat_end(&tm.ts_barrier_black);
#endif
#if 0
    tm_msg("w b n%p\n", n);
#endif
  }

  /**
   * If node is ECRU or WHITE,
   * It has not been reached yet.
   */
}


/**
 * Write barrier for nothing.
 *
 * DO NOTHING
 */
void __tm_write_barrier_ignore(void *ptr)
{
}

/*@}*/

/*******************************************************************************/
/*! \defgroup write_barrier_pure  Write Barrier: Pure */
/*@{*/

/**
 * Write barrier for pure pointers.
 */
void __tm_write_barrier_pure(void *ptr)
{
  /*! Begin time stats. */
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier_pure);
#endif

  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));

  /*! End time stats. */
#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_barrier_pure);
#endif
}


/*@}*/


#define RETURN goto _return

/*******************************************************************************/
/*! \defgroup write_barrier_root  Write Barrier: Root */
/*@{*/


/**
 * Write barrier for root or stack pointers.
 *
 * Don't know if this root has been marked yet or not.
 */
void __tm_write_barrier_root(void *ptr)
{
  /*! Begin time stats. */
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier_root);
#endif

  /**
   * If the ptr is a reference to a stack allocated object,
   * do nothing, because the stack will be scanned atomically before
   * the sweep phase.
   */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    ++ tm.stack_mutations; 
#if 0
    tm_msg("w s p%p\n", ptr);
#endif
    RETURN;
  }

  /**
   * Otherwise,
   * If the ptr is a reference to the heap. 
   * Do nothing, because we haven't started marking yet.
   */
#if 1
  if ( _tm_page_in_use(ptr) ) 
    RETURN;
#else
  if ( tm_ptr_l <= ptr && ptr <= tm_ptr_h )
    RETURN;
#endif

  /**
   * Otherwise, ptr must be a pointer to statically allocated (root) object.
   * Mark the roots as being mutated.
   */
  ++ tm.data_mutations;
#if 0
  tm_msg("w r p%p\n", ptr);
#endif

 _return:
  (void) 0;

  /*! End time stats. */
#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_barrier_root);
#endif
}


/*@}*/



/*******************************************************************************/
/*! \defgroup write_barrier_general  Write Barrier: General */
/*@{*/


/**
 * Write barrier for general references.
 */
void __tm_write_barrier(void *ptr)
{
  tm_node *n;

  /*! Begin time stats. */
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
#endif

  /**
   * If the ptr is a reference to a stack allocated object,
   * do nothing, because the stack will be marked before
   * the sweep phase.
   */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    ++ tm.stack_mutations; 
    // tm_msg("w s p%p\n", ptr);
    RETURN;
  }

  /**
   * Otherwise,
   * if the ptr is a reference to a node,
   * schedule it for remarking if it has already been marked.
   *
   * If it is not a reference to a node,
   * It must be a reference to a statically allocated object.
   * This means the data root set has (probably) been mutated.
   * Must flag the roots for remarking before the sweep phase.
   */
  if ( (n = tm_ptr_to_node(ptr)) ) {
    tm_write_barrier_node(n);
  } else {
    ++ tm.data_mutations;
#if 0
    tm_msg("w r p%p\n", ptr);
#endif
  }

 _return:
  (void) 0;

  /*! End time stats. */
#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_barrier);
#endif
}


/*@}*/

