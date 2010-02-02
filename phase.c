/** \file phase.c
 * \brief Phase
 *
 * - $Id: tm.c,v 1.20 2009-08-01 10:47:31 stephens Exp $
 */

#include "internal.h"
#include "phase.h"


/****************************************************************************/
/*! \defgroup phase Phase */
/*@{*/


/*! Phase names. */
const char *tm_phase_name[] = {
  "ALLOC",  /* tm_ALLOC */
  "UNMARK", /* tm_UNMARK */
  "ROOTS",  /* tm_ROOT */
  "SCAN",   /* tm_SCAN */
  "SWEEP",  /* tm_SWEEP */
  "Total",  /* tm_phase_END */
};


void tm_phase_data_init(tm_phase_data *pd)
{
  int i;

  memset(pd, 0, sizeof(*pd));

  for ( i = 0; i < (sizeof(pd->ts_phase) / sizeof(pd->ts_phase[0])); ++ i ) {
    pd->ts_phase[i].name = tm_phase_name[i];
  }
}


/**
 * Initialize the collection/allocation phase.
 */
void _tm_phase_init(int p)
{
  tm_msg("p %s\n", tm_phase_name[p]);

#if 0
  /* DEBUG: DELETE ME! */
  if ( tm.alloc_id == 1987 )
    tm_stop();
#endif

  ++ tm.p.n_phase_transitions[tm.p.phase][p];
  ++ tm.p.n_phase_transitions[tm.p.phase][tm_phase_END];
  ++ tm.p.n_phase_transitions[tm_phase_END][p];
  ++ tm.p.n_phase_transitions[tm_phase_END][tm_phase_END];

  if ( tm.p.phase == tm_SWEEP && tm.p.phase != p ) {
    tm.alloc_since_sweep = 0;
  }

#if 0
  fprintf(stderr, "\n%s->%s #%lu %lu %lu %lu %lu\n", tm_phase_name[tm.phase], tm_phase_name[p], (unsigned long) tm.alloc_id,
	  (unsigned long) tm.n[tm_WHITE],
	  (unsigned long) tm.n[tm_ECRU],
	  (unsigned long) tm.n[tm_GREY],
	  (unsigned long) tm.n[tm_BLACK],
	  0
	  );
#endif

  switch ( (tm.p.phase = p) ) {
  case tm_ALLOC:
    /**
     * - tm_ALLOC: allocate nodes until memory pressure is high.
     */
    tm.p.allocating = 1;
    tm.p.unmarking = 0;
    tm.p.marking = 0;
    tm.p.scanning = 0;
    tm.p.sweeping = 0;

    tm.alloc_color = tm_DEFAULT_ALLOC_COLOR;

    break;

  case tm_UNMARK:
    /**
     * - tm_UNMARK: begin unmarking tm_BLACK nodes as tm_ECRU:
     */
    tm.p.allocating = 0;
    tm.p.unmarking = 1;
    tm.p.marking = 0;
    tm.p.scanning = 0;
    tm.p.sweeping = 0;

    tm.alloc_color = tm_DEFAULT_ALLOC_COLOR;

    /*! Set up for unmarking. */
    tm_node_LOOP_INIT(tm_BLACK);

    /*! Keep track of how many tm_BLACK nodes are in use. */
    tm.n[tm_NU] = tm.n[tm_BLACK];
    break;

  case tm_ROOT:
    /**
     * - tm_ROOT: begin scanning roots for potential pointers:
     */
    tm.p.allocating = 0;
    tm.p.unmarking = 0;
    tm.p.marking = 1;
    tm.p.scanning = 0;
    tm.p.sweeping = 0;

    tm.alloc_color = tm_DEFAULT_ALLOC_COLOR;

    /*! Mark stack and data roots as un-mutated. */
    tm.stack_mutations = tm.data_mutations = 0;

    /*! Initialize root mark loop. */
    _tm_root_loop_init();
    break;

  case tm_SCAN:
    /**
     * - tm_SCAN: begin scanning tm_GREY nodes for internal pointers:
     */
    tm.p.allocating = 0;
    tm.p.unmarking = 0;
    tm.p.marking = 1;
    tm.p.scanning = 1;
    tm.p.sweeping = 0;

    tm.alloc_color = tm_DEFAULT_ALLOC_COLOR;

    /*! Mark stack and data roots as un-mutated. */
    tm.stack_mutations = tm.data_mutations = 0;

    /*! Set up for marking. */
    tm_node_LOOP_INIT(tm_GREY);
    break;

  case tm_SWEEP:
    /**
     * - tm_SWEEP: begin reclaiming any ECRU nodes as WHITE.
     */
    tm.p.allocating = 0;
    tm.p.unmarking = 0;
    tm.p.marking = 0;
    tm.p.scanning = 0;
    tm.p.sweeping = 1;

    tm.alloc_color = tm_SWEEP_ALLOC_COLOR;

    tm_assert_test(tm.n[tm_GREY] == 0);

    /* Set up for scanning. */
    // tm_node_LOOP_INIT(tm_GREY);

    /* Set up for sweeping. */
    // tm_node_LOOP_INIT(tm_ECRU);
    break;

  default:
    tm_fatal();
    break;
  }

  if ( 1 || p == tm_SWEEP ) {
    // tm_print_stats();
  }
  //tm_validate_lists();
}


/*@}*/

