#include "internal.h"

/***************************************************************************/
/* root scan */

long tm_root_scan_some_size = 512;

void _tm_root_loop_init()
{
  tm.rooti = tm.root_datai;
  tm.rp = tm.roots[tm.rooti].l;
  tm.data_mutations = tm.stack_mutations = 0;
}


static
void _tm_root_scan_id(int i)
{
  if ( tm.roots[i].l < tm.roots[i].h ) {
    tm_msg("r [%p,%p] %s\n", tm.roots[i].l, tm.roots[i].h, tm.roots[i].name);
    _tm_range_scan(tm.roots[i].l, tm.roots[i].h);
  }
}


void _tm_register_scan()
{
  _tm_root_scan_id(0);
}


void _tm_set_stack_ptr(void *stackvar)
{
  *tm.stack_ptrp = (char*) stackvar - 64;
}


void _tm_stack_scan()
{
  _tm_register_scan();
  _tm_root_scan_id(1);
  tm.stack_mutations = 0;
}


void _tm_root_scan_all()
{
  int i;

  tm_msg("r G%lu B%lu {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  for ( i = 0; tm.roots[i].name; ++ i ) {
    _tm_root_scan_id(i);
  }
  tm.data_mutations = tm.stack_mutations = 0;
  _tm_root_loop_init();
#if 0
  tm_msg("r G%lu B%lu }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
#endif
}


int _tm_root_scan_some()
{
  int result = 1;
  long left = tm_root_scan_some_size;

  tm_msg("r G%lu B%lu {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  tm_msg("r [%p,%p] %s\n", 
	 tm.roots[tm.rooti].l, 
	 tm.roots[tm.rooti].h,
	 tm.roots[tm.rooti].name 
	 );

  do {
    /* Try marking some roots. */
    while ( (void*) (tm.rp + sizeof(void*)) >= tm.roots[tm.rooti].h ) {
      ++ tm.rooti;
      if ( ! tm.roots[tm.rooti].name ) {
	_tm_root_loop_init();
	tm_msg("r done\n");

	result = 0;
	goto done;
      }

      tm_msg("r [%p,%p] %s\n", 
	     tm.roots[tm.rooti].l, 
	     tm.roots[tm.rooti].h,
	     tm.roots[tm.rooti].name);
      
      tm.rp = tm.roots[tm.rooti].l;
    }

    _tm_mark_possible_ptr(* (void**) tm.rp);

    tm.rp += tm_PTR_ALIGN;

    left -= tm_PTR_ALIGN;
  } while ( left > 0 );

 done:
#if 0
  tm_msg("r G%lu B%lu }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
#endif

  return result; /* We're not done. */
}


/***************************************************************************/
/* Marking */


void _tm_range_scan(const void *b, const void *e)
{
  const char *p;

  for ( p = b; 
	((char*) p + sizeof(void*)) <= (char*) e; 
	p += tm_PTR_ALIGN ) {
#if 0
    tm_node *n = 
#endif
      _tm_mark_possible_ptr(* (void**) p);

#if 0
    if ( n ) {
      tm_msg("M n%p p%p\n", n, p);
    }
#endif
  }
}


static __inline
void _tm_node_mark_and_scan_interior(tm_node *n, tm_block *b)
{
  tm_assert_test(tm_node_to_block(n) == b);
  tm_assert_test(tm_node_color(n) == tm_GREY);

  /* Move to marked (BLACK) list. */
  /*
   * Do this first, before marking roots.
   *
   * This will insure that
   * this node is mutated during
   * interior pointer scanning,
   * the node will be put back on
   * the GREY list by the write barrier.
   */
  tm_node_set_color(n, b, tm_BLACK);

  /* Mark all possible pointers. */
  {
    const char *ptr = tm_node_to_ptr(n);
    _tm_range_scan(ptr, ptr + b->type->size);
  }
}


size_t _tm_node_scan_some(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_GREY] ) {
    tm_node_LOOP(tm_GREY);
    {
      tm_assert_test(tm_node_color(n) == tm_GREY);

      _tm_node_mark_and_scan_interior(n, tm_node_to_block(n));

      ++ count;
      bytes += t->size;

      if ( (left -= t->size) <= 0 ) {
	tm_node_LOOP_BREAK(tm_GREY);
      }
    }
    tm_node_LOOP_END(tm_GREY);
  }

#if 0
  if ( count )
    tm_msg("M c%lu b%lu l%lu\n", count, bytes, tm.n[tm_GREY]);
#endif

  return tm.n[tm_GREY];
}


void _tm_node_scan_all()
{
  while ( tm.n[tm_GREY] ) {
    tm_node_LOOP_INIT(tm_GREY);
    _tm_node_scan_some(tm.n[tm_TOTAL]);
  }
}


