#include "internal.h"

const char *tm_alloc_log_name = 0;
FILE *tm_alloc_log_fh;

static unsigned long log_id = 0;
static unsigned long log_ratio = 1;

void tm_alloc_log_init()
{
  if ( ! tm_alloc_log_name ) {
    tm_alloc_log_name = getenv("TM_ALLOC_LOG");
  }

  if ( ! tm_alloc_log_fh ) {
    tm_alloc_log_fh = fopen(tm_alloc_log_name, "w+");
    if ( ! tm_alloc_log_fh ) {
      tm_abort();
    }
    fprintf(tm_alloc_log_fh, "#ID PTR WHITE ECRU GREY BLACK PHASE BLOCKS FREE_BLOCKS\n");
  }
}


void tm_alloc_log(void *ptr)
{
  if ( tm_alloc_log_fh ) {
    if ( log_id ++ % log_ratio == 0 ) {
      fprintf(tm_alloc_log_fh,
	      "%lu %lu %lu %lu %lu %lu %lu %lu %lu, %lu\n",  
	      (unsigned long) tm.alloc_id,
	      (unsigned long) ptr,
	      (unsigned long) tm.n[tm_WHITE],
	      (unsigned long) tm.n[tm_ECRU],
	      (unsigned long) tm.n[tm_GREY],
	      (unsigned long) tm.n[tm_BLACK],
	      (unsigned long) tm.n[tm_TOTAL],
	      (unsigned long) tm.p.phase,
	      (unsigned long) tm.n[tm_B],
	      (unsigned long) tm.free_blocks_n
	      );
    }
    if ( log_ratio < 1000 && log_id / log_ratio > 100 ) {
      log_ratio *= 10;
    }
  }
}
