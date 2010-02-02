/** \file debug.c
 * \brief Debugging support.
 */
#include "internal.h"

/****************************************************************************/
/* Names */

/*! Struct names. */
const char *tm_struct_name[] = {
  "FREE_BLOCK",
  "LIVE_BLOCK",
  "FREE_TYPE",
  "LIVE_TYPE",
  0
};


/****************************************************************************/
/*! \defgroup debug_message Debug Message */
/*@{*/

/*! Debug message file handle. */
FILE *tm_msg_file = 0;
const char *tm_msg_prefix = "";

/*! Default messaages to enable. */
const char *tm_msg_enable_default = 
// "sNpwtFWVAg*r"
// "gFWA"
// "FWA"
   "FW"
//  "gTFWA"
// "RgTFWA"
;

/*! Enable all messages? */
int tm_msg_enable_all = 0;


/*@}*/

/****************************************************************************/
/*! \defgroup debugging Debugging */
/*@{*/


void tm_msg_init()
{
  /*! Initialize tm_msg() ignore table. */
  if ( tm_msg_enable_all ) {
    tm_msg_enable("\1", 1);
  } else {
    tm_msg_enable(tm_msg_enable_default, 1);
    tm_msg_enable(" \t\n\r", 1);
  }

  tm_msg_enable("WF", 1);
  // tm_msg_enable("b", 1);
}


/**
 * Enable or disable messages.
 *
 * Messages correspond the first character in the format string of tm_msg().
 */
void tm_msg_enable(const char *codes, int enable)
{
  const unsigned char *r;
 
  enable = enable ? 1 : -1;
  for ( r = (const unsigned char *) codes; *r; ++ r ) {
    /* '\1' enables all codes. */
    if ( *r == '\1' ) {
      memset(tm.msg_enable_table, enable, sizeof(tm.msg_enable_table));
      break;
    } else {
      tm.msg_enable_table[*r] += enable;
    }
  }
}

/**
 * Prints messages to tm_msg_file.
 */
void tm_msg(const char *format, ...)
{
  va_list vap;

  /* Determine if we need to ignore the msg based on the first char in the format. */
  if ( (tm.msg_ignored = ! tm.msg_enable_table[(unsigned char) format[0]]) )
    return;

  /*! Returns if format is empty. */
  if ( ! *format )
    return;

  /*! Default tm_msg_file to stderr. */
  if ( ! tm_msg_file )
    tm_msg_file = stderr;

  /*! Print header. */
  fprintf(tm_msg_file, "%s%s%c %6lu t#%d[%lu] ",
	  tm_msg_prefix,
	  *tm_msg_prefix ? " " : "",
	  tm_phase_name[tm.p.phase][0], 
	  (unsigned long) tm.alloc_id,
	  tm.alloc_request_type ? tm.alloc_request_type->id : 0,
	  (unsigned long) tm.alloc_request_size);

  /*! Print allocation pass if > 1 */
  if ( tm.alloc_pass > 1 )
    fprintf(tm_msg_file, "(pass %lu) ", (unsigned long) tm.alloc_pass);

  /*! Print msg. */
  va_start(vap, format);
  vfprintf(tm_msg_file, format, vap);
  va_end(vap);

  /*! Flush IO buffers. */
  fflush(tm_msg_file);
  // fgetc(stdin);
}


/**
 * Format additional message data.
 */
void tm_msg1(const char *format, ...)
{
  va_list vap;

  /* Don't bother if last msg was ignored. */
  if ( tm.msg_ignored )
    return;

  if ( ! *format )
    return;

  va_start(vap, format);
  vfprintf(tm_msg_file, format, vap);
  va_end(vap);

  fflush(tm_msg_file);
}


/*@}*/

/****************************************************************************/
/* \defgroup error_general Error: General */
/*@{*/


/**
 * Put a debugger break point here!
 */
void tm_stop()
{
}

/**
 * Fatal, non-recoverable internal error.
 */
void tm_fatal()
{
  tm_msg("Fatal Aborting!\n");

  tm_stop();

  abort();
}

/**
 * Abort after printing stats.
 */
void tm_abort()
{
  static int in_abort; /* THREAD? */

  ++ in_abort;

  if ( in_abort == 1 && tm.inited ) {
    tm_print_stats();
  }

  -- in_abort;

  tm_fatal();
}

/*@}*/

/**************************************************************************/
/*! \defgroup assertion Assertion, Warning */
/*@{*/

/**
 * Format assertion error.
 */
void _tm_assert(const char *expr, const char *file, int lineno)
{
  tm_msg("\n");
  tm_msg("Fatal assertion \"%s\" failed %s:%d ", expr, file, lineno);
}

/*@}*/

/**************************************************************************/
/*! \defgroup validiation Validation */
/*@{*/


/**
 * Validate internal lists against bookkeeping stats.
 *
 * This is expensive.
 */
void tm_validate_lists()
{
  int j;
  tm_type *t;
  tm_block *b;
  tm_node *node;
  size_t n[tm__LAST2];
  size_t bn[tm__LAST2];
  size_t tn[tm__LAST2];

  memset(n, 0, sizeof(n));
  memset(bn, 0, sizeof(bn));

#if 0
  fprintf(tm_msg_file, "V");
  fflush(tm_msg_file);
#endif

  /* Validate free block list. */
  tm_assert(tm_list_color(&tm.free_blocks) == tm_FREE_BLOCK);
  tm_list_LOOP(&tm.free_blocks, b);
  {
    _tm_block_validate(b);
    tm_assert(tm_list_color(b) == tm_FREE_BLOCK);
    tm_assert(b->type == 0);
    tm_assert(b->next_parcel == b->begin);
  }
  tm_list_LOOP_END;

  /* Validate types. */
  tm_assert(tm_list_color(&tm.types) == tm_LIVE_TYPE);
  tm_list_LOOP(&tm.types, t);
  {
    tm_assert(tm_list_color(t) == tm_LIVE_TYPE);

    /* Validate type totals. */
    memset(tn, 0, sizeof(n));

    /* Validate type blocks. */
    bn[tm_B] = 0;
    tm_assert(tm_list_color(&t->blocks) == tm_LIVE_BLOCK);
    tm_list_LOOP(&t->blocks, b);
    {
      _tm_block_validate(b);
      tm_assert(tm_list_color(b) == tm_LIVE_BLOCK);
      tm_assert(b->type == t);
      ++ bn[tm_B];
    }
    tm_list_LOOP_END;
    tm_assert(bn[tm_B] == t->n[tm_B]);

    /* Validate colored node lists. */
    for ( j = 0; 
	  j < sizeof(t->color_list) / sizeof(t->color_list[0]); 
	  ++ j ) {
      /* Validate lists. */
      tm_assert(tm_list_color(&t->color_list[j]) == j);
      tm_list_LOOP(&t->color_list[j], node);
      {
	tm_assert(tm_node_color(node) == j);
	++ tn[j];
      }
      tm_list_LOOP_END;

      tm_assert(t->n[j] == tn[j]);
      tn[tm_TOTAL] += tn[j];
      n[j] += tn[j];
      n[tm_TOTAL] += tn[j];
    }
    tm_assert(t->n[tm_TOTAL] == tn[tm_TOTAL]);
  }
  tm_list_LOOP_END;

  /* Validate global node color counters. */
  tm_assert(n[tm_WHITE] == tm.n[tm_WHITE]);
  tm_assert(n[tm_ECRU]  == tm.n[tm_ECRU]);
  tm_assert(n[tm_GREY]  == tm.n[tm_GREY]);
  tm_assert(n[tm_BLACK] == tm.n[tm_BLACK]);
  tm_assert(n[tm_TOTAL] == tm.n[tm_TOTAL]);
}

/*@}*/

/***************************************************************************/
/* \defgroup error_sweep Error: Sweep */
/*@{*/


#ifndef _tm_sweep_is_error
/*! If true and sweep occurs, raise a error. */
int _tm_sweep_is_error = 0;
#endif

/**
 * Checks for a unexpected node sweep.
 *
 * If a sweep occured at this time,
 * the mark phase created a free tm_node (tm_WHITE) when it should not have.
 *
 * TM failed to mark all tm_nodes as in-use (tm_GREY, or tm_BLACK).
 *
 * This can be considered a bug in:
 * - The write barrier.
 * - Mutators that failed to use the write barrier.
 * - Locating nodes for potential pointers.
 * - Other book-keeping or list management.
 */
int _tm_check_sweep_error()
{
  tm_type *t;
  tm_node *n;

  /*! OK: A sweep is not considered an error. */
  if ( ! _tm_sweep_is_error ) {
    return 0;
  }

  /*! OK: Nothing was left unmarked. */
  if ( ! tm.n[tm_ECRU] ) {
    return 0;
  }

  /*! ERROR: There were some unmarked (tm_ECRU) tm_nodes. */
  tm_msg("Fatal %lu dead nodes; there should be no sweeping.\n", tm.n[tm_ECRU]);
  tm_stop();
  // tm_validate_lists();
  
  /*! Print each unmarked node. */
  tm_list_LOOP(&tm.types, t);
  {
    tm_list_LOOP(&t->color_list[tm_ECRU], n);
    {
      tm_assert_test(tm_node_color(n) == tm_ECRU);
      tm_msg("Fatal node %p color %s size %lu should not be sweeped!\n", 
	     (void*) n, 
	     tm_color_name[tm_node_color(n)], 
	     (unsigned long) t->size);
      {
	void ** vpa = tm_node_to_ptr(n);
	tm_msg("Fatal cons (%d, %p)\n", ((long) vpa[0]) >> 2, vpa[1]);
      }
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;
  
  /*! Print stats. */
  tm_print_stats();
  
  /*! Attempt to mark all roots. */
  _tm_phase_init(tm_ROOT);
  _tm_root_scan_all();
  
  /*! Scan all marked nodes. */
  _tm_phase_init(tm_SCAN);
  _tm_node_scan_all();
  
  /*! Was there still unmarked nodes? */
  if ( tm.n[tm_ECRU] ) {
    tm_msg("Fatal after root mark: still missing %lu references.\n",
	   (unsigned long) tm.n[tm_ECRU]);
  } else {
    tm_msg("Fatal after root mark: OK, missing references found!\n");
  }
  
  /*! Mark all the tm_ECRU nodes tm_BLACK */
  tm_list_LOOP(&tm.types, t);
  {
    tm_list_LOOP(&t->color_list[tm_ECRU], n);
    {
      tm_node_set_color(n, tm_node_to_block(n), tm_BLACK);
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;
  
  /*! Start tm_UNMARK phase. */
  _tm_phase_init(tm_UNMARK);
  
  /*! Assert there is no unmarked nodes. */
  tm_assert_test(tm.n[tm_ECRU] == 0);
  
  /*! Stop in debugger? */
  tm_stop();
  
  /*! Clear worst alloc time. */
  memset(&tm.ts_alloc.tw, 0, sizeof(tm.ts_alloc.tw));
  
  /*! Return 1 to signal an error. */
  return 1;
}


/*@}*/
