/* $Id: user.c,v 1.8 2002-05-11 02:33:27 stephens Exp $ */

#ifndef tm_USE_times
#define tm_USE_times 0
#endif

#include "tm.h"
#include "internal.h"

#if tm_USE_times

#include <sys/times.h>

#else

#include <sys/time.h>
#include <unistd.h>

static __inline double tv_2_double(struct timeval *t)
{
  return (double) t->tv_sec + (double) t->tv_usec / 1000000.0;
}
#endif

int _tm_user_bss[4];
int _tm_user_data[4] = { 0, 1, 2, 3 };

/***************************************************************************/
/* time stats support. */



void tm_time_stat_begin(tm_time_stat *ts)
{
#if tm_USE_times
  struct tms t0;
  times(&t0);
  ts->t0 = (double) t0.tms_utime / (double) CLOCKS_PER_SEC;
  ts->t01 = (double) t0.tms_stime / (double) CLOCKS_PER_SEC;
#else
  struct timeval t0;
  gettimeofday(&t0, 0);
  ts->t0 = tv_2_double(&t0);
#endif
}


void tm_time_stat_print_(tm_time_stat *ts, int flags)
{
#define tv_fmt "%.7f"
#define tv_fmt_args(V) (double) (V)

  tm_msg("T %s \t"
	 ,
	 ts->name);

  if ( ! (flags & (2|4)) ) {
    tm_msg1(
	   " dt" tv_fmt
	   ,
	   tv_fmt_args(ts->td)
	   );
  }

  tm_msg1(
	 " st" tv_fmt 
	 ,
	 tv_fmt_args(ts->ts)
	 );

  if ( flags & 1 ) {
    tm_msg1(
	   " wt" tv_fmt
	   ,
	   tv_fmt_args(ts->tw)
	   );
  }

  if ( flags & 2 ) {
    tm_msg1(
	   " c%lu"
	   , 
	   (unsigned long) ts->count
	   );
  }

  if ( flags & 4 ) {
    tm_msg1(
	    " at%7f"
	    ,
	    (double) ts->ts / ts->count
	    );
  }

  tm_msg1("\n");
}


void tm_time_stat_end(tm_time_stat *ts)
{
#if tm_USE_times
  {
    struct tms t1;
    times(&t1);
    ts->t1 = (double) t1.tms_utime / (double) CLOCKS_PER_SEC;
    ts->t11 = (double) t1.tms_stime / (double) CLOCKS_PER_SEC;
  }
#else
  {
    struct timeval t1;
    gettimeofday(&t1, 0);
    ts->t1 = tv_2_double(&t1);
  }
#endif

  ts->count ++;

  /* Compute dt. */
  ts->td = ts->t1 - ts->t0;
  ts->td += ts->t11 - ts->t01;

  /* Compute sum of dt. */
  ts->ts += ts->td;

  /* Compute worst time. */
  if ( (ts->tw_changed = ts->tw < ts->td) ) {
    ts->tw = ts->td;
  }

  /* Print stats. */
  tm_time_stat_print_(ts, ts->tw_changed ? 1 : 0);
}


void tm_print_time_stats()
{
  int i;

  tm_msg_enable("T", 1);

  tm_msg("T {\n");

  tm_time_stat_print_(&tm.ts_alloc_os, ~0);
  tm_time_stat_print_(&tm.ts_alloc, ~0);
  tm_time_stat_print_(&tm.ts_free, ~0);
  tm_time_stat_print_(&tm.ts_gc, ~0);
  for ( i = 0; i < (sizeof(tm.ts_phase)/sizeof(tm.ts_phase[0])); ++ i ) {
    tm_time_stat_print_(&tm.ts_phase[i], ~0);
  }

  tm_msg("T }\n");

  tm_msg_enable("T", 0);
}


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

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);

  ptr = tm_alloc_inner(size);

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

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_alloc_desc_inner(desc);

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

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_realloc_inner(oldptr, size);

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

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  tm_free_inner(ptr);

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

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);

  tm_gc_full_inner();

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_gc);
#endif
}


/***************************************************************************/
