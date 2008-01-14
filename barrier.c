#include "internal.h"

/***************************************************************************/
/* write barrier */

void (*_tm_write_root)(void *referent) = __tm_write_root_ignore;
void (*_tm_write_barrier)(void *referent) = __tm_write_barrier_ignore;
void (*_tm_write_barrier_pure)(void *referent) = __tm_write_barrier_pure_ignore;

/*******************************************************************************/

/*
** tm_root_write() should be called after modifing a root ptr.
*/
void tm_mark(void *ptr)
{
  _tm_mark_possible_ptr(ptr);
}


void __tm_write_root_ignore(void *referent)
{
  tm_abort();
  /* DO NOTHING */
}


void __tm_write_root_root(void *referent)
{
  tm_abort();
  tm_mark(* (void**) referent);
#if 0
  tm_msg("w r p%p\n", referent);
#endif
}


void __tm_write_root_mark(void *referent)
{
  tm_abort();
  tm_mark(* (void**) referent);
#if 0
  tm_msg("w r p%p\n", referent);
#endif
}


void __tm_write_root_sweep(void *referent)
{
  tm_abort();
#if 0
  tm_msg("w r p%p\n", referent);
#endif
}


/*******************************************************************************/
/* General write barrier routine. */


/*
** tm_write_barrier(ptr) must be called after any ptrs
** within the node n are mutated.
*/
static __inline
void tm_write_barrier_node(tm_node *n)
{
  switch ( tm_node_color(n) ) {
  case tm_GREY:
    /* Already marked. */
    break;

  case tm_ECRU:
    /* Not reached yet. */
    break;

  case tm_BLACK:
    /*
    ** The node has already been marked and scanned.
    ** The mutator may have introduced new pointers
    ** to ECRU nodes.
    **
    ** Reschedule it to be scanned.
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


void __tm_write_barrier_ignore(void *ptr)
{
  /* DO NOTHING */
}


#define RETURN goto _return

void __tm_write_barrier_root(void *ptr)
{
#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
#endif

  /* Currently marking roots.
  ** Don't know if this root has been marked yet or not.
  */

  /*
  ** If the ptr is a reference to a stack allocated object,
  ** do nothing, because the stack will be scanned atomically before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    ++ tm.stack_mutations; 
#if 0
    tm_msg("w s p%p\n", ptr);
#endif
    RETURN;
  }

  /*
  ** If the ptr is a reference to the heap. 
  ** Do nothing, because we haven't started marking yet.
  */
#if 1
  if ( _tm_page_in_use(ptr) ) 
    RETURN;
#else
  if ( tm_ptr_l <= ptr && ptr <= tm_ptr_h )
    RETURN;
#endif

  /*
  ** Must be a pointer to statically allocated (root) object.
  ** Mark the root as being mutated.
  */
  ++ tm.data_mutations;
#if 0
  tm_msg("w r p%p\n", ptr);
#endif

 _return:
  (void) 0;

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_barrier);
#endif
}


void __tm_write_barrier_mark(void *ptr)
{
  tm_node *n;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
#endif

  tm_assert_test(tm.phase == tm_SCAN);

  /*
  ** If the ptr is a reference to a stack allocated object,
  ** do nothing, because the stack will be marked before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    ++ tm.stack_mutations; 
    // tm_msg("w s p%p\n", ptr);
    RETURN;
  }

  /*
  ** If the ptr is a reference to a node,
  ** schedule it for remarking if it has already been marked.
  **
  ** If it is not a reference to a node,
  ** It must be a reference to a statically allocated object.
  ** This means the data root set has (probably) been mutated.
  ** Must flag the roots for remarking before the sweep phase.
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

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_barrier);
#endif
}


void __tm_write_barrier_sweep(void *ptr)
{
  tm_node *n;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_barrier);
#endif

  tm_assert_test(tm.phase == tm_SWEEP);

  /*
  ** If the ptr is a reference to a stack allocated object,
  ** do nothing, because the stack will be marked before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    ++ tm.stack_mutations;
    // tm_msg("w s p%p\n", ptr);
    RETURN;
  }

  /*
  ** If the ptr is a reference to a node,
  ** schedule it for remarking if it has already been marked.
  **
  ** If it is not a reference to a node,
  ** it must be a reference to a statically allocated object.
  ** This means the data root set has (probably) been mutated.
  ** Must flag the roots for remarking before the sweep phase.
  **
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

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_barrier);
#endif
}

#undef RETURN

/*******************************************************************************/
/* the tm_write_barrier_pure(ptr) assumes ptr is directly from tm_alloc. */

void __tm_write_barrier_pure_ignore(void *ptr)
{
  /* DO NOTHING */
}


void __tm_write_barrier_pure_root(void *ptr)
{
  /* DO NOTHING */
}


void __tm_write_barrier_pure_mark(void *ptr)
{
  tm_assert_test(tm.phase == tm_SCAN);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}


void __tm_write_barrier_pure_sweep(void *ptr)
{
  tm_assert_test(tm.phase == tm_SWEEP);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}


