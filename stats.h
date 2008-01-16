#ifndef tm_STATS_H
#define tm_STATS_H

/****************************************************************************/
/* Stats. */

typedef struct tm_time_stat {
  const char *name;
  double 
    td, /* Last time. */
    ts, /* Total time. */
    tw, /* Worst time. */
    ta, /* Avg time. */
    t0, t1, 
    t01, t11;
  short tw_changed;
  unsigned int count;
} tm_time_stat;

void tm_time_stat_begin(tm_time_stat *ts);
void tm_time_stat_end(tm_time_stat *ts);

void tm_print_stats();
void tm_print_block_stats();
void tm_print_time_stats();

#endif
