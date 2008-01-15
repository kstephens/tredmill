/* $Id: user.c,v 1.10 2008-01-15 05:21:03 stephens Exp $ */

#include "internal.h"

int _tm_user_bss[4];
int _tm_user_data[4] = { 0, 1, 2, 3 };

/***************************************************************************/
/* User-level routines. */


/*
** Save the registers and stack pointers, before
** calling the "inner" routines.
** Begin timing stats.
*/

void *tm_alloc(size_t size)
{
  void *ptr = 0;

  if ( size == 0 )
    return 0;

  if ( ! tm.inited ) {
    tm_init(0, (char***) ptr, 0);
  }

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_alloc);
#endif

  _tm_clear_some_stack_words();
  _tm_set_stack_ptr(&ptr);

  ptr = _tm_alloc_inner(size);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc);
#endif

  return ptr;
}


void *tm_alloc_desc(tm_adesc *desc)
{
  void *ptr = 0;

  if ( desc == 0 || desc->size == 0 )
    return 0;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_alloc);
#endif

  _tm_clear_some_stack_words();
  _tm_set_stack_ptr(&ptr);
  ptr = _tm_alloc_desc_inner(desc);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc);
#endif

  return ptr;
}


/***************************************************************************/


void *tm_realloc(void *oldptr, size_t size)
{
  void *ptr = 0;

  if ( oldptr == 0 )
    return tm_alloc(size);

  if ( size == 0 ) {
    tm_free(oldptr);
    return 0;
  }

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_alloc);
#endif

  _tm_clear_some_stack_words();
  _tm_set_stack_ptr(&ptr);
  ptr = _tm_realloc_inner(oldptr, size);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc);
#endif

  return ptr;
}


/***************************************************************************/


void tm_free(void *ptr)
{
  if ( ! tm.inited ) {
    tm_init(0, (char***) ptr, 0);
  }

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_free);
#endif

  _tm_clear_some_stack_words();
  _tm_set_stack_ptr(&ptr);
  _tm_free_inner(ptr);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_free);
#endif
}


/***************************************************************************/


void tm_gc_full()
{
  void *ptr = 0;

  if ( ! tm.inited ) {
    tm_init(0, (char***) ptr, 0);
  }

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_gc);
#endif

  _tm_clear_some_stack_words();
  _tm_set_stack_ptr(&ptr);

  _tm_gc_full_inner();

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_gc);
#endif
}


/***************************************************************************/
