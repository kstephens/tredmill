/** \file barrier.c
 * \brief Write barriers.
 */
#include "internal.h"

/***************************************************************************/
/*! \defgroup write_barrier_hook Write Barrier: Hook */
/*@{*/

/*! Write barrier hook for pointer into stack or data segment root. */
void (*_tm_write_root)(void *referent) = __tm_write_root_ignore;
/*! Write barrier hook for unknown type of pointer (could be on stack, data segment or within tm_alloc() node.) */
void (*_tm_write_barrier)(void *referent) = __tm_write_barrier_ignore;
/*! Write barrier hook for pure pointer (a pointer to beginning of tm_alloc()'ed address.) */
void (*_tm_write_barrier_pure)(void *referent) = __tm_write_barrier_pure_ignore;

/*@}*/

/*******************************************************************************/
/*! \defgroup write_barrier_function Write Barrier: Function */
/*@{*/

/**
 * Marks a possible pointer.
 *
 * tm_root_write() should be called after modifing a root ptr.
 */
void tm_mark(void *ptr)
{
  _tm_mark_possible_ptr(ptr);
}

/**
 * ???
 */
void __tm_write_root_ignore(void *referent)
{
  tm_abort();
  /* DO NOTHING */
}

/**
 * ???
 */
void __tm_write_root_root(void *referent)
{
  tm_abort();
  tm_mark(* (void**) referent);
#if 0
  tm_msg("w r p%p\n", referent);
#endif
}


/**
 * ???
 */
void __tm_write_root_mark(void *referent)
{
  tm_abort();
  tm_mark(* (void**) referent);
#if 0
  tm_msg("w r p%p\n", referent);
#endif
}


/**
 * ???
 */
void __tm_write_root_sweep(void *referent)
{
  tm_abort();
#if 0
  tm_msg("w r p%p\n", referent);
#endif
}


/*@}*/

/*******************************************************************************/
/*! \defgroup write_barrier_general  Write Barrier: General */
/*@{*/


/**
 * Must be called after any ptrs
 * within the tm_node n are mutated.
 */
static __inline
void tm_write_barrier_node(tm_node *n)
{
  switch ( tm_node_color(n) ) {
  case tm_GREY:
    /**
     * If node is tm_GREY,
     * it is already marked for scanning.
     *
     * BUG??: What if this node is partially scanned already?
     */
    break;

  case tm_ECRU:
    /**
     * If node is tm_ECRU,
     * It has not been reached yet.
     */
    break;

  case tm_BLACK:
    /**
     * If node is tm_BLACK,
     * The node has already been marked and scanned.
     * The mutator may have introduced new pointers
     * to ECRU nodes.
     *
     * Reschedule it to be scanned.
     */
#if tm_TIME_STAT
    tm_time_stat_begin(&tm.ts_barrier_black);
#endif

    tm_node_set_color(n, tm_node_to_block(n), tm_GREY);

#if tm_TIME_STAT
    tm_time_stat_end(&tm.ts_barrier_black);
#endif
#if 0
    tm_msg("w b n%p\n", n);
#endif
    break;

  default:
    tm_abort();
    break;
  }
}

/**
 * Write barrier for nothing.
 *
 * DO NOTHING
 */
void __tm_write_barrier_ignore(void *ptr)
{
}


#define RETURN goto _return

/**
 * Write barrier during tm_ROOT phase.
 *
 * Don't know if this root has been marked yet or not.
 */
void __tm_write_barrier_root(void *ptr)
{
  /*! Begin time stats. */
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
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
  tm_time_stat_end(&tm.ts_barrier);
#endif
}


/**
 * Write barrier during tm_SCAN phase.
 */
void __tm_write_barrier_mark(void *ptr)
{
  tm_node *n;

  /*! Begin time stats. */
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
#endif

  tm_assert_test(tm.phase == tm_SCAN);

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


/**
 * Write barrier during tm_SWEEP phase.
 */
void __tm_write_barrier_sweep(void *ptr)
{
  tm_node *n;

  /*! Begin time stats. */
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
#endif

  /*! Assert we are in the tm_SWEEP phase. */
  tm_assert_test(tm.phase == tm_SWEEP);

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
   * it must be a reference to a statically allocated object.
   * This means the data root set has (probably) been mutated.
   * Flag the roots for remarking before the sweep phase.
   *
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

#undef RETURN

/*@}*/

/*******************************************************************************/
/*! \defgroup write_barrier_phase  Write Barrier: Phase */
/*@{*/

/**
 * Write barrier for pure pointers ignored.
 *
 * DO NOTHING
 */
void __tm_write_barrier_pure_ignore(void *ptr)
{
}

/**
 * Write barrier for pure pointers during tm_ROOT.
 *
 * DO NOTHING
 */
void __tm_write_barrier_pure_root(void *ptr)
{
}


/**
 * Write barrier for pure pointers during tm_SCAN.
 */
void __tm_write_barrier_pure_mark(void *ptr)
{
  tm_assert_test(tm.phase == tm_SCAN);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}


/**
 * Write barrier for pure pointers during tm_SWEEP.
 */
void __tm_write_barrier_pure_sweep(void *ptr)
{
  tm_assert_test(tm.phase == tm_SWEEP);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}


