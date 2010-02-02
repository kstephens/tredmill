/** \file stats.h
 * \brief Statistics.
 */
#ifndef tm_STATS_H
#define tm_STATS_H

/****************************************************************************/

/**
 * Timing statstics.
 */
typedef struct tm_time_stat {
  const char *name;   /*!< The name for this statistic. */
  double td;          /*!< Last time in seconds. */
  double ts;          /*!< Total time in seconds. */
  double tw;          /*!< Worst time in seconds. */
  double ta;          /*!< Average time in seconds. */
  double t0;          /*!< Absolute time at tm_time_stat_begin(). */
  double t1;          /*!< Absolute time at tm_time_stat_end(). */ 
  double t01;         /*!< Aux time at tm_time_stat_begin(). */
  double t11;         /*!< Aux time at tm_time_stat_end(). */
  short tw_changed;   /*!< Time worst changed? */
  unsigned int count; /*!< Total calls to tm_time_stat_end(). */
} tm_time_stat;

void tm_time_stat_begin(tm_time_stat *ts);
void tm_time_stat_end(tm_time_stat *ts);

void tm_print_stats();
void tm_print_block_stats();
void tm_print_time_stats();

void tm_print_color_transition_stats();
void tm_print_phase_transition_stats();

void tm_gc_clear_stats();

#endif
