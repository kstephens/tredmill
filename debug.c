/** \file debug.c
 * \brief Debugging support.
 */
#include "debug.h"
#include <stdarg.h>
#include <string.h>
#include "phase.h"   /* tm_phase_name[] */
#include "tm_data.h" /* tm. */
#include "node.h"    /* tm_node_to_ptr() */

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


/*! True if last tm_msg() was disabled; used by tm_msg1() */
int tm_msg_ignored;
/*! Table of enabled message types. */
char tm_msg_enable_table[256];

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
      memset(tm_msg_enable_table, enable, sizeof(tm_msg_enable_table));
      break;
    } else {
      tm_msg_enable_table[*r] += enable;
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
  if ( (tm_msg_ignored = ! tm_msg_enable_table[(unsigned char) format[0]]) )
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
	  ' ', /* tm_phase_name[tm.p.phase][0], */ 
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
  if ( tm_msg_ignored )
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


int _tm_sweep_is_error;


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

#if 0
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
#endif
  }
  tm_list_LOOP_END;

  /* Validate global node color counters. */
  tm_assert(n[WHITE] == tm.n[WHITE]);
  tm_assert(n[ECRU]  == tm.n[ECRU]);
  tm_assert(n[GREY]  == tm.n[GREY]);
  tm_assert(n[BLACK] == tm.n[BLACK]);
  tm_assert(n[tm_TOTAL] == tm.n[tm_TOTAL]);
}

/*@}*/

