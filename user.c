/* $Id: user.c,v 1.4 1999-12-28 23:44:24 stephensk Exp $ */

#include "tm.h"
#include "internal.h"

/***************************************************************************/

typedef struct timeval tv;

static __inline void tv_sum(tv *sum, tv *dt, tv *t0, tv *t1)
{
  while ( t1->tv_usec < t0->tv_usec ) {
    t1->tv_usec += 1000000;
    t1->tv_sec --;
  }

  dt->tv_usec = t1->tv_usec - t0->tv_usec;
  sum->tv_usec += dt->tv_usec;

  dt->tv_sec = t1->tv_sec - t0->tv_sec;
  sum->tv_sec += dt->tv_sec;

  while ( sum->tv_usec > 1000000 ) {
    sum->tv_usec -= 1000000;
    sum->tv_sec ++;
  }
}

void *tm_alloc(size_t size)
{
  tv t0, t1;
  void *ptr = 0;

  if ( size == 0 )
    return 0;

#if 1
  gettimeofday(&t0, 0);
#endif

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_alloc_inner(size);

#if 1
  gettimeofday(&t1, 0);
  tv_sum(&tm.ts, &tm.td, &t0, &t1);
  tm_msg("T A dt%lu.%09lu st%lu.%09lu\n",
	 (unsigned long) tm.td.tv_sec,
	 (unsigned long) tm.td.tv_usec,
	 (unsigned long) tm.ts.tv_sec,
	 (unsigned long) tm.ts.tv_usec);
#endif

  return ptr;
}


void *tm_alloc_desc(tm_adesc *desc)
{
  void *ptr = 0;

  if ( desc == 0 || desc->size == 0 )
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
  ptr = tm_alloc_desc_inner(desc);

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
