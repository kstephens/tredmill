#include "tm.h"
#include "internal.h"

/***************************************************************************/

void *tm_alloc(size_t size)
{
  void *ptr = 0;

  if ( size == 0 )
    return 0;

#if 0
  memset(&tm.alloc_time, 0, sizeof(tm.alloc_time));
  times(&tm.alloc_time);
  tm_msg("Times before: %lu %lu %lu %lu\n", 
	 tm.alloc_time.tms_utime,
	 tm.alloc_time.tms_stime,
	 tm.alloc_time.tms_cutime,
	 tm.alloc_time.tms_cstime);
#endif

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_alloc_inner(size);

#if 0
  times(&tm.alloc_time);
  tm_msg("Times after: %lu %lu %lu %lu\n", 
	 tm.alloc_time.tms_utime,
	 tm.alloc_time.tms_stime,
	 tm.alloc_time.tms_cutime,
	 tm.alloc_time.tms_cstime);
#endif

  return ptr;
}

void *tm_realloc(void *oldptr, size_t size)
{
  void *ptr = 0;

  if ( oldptr == 0 )
    return tm_alloc(size);

  if ( size == 0 ) {
    tm_free(oldptr);
    return 0;
  }

#if 0
  memset(&tm.alloc_time, 0, sizeof(tm.alloc_time));
  times(&tm.alloc_time);
  tm_msg("Times before: %lu %lu %lu %lu\n", 
	 tm.alloc_time.tms_utime,
	 tm.alloc_time.tms_stime,
	 tm.alloc_time.tms_cutime,
	 tm.alloc_time.tms_cstime);
#endif

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_realloc_inner(oldptr, size);

#if 0
  times(&tm.alloc_time);
  tm_msg("Times after: %lu %lu %lu %lu\n", 
	 tm.alloc_time.tms_utime,
	 tm.alloc_time.tms_stime,
	 tm.alloc_time.tms_cutime,
	 tm.alloc_time.tms_cstime);
#endif

  return ptr;
}

void tm_gc_full()
{
  void *ptr = 0;

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  tm_gc_full_inner();
}

/***************************************************************************/
