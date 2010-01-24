/** \file stats.c
 * \brief Statistics.
 */
#include "internal.h"

/***************************************************************************/

#ifndef tm_USE_times
/*! If true, use times() to collect timing statistics. */
#define tm_USE_times 0
#endif

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


/**
 * Print utilizations.
 */
static
void _tm_print_utilization(const char *name, tm_type *t, size_t *n, int nn, size_t *sum)
{
  int j;

  tm_msg(name);

  /* Compute total number of nodes in use. */
  if ( nn > tm_NU ) {
    switch ( tm.phase ) {
    case tm_UNMARK:
    case tm_ROOT:
    case tm_SCAN:
      n[tm_NU] = n[tm_ECRU] + n[tm_GREY] + n[tm_BLACK];
      break;
      
    case tm_SWEEP:
      n[tm_NU] = n[tm_GREY] + n[tm_BLACK];
      break;
      
    default:
      tm_abort();
    }
    if ( sum && sum != n )
      sum[tm_NU] += n[tm_NU];
  }

  /* Compute total number of bytes in use. */

  if ( sum != n ) {
    if ( nn > tm_b ) {
      n[tm_b] = t ? n[tm_NU] * t->size : 0;
      if ( sum )
	sum[tm_b] += n[tm_b];
    }

    /* Compute avg. bytes / node. */
    if ( nn > tm_b_NU ) {
      n[tm_b_NU] = n[tm_NU] ? n[tm_b] / n[tm_NU] : 0;
      if ( sum )
	sum[tm_b_NU] = sum[tm_NU] ? sum[tm_b] / sum[tm_NU] : 0;
    }

  }

  /* Print fields. */
  for ( j = 0; j < nn; j ++ ) {
    if ( sum && sum != n && j <= tm_B ) {
      sum[j] += n[j];
    }
    tm_msg1("%c%-4lu ", tm_color_name[j][0], (unsigned long) n[j]);
  }

  tm_msg1("\n");
}


/**
 * API: Print statistics.
 */
void tm_print_stats()
{
  tm_type *t;
  size_t sum[tm__LAST3];

  memset(sum, 0, sizeof(sum));

  tm_msg_enable("X", 1);

  tm_msg("X { t#%d : blk_in_use %lu[~%lu], blk_free %lu[~%lu]\n",
	 tm.type_id,
	 tm.n[tm_B],
	 tm.n[tm_B] * tm_block_SIZE,
	 tm.free_blocks_n,
	 tm.free_blocks_n * tm_block_SIZE
	 );

  tm_list_LOOP(&tm.types, t);
  {
    tm_msg("X  t#%d S%-6lu\n", t->id, (unsigned long) t->size, (unsigned) t->n[tm_B]);
    _tm_print_utilization("X    ", t, t->n, sizeof(t->n)/sizeof(t->n[0]), sum);
  }
  tm_list_LOOP_END;

  /* current/max number of blocks/bytes allocated from OS is in tm.n[]. */
  sum[tm_B_OS] = tm.n[tm_B_OS];
  sum[tm_b_OS] = tm.n[tm_b_OS];
  sum[tm_B_OS_M] = tm.n[tm_B_OS_M];
  sum[tm_b_OS_M] = tm.n[tm_b_OS_M];

  _tm_print_utilization("X  S ", 0, sum, sizeof(sum)/sizeof(sum[0]), sum);

  tm_msg("X }\n");

  tm_msg_enable("X", 0);

  tm_print_time_stats();

  tm_print_color_transition_stats();

  tm_print_phase_transition_stats();

  //tm_validate_lists();
}


/**
 * API: Print color transitions statistics.
 */
void tm_print_color_transition_stats()
{
  int i, j;

  // tm_assert(sizeof(tm_color_name) / sizeof(tm_color_name[0]) >= tm_TOTAL + 1);

  tm_msg_enable("C", 1);
  tm_msg("C { color transitions \n");

  tm_msg("C   %-10s ", "from\\to");
  for ( i = 0; i <= tm_TOTAL; ++ i ) {
    tm_msg1("%-10s ", tm_color_name[i]);
  }
  tm_msg1("\n");


  for ( i = 0; i <= tm_TOTAL; ++ i ) {
    tm_msg("C   %-10s ", tm_color_name[i]);
    for ( j = 0; j <= tm_TOTAL; ++ j ) {
      tm_msg1("%-10lu ", (unsigned long) tm.n_color_transitions[i][j]);
    }
    tm_msg1("\n");
  }

  tm_msg("C }\n");
  tm_msg_enable("C", 0);
}



/**
 * API: Print phase transitions statistics.
 */
void tm_print_phase_transition_stats()
{
  int i, j;
  
  // tm_assert(sizeof(tm_phase_name) / sizeof(tm_phase_name[0]) == tm_phase_END + 1);

  tm_msg_enable("P", 1);
  tm_msg("P { phase transitions \n");

  tm_msg("P   %-10s ", "from\\to");
  for ( i = 0; i <= tm_phase_END; ++ i ) {
    tm_msg1("%-10s ", tm_phase_name[i]);
  }
  tm_msg1("\n");


  for ( i = 0; i <= tm_phase_END; ++ i ) {
    tm_msg("P   %-10s ", tm_phase_name[i]);
    for ( j = 0; j <= tm_phase_END; ++ j ) {
      tm_msg1("%-10lu ", (unsigned long) tm.n_phase_transitions[i][j]);
    }
    tm_msg1("\n");
  }

  tm_msg("P }\n");
  tm_msg_enable("P", 0);
}



/**
 * API: Print block statistics.
 */
void tm_print_block_stats()
{
  tm_type *t;

  tm_block *b;

  tm_msg_enable("X", 1);

  tm_msg("X { b tb%lu[%lu]\n",
	 tm.n[tm_B],
	 tm.n[tm_B] * tm_block_SIZE
	 );

  tm_list_LOOP(&tm.types, t);
  {
    tm_msg("X   t%p s%lu \n", t, (unsigned long) t->size);
    
    tm_list_LOOP(&t->blocks, b);
    {
      int j;

      _tm_block_validate(b);

      tm_msg("X    b%p s%lu ", (void*) b, (unsigned long) b->size);

      for ( j = 0; j < sizeof(b->n)/sizeof(b->n[0]); j ++ ) {
	tm_msg1("%c%-4lu ", tm_color_name[j][0], (unsigned long) b->n[j]);
      }
      
      tm_msg1("\n");
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;

  tm_msg("X }\n");

  tm_msg_enable("X", 0);
}


/***************************************************************************/
/* time stats support. */

void tm_time_stat_print_(tm_time_stat *ts, int flags, size_t *alloc_count_p);


/**
 * Begin timing stats collection.
 */
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


/**
 * End timing stats collection.
 */
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

  ++ ts->count;

  /* Compute dt. */
  ts->td = ts->t1 - ts->t0;
#if tm_USE_times
  ts->td += ts->t11 - ts->t01;
#endif

  /* Compute sum of dt. */
  ts->ts += ts->td;

  /* Compute avg time. */
  ts->ta = ts->ts / (double) ts->count;

  /* Compute worst time. */
  if ( (ts->tw_changed = ts->tw < ts->td) ) {
    ts->tw = ts->td;
  }

#if 0
  /* Print stats. */
  tm_time_stat_print_(ts, ts->tw_changed ? 1 : 0, 0);
#endif
}

/**
 * Print timing stats component.
 */
void tm_time_stat_print_(tm_time_stat *ts, int flags, size_t *alloc_count_p)
{
#define tv_fmt "%.7f"
#define tv_fmt_s "%8.4f"
#define tv_fmt_args(V) (double) (V)

  tm_msg("T   %-12s "
	 ,
	 ts->name);

  if ( ! (flags & (2|4)) ) {
    tm_msg1(
	   " dt " tv_fmt
	   ,
	   tv_fmt_args(ts->td)
	   );
  }

  tm_msg1(
	 " st " tv_fmt_s
	 ,
	 tv_fmt_args(ts->ts)
	 );

  if ( flags & 1 ) {
    tm_msg1(
	   " wt " tv_fmt
	   ,
	   tv_fmt_args(ts->tw)
	   );
  }

  if ( flags & 2 ) {
    tm_msg1(
	   " c %8lu"
	   , 
	   (unsigned long) ts->count
	   );
  }

  if ( flags & 4 ) {
    tm_msg1(
	    " at %.7f"
	    ,
	    (double) ts->ta
	    );
  }

  if ( alloc_count_p ) {
    tm_msg1(
	    " A %8lu"
	    ,
	    (unsigned long) *alloc_count_p
	    );
  }

  tm_msg1("\n");
}


/**
 * API: Print timing stats.
 */
void tm_print_time_stats()
{
  int i;

  tm_msg_enable("T", 1);

  tm_msg("T {\n");

  tm_time_stat_print_(&tm.ts_os_alloc, ~0, 0);
  tm_time_stat_print_(&tm.ts_os_free, ~0, 0);
  tm_time_stat_print_(&tm.ts_alloc, ~0, 0);
  tm_time_stat_print_(&tm.ts_free, ~0, 0);
  tm_time_stat_print_(&tm.ts_gc, ~0, 0);
  tm_time_stat_print_(&tm.ts_barrier, ~0, 0);
  tm_time_stat_print_(&tm.ts_barrier_black, ~0, 0);

  for ( i = 0; 
	i < (sizeof(tm.ts_phase) / sizeof(tm.ts_phase[0]));
	++ i ) {
    tm_time_stat_print_(&tm.ts_phase[i], ~0, &tm.alloc_by_phase[i]);
  }

  tm_msg("T }\n");

  tm_msg_enable("T", 0);
}


