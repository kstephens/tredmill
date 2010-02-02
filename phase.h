/** \file phase.h
 * \brief Phase.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_phase_H
#define _tredmill_phase_H

#include "stats.h"

/****************************************************************************/
/*! \defgroup phase Phase */
/*@{*/

/**
 * The phases of the allocator.
 *
 * These are similar to the phases in 
 * typical in a stop-the-world collector,
 * except the work for these phases is
 * done for short periods within tm_alloc().
 */
enum tm_phase {
  /*! Allocate nodes.         (WHITE->ECRU) */
  tm_ALLOC,
  /*! Unmark nodes.           (BLACK->ECRU) */
  tm_UNMARK,
  /*! Begin mark roots.       (ECRU->GREY)  */ 
  tm_ROOT,
  /*! Scan marked nodes.      (ECRU->GREY, GREY->BLACK) */
  tm_SCAN,
  /*! Sweep unmarked nodes.   (ECRU->WHITE) */
  tm_SWEEP,
  /*! Placeholder for size of arrays indexed by tm_phase. */
  tm_phase_END
};

typedef
struct tm_phase_data {
  /*! The current processing phase. */
  enum tm_phase phase, next_phase;

  /*! Number of transitions from one phase to another. */
  size_t n_phase_transitions[tm_phase_END + 1][tm_phase_END + 1];

  /*! Number of cumulative allocations during each phase. */
  size_t alloc_by_phase[tm_phase_END];

  /*! Time spent in each phase during tm_alloc().   */
  tm_time_stat   ts_phase[tm_phase_END]; 

  /*! Possible actions during current phase. */
  int parceling;
  int allocating;
  int marking;
  int scanning;
  int sweeping;
  int unmarking;
} tm_phase_data;

extern
const char *tm_phase_name[];

/*! Initialize phase-oriented data. */
void tm_phase_data_init(tm_phase_data *pd);

/*! Initializes a new allocation phase. */
void _tm_phase_init(int p);

/*@}*/

#endif
