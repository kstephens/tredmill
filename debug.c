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
  "UNMARK", /* tm_UNMARK */
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

  /* Default tm_msg_file. */
  if ( ! tm_msg_file )
    tm_msg_file = stderr;

  if ( ! *format )
    return;

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
/* assertions, warnings. */

void _tm_assert(const char *expr, const char *file, int lineno)
{
  tm_msg("\n");
  tm_msg("Fatal assertion \"%s\" failed %s:%d ", expr, file, lineno);
}


