/* $Id: user.c,v 1.5 1999-12-29 02:52:32 stephensk Exp $ */

#include "tm.h"
#include "internal.h"

#define tv_fmt "%lu.%06lu"
#define tv_fmt_args(V) (unsigned long) (V).tv_sec, (unsigned long) (V).tv_usec

/***************************************************************************/

typedef struct timeval tv;

static __inline void tv_sum(tv *sum, tv *dt, tv *t0, tv *t1, tv *mt)
{
  int mt_changed = 0;

  /* Normalize t1 - t0 */
  while ( t1->tv_usec < t0->tv_usec ) {
    t1->tv_usec += 1000000;
    t1->tv_sec --;
  }

  /* Compute dt */
  dt->tv_usec = t1->tv_usec - t0->tv_usec;
  sum->tv_usec += dt->tv_usec;

  dt->tv_sec = t1->tv_sec - t0->tv_sec;
  sum->tv_sec += dt->tv_sec;

  /* Normalize sum */
  while ( sum->tv_usec > 1000000 ) {
    sum->tv_usec -= 1000000;
    sum->tv_sec ++;
  }

  /* Calc max dt time. */
  if ( mt->tv_sec < dt->tv_sec ) {
    *mt = *dt;
    mt_changed = 1;
  } else if ( mt->tv_sec == dt->tv_sec && mt->tv_usec < dt->tv_usec ) {
    mt->tv_usec = dt->tv_usec;
    mt_changed = 1;
  }
  if ( mt_changed ) {
    tm_msg("T A wt" tv_fmt "\n", tv_fmt_args(*mt));
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
  tv_sum(&tm.ts, &tm.td, &t0, &t1, &tm.tw);
  tm_msg("T A"
	 " dt" tv_fmt 
	 " st" tv_fmt 
	 " \n",
	 tv_fmt_args(tm.td),
	 tv_fmt_args(tm.ts)
	 );
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
