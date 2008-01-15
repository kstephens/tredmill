#include "internal.h"

/****************************************************************************/
/* Names */

const char *tm_color_name[] = {
  "WHITE", /* tm_WHITE */
  "ECRU",  /* tm_ECRU */
  "GREY",  /* tm_GREY */
  "BLACK", /* tm_BLACK */

  "TOTAL", /* tm_TOTAL */

  "b",     /* tm_B */
  "n",     /* tm_NU */
  "u",     /* tm_b */
  "p",     /* tm_b_NU */

  "O",     /* tm_B_OS */
  "o",     /* tm_b_OS */
  "P",     /* tm_B_OS_M */
  "p",     /* tm_b_OS_M */
  0
};

const char *tm_struct_name[] = {
  "FREE_BLOCK",
  "LIVE_BLOCK",
  "FREE_TYPE",
  "LIVE_TYPE",
  0
};

const char *tm_phase_name[] = {
  "ALLOC",  /* tm_ALLOC */
  "ROOTS",  /* tm_ROOT */
  "SCAN",   /* tm_SCAN */
  "SWEEP",  /* tm_SWEEP */
  0
};



/****************************************************************************/
/* Debug messages. */

FILE *tm_msg_file = 0;
const char *tm_msg_prefix = "";

const char *tm_msg_enable_default = 
// "sNpwtFWVAg*r"
   "gFWA"
//  "gTFWA"
// "RgTFWA"
;

int tm_msg_enable_all = 0;


/****************************************************************************/
/* Debugging. */


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


void tm_msg(const char *format, ...)
{
  va_list vap;

  /* Determine if we need to ignore the msg based on the first char in the format. */
  if ( (tm.msg_ignored = ! tm.msg_enable_table[(unsigned char) format[0]]) )
    return;

  if ( ! *format )
    return;

  /* Default tm_msg_file. */
  if ( ! tm_msg_file )
    tm_msg_file = stderr;

  /* Print header. */
  fprintf(tm_msg_file, "%s%s%c %6lu t#%d[%lu] ",
	  tm_msg_prefix,
	  *tm_msg_prefix ? " " : "",
	  tm_phase_name[tm.phase][0], 
	  (unsigned long) tm.alloc_id,
	  tm.alloc_request_type ? tm.alloc_request_type->id : 0,
	  (unsigned long) tm.alloc_request_size);

  if ( tm.alloc_pass > 1 )
    fprintf(tm_msg_file, "(pass %lu) ", (unsigned long) tm.alloc_pass);

  /* Print msg. */
  va_start(vap, format);
  vfprintf(tm_msg_file, format, vap);
  va_end(vap);

  fflush(tm_msg_file);
  // fgetc(stdin);
}


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


/****************************************************************************/
/* Generalize error handling. */


void tm_stop() /* put a debugger break point here! */
{
}


void tm_fatal()
{
  tm_msg("Fatal Aborting!\n");

  tm_stop();

  abort();
}


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


/**************************************************************************/
/* Assertions, warnings. */

void _tm_assert(const char *expr, const char *file, int lineno)
{
  tm_msg("\n");
  tm_msg("Fatal assertion \"%s\" failed %s:%d ", expr, file, lineno);
}


/**************************************************************************/
/* Validations. */


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
    tm_assert(b->alloc == b->begin);
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


/***************************************************************************/
/* Sweeping */


#ifndef _tm_sweep_is_error
int _tm_sweep_is_error = 0;
#endif

int _tm_check_sweep_error()
{
  tm_type *t;
  tm_node *n;

  if ( ! _tm_sweep_is_error ) {
    return 0;
  }

  if ( ! tm.n[tm_ECRU] ) {
    return 0;
  }

  tm_msg("Fatal %lu dead nodes; there should be no sweeping.\n", tm.n[tm_ECRU]);
  tm_stop();
  // tm_validate_lists();
  
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
	tm_msg("Fatal cons (%d, %p)\n", ((int) vpa[0]) >> 2, vpa[1]);
	}
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;
  
  tm_print_stats();
  
  /* Attempting to mark all roots. */
  _tm_phase_init(tm_ROOT);
  _tm_root_scan_all();
  
  /* Scan all marked nodes. */
  _tm_phase_init(tm_SCAN);
  _tm_node_scan_all();
  
  if ( tm.n[tm_ECRU] ) {
    tm_msg("Fatal after root mark: still missing %lu references.\n",
	   (unsigned long) tm.n[tm_ECRU]);
  } else {
    tm_msg("Fatal after root mark: OK, missing references found!\n");
  }
  
  tm_list_LOOP(&tm.types, t);
  {
    tm_list_LOOP(&t->color_list[tm_ECRU], n);
    {
      tm_node_set_color(n, tm_node_to_block(n), tm_BLACK);
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;
  
  _tm_phase_init(tm_ALLOC);
  tm_assert_test(tm.n[tm_ECRU] == 0);
  
  tm_stop();
  
  /* Clear worst alloc time. */
  memset(&tm.ts_alloc.tw, 0, sizeof(tm.ts_alloc.tw));
  
  return 1;
}


