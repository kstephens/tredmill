/* $Id: user.c,v 1.6 2000-01-07 09:38:31 stephensk Exp $ */

#include "tm.h"
#include "internal.h"

#define tv_fmt "%lu.%06lu"
#define tv_fmt_args(V) (unsigned long) (V).tv_sec, (unsigned long) (V).tv_usec

int _tm_user_bss[4];
int _tm_user_data[4] = { 0, 1, 2, 3 };

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

void tm_time_stat_begin(tm_time_stat *ts)
{
  gettimeofday(&ts->t0, 0);
}

void tm_time_stat_end(tm_time_stat *ts, const char *name)
{
  ts->name = name;
  ts->count ++;
  gettimeofday(&ts->t1, 0);
  tv_sum(&ts->ts, &ts->td, &ts->t0, &ts->t1, &ts->tw);
  tm_msg("T %s"
	 " dt" tv_fmt 
	 " st" tv_fmt 
	 " \n",
	 name, 
	 tv_fmt_args(ts->td),
	 tv_fmt_args(ts->ts)
	 );
}

void *tm_alloc(size_t size)
{
  void *ptr = 0;

  if ( size == 0 )
    return 0;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_alloc);
#endif

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_alloc_inner(size);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc, "A");
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

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_alloc_desc_inner(desc);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc, "A");
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

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_alloc);
#endif

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_realloc_inner(oldptr, size);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc, "R");
#endif

  return ptr;
}

void tm_gc_full()
{
  void *ptr = 0;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_gc);
#endif

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  tm_gc_full_inner();

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_gc, "GC");
#endif
}

/***************************************************************************/
