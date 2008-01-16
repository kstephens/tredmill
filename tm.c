/* $Id: tm.c,v 1.19 2008-01-16 04:14:07 stephens Exp $ */

#include "tm.h"
#include "internal.h"

/****************************************************************************/
/* User control vars. */

long tm_node_alloc_some_size = 8;  /* Nodes to alloc from block. */
long tm_node_scan_some_size = 96;  /* Words to scan per alloc. */
long tm_node_sweep_some_size = 8;  /* Nodes to sweep per alloc. */
long tm_node_unmark_some_size = 8; /* Nodes to unmark per alloc. */

long tm_block_sweep_some_size = 2;

size_t tm_os_alloc_max = 64 * 1024 * 1024;

/* If true all root are scanned atomically. */
int    tm_root_scan_full = 1;


/****************************************************************************/
/* Safety */

void tm_validate_lists();


/**************************************************************************/
/* Allocation colors. */


/*
This table defines what color a newly allocated node
should be during the current allocation phase.
*/

#if 1
 /*
 ** Default allocated nodes to "not marked".
 ** Rational: some are allocated but thrown away early.
 */
#define tm_DEFAULT_ALLOC_COLOR tm_ECRU
#else
 /*
 ** Default allocated nodes to "scheduled, not marked".
 ** Rational: they are allocated because they are to be used (referred to).
 ** This seems to cause it to keep allocating new pages before sweeping.
 */
#define tm_DEFAULT_ALLOC_COLOR tm_GREY
#endif


#if 1
#define tm_SWEEP_ALLOC_COLOR tm_GREY
#else
#define tm_SWEEP_ALLOC_COLOR tm_BLACK
#endif

static const tm_color tm_phase_alloc[] = {
  tm_DEFAULT_ALLOC_COLOR,  /* tm_ALLOC */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_ROOT */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_SCAN */
  tm_SWEEP_ALLOC_COLOR,    /* tm_SWEEP */
  -1
};

#undef tm_DEFAULT_ALLOC_COLOR

/**************************************************************************/
/* Global allocator data */

struct tm_data tm;

/****************************************************************************/
/* Phase support. */


static 
int tm_stack_growth(char *ptr, int depth)
{
  char *top_of_stack = (char*) &top_of_stack;
  if ( depth ) {
    return tm_stack_growth(ptr, depth - 1);
  }
  return top_of_stack - ptr;
}


void _tm_phase_init(int p)
{
  tm_msg("p %s\n", tm_phase_name[p]);

#if 0
  if ( tm.alloc_id == 1987 )
    tm_stop();

#endif

  switch ( (tm.phase = p) ) {
  case tm_ALLOC:
    tm.marking = 0;
    tm.scanning = 0;
    tm.sweeping = 0;
    tm.unmarking = 1;

    /* Set up for unmarking. */
    // tm_node_LOOP_INIT(tm_BLACK);

    /* Keep track of how many nodes are in use. */
    tm.n[tm_NU] = tm.n[tm_BLACK];

    /* Write barrier. */
    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_ROOT:
    tm.marking = 1;
    tm.scanning = 0;
    tm.sweeping = 0;
    tm.unmarking = 0;

    tm.stack_mutations = tm.data_mutations = 0;

    /* Initialize root mark loop. */
    _tm_root_loop_init();

    /* Write barrier. */
    _tm_write_root = __tm_write_root_root;
    _tm_write_barrier = __tm_write_barrier_root;
    _tm_write_barrier_pure = __tm_write_barrier_pure_root;
    break;

  case tm_SCAN:
    tm.marking = 1;
    tm.scanning = 1;
    tm.sweeping = 0;
    tm.unmarking = 0;

    tm.stack_mutations = tm.data_mutations = 0;

    /* Set up for marking. */
    // tm_node_LOOP_INIT(tm_GREY);

    /* Write barrier. */
    _tm_write_root = __tm_write_root_mark;
    _tm_write_barrier = __tm_write_barrier_mark;
    _tm_write_barrier_pure = __tm_write_barrier_pure_mark;
    break;

  case tm_SWEEP:
    tm.marking = 1;
    tm.scanning = 1;
    tm.sweeping = 1;
    tm.unmarking = 0;

    tm_assert_test(tm.n[tm_GREY] == 0);

    /* Set up for scanning. */
    // tm_node_LOOP_INIT(tm_GREY);

    /* Set up for sweeping. */
    // tm_node_LOOP_INIT(tm_ECRU);

    /* Write barrier. */
    /* Recolor mutated BLACK nodes to GREY for rescanning. */
    _tm_write_root = __tm_write_root_sweep;
    _tm_write_barrier = __tm_write_barrier_sweep;
    _tm_write_barrier_pure = __tm_write_barrier_pure_sweep;
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


static __inline
void _tm_block_sweep_init()
{
#if 0
  tm.bt = tm_list_next(&tm.types);
  tm.bb = tm.bt != tm_list_next(tm.bt) ?
    tm_list_next(&tm.bt->blocks) :
    0;
#endif
}



/****************************************************************************/
/* Init. */

static __inline
void tm_type_init(tm_type *t, size_t size);


void tm_init(int *argcp, char ***argvp, char ***envpp)
{
  int i;

  tm_assert(sizeof(unsigned long) == sizeof(void *));

  /* Time stat names, */
  tm.ts_os_alloc.name = "tm_os_alloc";
  tm.ts_os_free.name = "tm_os_free";
  tm.ts_alloc.name = "tm_malloc";
  tm.ts_free.name = "tm_free";
  tm.ts_gc.name = "gc";
  tm.ts_barrier.name = "tm_barrier";
  tm.ts_barrier_black.name = "tm_barrier B";

  for ( i = 0; i < (sizeof(tm.ts_phase) / sizeof(tm.ts_phase[0])); ++ i ) {
    tm.ts_phase[i].name = tm_phase_name[i];
  }

  /* Msg ignore table. */
  if ( tm_msg_enable_all ) {
    tm_msg_enable("\1", 1);
  } else {
    tm_msg_enable(tm_msg_enable_default, 1);
    tm_msg_enable(" \t\n\r", 1);
  }

  tm_msg_enable("WF", 1);
  // tm_msg_enable("b", 1);

  if ( tm.inited ) {
    tm_msg("WARNING: tm_init() called more than once.\nf");
  }
  tm.initing ++;

  if ( ! argcp || ! argvp ) {
    tm_msg("WARNING: tm_init() not called from main().\n");
  }

  if ( ! envpp ) {
    extern char **environ;
    tm_msg("WARNING: tm_init(): not passed &envp.\n");
    envpp = (char ***) &environ;
  }

#if 0
#define P(X) tm_msg(" %s = %ld\n", #X, (long) X);
  P(sizeof(tm_list));
  P(sizeof(tm_node));
  P(sizeof(tm_block));
  P(sizeof(tm_type));
  P(sizeof(struct tm_data));
#undef P
#endif

  /* Possible pointer range. */
  tm_ptr_l = (void*) ~0UL;
  tm_ptr_h = 0;

  /* Roots. */
  tm.nroots = 0;
  tm.data_mutations = tm.stack_mutations = 0;

  tm.root_datai = -1;

  /* Roots: register set. */
  /* A C jmpbuf struct contains the saved registers set, hopefully. */
  tm_root_add("register", &tm.jb, (&tm.jb) + 1);

  /* Roots: stack */

  {
    void *bottom_of_stack, *top_of_stack = (void*) &i;

#ifndef tm_ENVIRON_0_ALLOCATED_ON_STACK
#define tm_ENVIRON_0_ALLOCATED_ON_STACK 1
#endif

#if tm_ENVIRON_0_ALLOCATED_ON_STACK
    {
      extern char **environ;
      bottom_of_stack = (void*) environ[0];
    }
#else
  /* argvp contains a caller's auto variable. */
  /* Hope that we are being called from somewhere close to the bottom of the stack. */
    bottom_of_stack = (void*) argvp;
#endif

    {
      void *l = top_of_stack;
      void *h = bottom_of_stack;
      
      /* Determine direction of stack growth. */
      if ( tm_stack_growth((char*) &l, 5) < 0 ) {
	tm.stack_grows = -1;
      } else {
	void *t = l;
	l = h;
	h = t;
	tm.stack_grows = 1;
      }
      
      /* Attempt to nudge to page boundaries. */
      {
	size_t stack_page_size = 4096;
	l = (void*) ((unsigned long) (l) - ((unsigned long) (l) % stack_page_size));
	h = (void*) ((unsigned long) (h) + (stack_page_size - ((unsigned long) (h) % stack_page_size)));
      }

      i = tm_root_add("stack", l, h);
    }

    /* Remember where to put the stack pointer. */
    if ( tm.stack_grows < 0 ) {
      tm.stack_ptrp = (void**) &tm.roots[i].l;
    } else {
      tm.stack_ptrp = (void**) &tm.roots[i].h;
    }

    tm.root_datai = i + 1;

    /* IMPLEMENT: Support for multithreading stacks. */
  }


  /* Anti-roots: do not scan tm's internal data structures. */
  tm_root_remove("tm", &tm, &tm + 1);

  /* Roots: initialized, and zeroed data segments. */
#ifdef __win32__
  {
    extern int _data_start__, _data_end__, _bss_start__, _bss_end__;

    tm_root_add("initialized data", &_data_start__, &_data_end__);
    tm_root_add("uninitialized data", &_bss_start__, &_bss_end__);

  }
#define tm_roots_data_segs
#endif

#ifdef __linux__
  {
    extern int __data_start, __bss_start, _end;

    tm_assert(&__data_start < &__bss_start && &__bss_start < &_end);
    tm_root_add("initialized data", &__data_start, &__bss_start);
    tm_root_add("uninitialized data", &__bss_start, &_end);
  }
#define tm_roots_data_segs
#endif

#ifndef tm_roots_data_segs
#error must specify how to find the data segment(s) for root marking.
#endif


  /* IMPLEMENT: Support dynamically-loaded library data segments. */

  /* Dump the root sets. */
  tm_msg_enable("R", 1);

  tm_msg("R ROOTS {\n");
  for ( i = 0; tm.roots[i].name; ++ i ) {
    if ( tm.roots[i].l != tm.roots[i].h ) {
      tm_msg("R \t [%p,%p] %s %d\n",
	     tm.roots[i].l,
	     tm.roots[i].h,
	     tm.roots[i].name,
	     i);
    }
  }
  tm_msg("R }\n");

  tm_msg_enable("R", 0);

  /* Validate root sets. */
  {
    extern int _tm_user_bss[], _tm_user_data[];

    tm_assert(tm_ptr_is_in_root_set(_tm_user_bss), ": _tm_user_bss = %p", _tm_user_bss);
    tm_assert(tm_ptr_is_in_root_set(_tm_user_data), ": _tm_user_data = %p", _tm_user_data);
    _tm_set_stack_ptr(&i);
    tm_assert(tm_ptr_is_in_root_set(&i), ": &i = %p", &i);
  }

  /* Root marking. */
  _tm_root_loop_init();

  /* Global lists. */
  tm_type_init(&tm.types, 0);
  tm_list_set_color(&tm.types, tm_LIVE_TYPE);

  /* Node color iterators. */
#define X(C) \
  tm.node_color_iter[C].color = C; \
  tm_node_LOOP_INIT(C)
  
  X(tm_WHITE);
  X(tm_ECRU);
  X(tm_GREY);
  X(tm_BLACK);

#undef X

  /* Page managment. */
  memset(tm.page_in_use, 0, sizeof(tm.page_in_use));

  /* Block Mgmt. */
  tm_list_init(&tm.free_blocks);
  tm_list_set_color(&tm.free_blocks, tm_FREE_BLOCK);
  tm.free_blocks_n = 0;

  /* Types. */

  /* Type desc free list. */
  for ( i = 0; i < sizeof(tm.type_reserve)/sizeof(tm.type_reserve[0]); ++ i ) {
    tm_type *t = &tm.type_reserve[i];
    t->hash_next = (void*) tm.type_free;
    tm.type_free = t;
  }
  
  /* Type desc hash table. */
  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm.type_hash[i] = 0;
  }

  /* Block sweep iterator. */
  _tm_block_sweep_init();

  /* Start by allocating. */
  _tm_phase_init(tm_ALLOC);

  /* Finally... initialized. */
  -- tm.initing;
  ++ tm.inited;
  tm_msg_enable("WF", 0);
}


/***************************************************************************/
/* Node color mgmt. */


static __inline
void _tm_node_set_color(tm_node *n, tm_block *b, tm_type *t, tm_color c)
{
  int nc = tm_node_color(n);

  tm_assert_test(b);
  tm_assert_test(t);
  tm_assert_test(c <= tm_BLACK);

  /* Cumulative global stats. */
  ++ tm.alloc_n[c];
  ++ tm.alloc_n[tm_TOTAL];

  /* Global stats. */
  ++ tm.n[c];
  tm_assert_test(tm.n[nc]);
  -- tm.n[nc];

  /* Type stats. */
  ++ t->n[c];
  tm_assert_test(t->n[nc]);
  -- t->n[nc];

  /* Block stats. */
  ++ b->n[c];
  tm_assert_test(b->n[nc]);
  -- b->n[nc];

  tm_list_set_color(n, c);
}


void tm_node_set_color(tm_node *n, tm_block *b, tm_color c)
{
  tm_type *t;

  tm_assert_test(b);
  // _tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;

  _tm_node_set_color(n, b, t, c);

  tm_list_remove_and_append(&t->color_list[c], n);
 
  // _tm_validate_lists();
}


/**************************************************/


static __inline
void _tm_type_add_block(tm_type *t, tm_block *b)
{
  tm_assert_test(t);
  tm_assert_test(b);
  tm_assert_test(b->type == 0);

  /* Add to type's block list. */
  b->type = t;

  /* Type block stats. */
  ++ t->n[tm_B];

  /* Add to type's block list. */
  tm_list_insert(&t->blocks, b);
}


static __inline
void _tm_type_remove_block(tm_type *t, tm_block *b)
{
  tm_assert_test(t);
  tm_assert_test(b->type);
  tm_assert_test(b->type == t);

  /* Type block stats. */
  tm_assert_test(t->n[tm_B]);
  -- t->n[tm_B];

  /* Don't allocate nodes from it any more. */
  if ( t->alloc_from_block == b ) {
    t->alloc_from_block = 0;
  }

  /* Remove from type's block list. */
  tm_list_remove(b);

  /* Dissassociate with the type. */
  b->type = 0;
}


static __inline
void tm_block_init(tm_block *b)
{
  tm_assert_test(b->size);

  /* Initialize. */
  tm_list_init(&b->list);
  tm_list_set_color(b, tm_LIVE_BLOCK);

#if tm_name_GUARD
  b->name = "BLOCK";
#endif

  /* No type, yet. */
  b->type = 0;

  /* Init bounds of the block's allocation space. */
  b->begin = (char*) b + tm_block_HDR_SIZE;
  b->end   = (char*) b + b->size;
  
  /* Initialize block's allocation pointer
   * to beginning of valid space.
   */
  b->alloc = b->begin;

  /* Update valid node ptr range. */
  if ( tm_ptr_l > (void*) b->begin ) {
    tm_ptr_l = b->begin;
  }
  if ( tm_ptr_h < (void*) b->end ) {
    tm_ptr_h = b->end;
  }
  
#if tm_block_GUARD
  b->guard1 = b->guard2 = tm_block_hash(b);
#endif

  /* Clear block stats. */
  memset(b->n, 0, sizeof(b->n));

  /* Reset block sweep iterator. */
  _tm_block_sweep_init();

  /* Remember the first block and most recent blocks allocated. */
  tm.block_last = b;
  if ( ! tm.block_first ) {
    tm.block_first = b;
  } else {
#if tm_USE_SBRK
    /* Make sure heap grows up, other code depends on it. */
    tm_assert((void*) b >= (void*) tm.block_first);
#endif
  }
}


static
tm_block *_tm_block_alloc(size_t size)
{
  tm_block *b = 0;
  size_t offset;

  /* Force allocation to a multiple of a block. */
  if ( (offset = (size % tm_block_SIZE)) )
    size += tm_block_SIZE - offset;

  /* Scan block free list for a block of the right size. */
  {
    tm_block *bi;

    tm_list_LOOP(&tm.free_blocks, bi);
    {
      if ( bi->size == size &&
	   tm_list_color(bi) == tm_FREE_BLOCK
	   ) {
	tm_assert_test(tm.free_blocks_n);
	-- tm.free_blocks_n;

	tm_list_remove(bi);

	b = bi;

	tm_msg("b a fl b%p %d\n", (void*) b, tm.free_blocks_n);
	break;
      }
    }
    tm_list_LOOP_END;
  }

  /*
  ** If we didn't find one in the free list,
  ** allocate one from the OS.
  */
  if ( ! b ) {
    /* Make sure it's aligned to tm_block_SIZE. */
    b = _tm_os_alloc_aligned(size);

    /* Initialize its size. */
    b->size = size;
    
    /* Global OS block stats. */
    ++ tm.n[tm_B_OS];
    if ( tm.n[tm_B_OS_M] < tm.n[tm_B_OS] )
      tm.n[tm_B_OS_M] = tm.n[tm_B_OS];

    tm.n[tm_b_OS] += size;
    if ( tm.n[tm_b_OS_M] < tm.n[tm_b_OS] )
      tm.n[tm_b_OS_M] = tm.n[tm_b_OS];

    tm_msg("b a os b%p\n", (void*) b);
  }

  tm_block_init(b);

  /* Update stats. */
  ++ tm.n[tm_B];
  tm.blocks_allocated_since_gc += size / tm_block_SIZE;

  // tm_validate_lists();

  // fprintf(stderr, "  _tm_block_alloc(%p)\n", b);

  return b;
}


static 
tm_block *tm_block_scavenge(tm_type *t)
{
  tm_type *type = 0;

  tm_list_LOOP(&tm.types, type);
  {
    tm_block *block = 0;

    tm_list_LOOP(&type->blocks, block);
    {
      if ( block->type != t && 
	   tm_block_unused(block) &&
	   block->n[tm_TOTAL]
	   ) {
	return block;
      }
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;

  return 0;
}


/**********************************************************/
/* Block free */

static __inline
void _tm_node_delete(tm_node *n, tm_block *b);

static
int _tm_block_reclaim_nodes(tm_block *b)
{
  int count = 0, bytes = 0;
  tm_type *t = b->type;
  
  tm_assert_test(b->type);

  /*
  ** If all nodes in the block are free, 
  ** remove all nodes in the block from any lists.
  */
  tm_assert_test(tm_block_unused(b));

  {
    tm_node *n;
    
    /* First block. */
    n = tm_block_node_begin(b);
    while ( (void*) n < tm_block_node_alloc(b) ) {
      /* Remove and advance. */
      ++ count;
      bytes += b->size;

      _tm_node_delete(n, b);

      n = tm_block_node_next(b, n);
    }
  }

  /* Update white list stats. */

  /* Type node counts. */
  tm_assert_test(t->n[tm_WHITE] >= count);
  t->n[tm_WHITE] -= count;
  tm_assert_test(t->n[tm_TOTAL] >= count);
  t->n[tm_TOTAL] -= count;

  /* Block node counts. */
  tm_assert_test(b->n[tm_WHITE] >= count);
  b->n[tm_WHITE] -= count;
  tm_assert_test(b->n[tm_WHITE] == 0);

  tm_assert_test(b->n[tm_TOTAL] >= count);
  b->n[tm_TOTAL] -= count;
  tm_assert_test(b->n[tm_TOTAL] == 0);

  /* Global node counts. */
  tm_assert_test(tm.n[tm_WHITE] >= count);
  tm.n[tm_WHITE] -= count;
  tm_assert_test(tm.n[tm_TOTAL] >= count);
  tm.n[tm_TOTAL] -= count;

  return count;
}


static __inline
void _tm_block_reclaim(tm_block *b)
{
  tm_assert_test(b);
  tm_assert_test(tm_list_color(b) == tm_LIVE_BLOCK);

  /* Reclain any allocated nodes from type free lists. */
  _tm_block_reclaim_nodes(b);

  /* Avoid pointers into block. */
  b->alloc = b->begin;

  /* Global block stats. */
  tm_assert_test(tm.n[tm_B]);
  -- tm.n[tm_B];

  /* List cannot be last allocated. */
  if ( tm.block_last == b ) {
    tm.block_last = 0;
  }
  if ( tm.block_first == b ) {
    tm.block_first = 0;
  }

  /* Remove block from type. */
  _tm_type_remove_block(b->type, b);

  /* Mark block's pages as unused. */
  _tm_page_mark_unused_range(b, b->size);
}


static __inline
void _tm_block_free(tm_block *b)
{
  int os_free = 0;

  tm_assert(tm_block_unused(b));

  /* Reclaim block from type list. */
  _tm_block_reclaim(b);

#if tm_USE_MMAP
  /* Reduce calls to _tm_os_free() by keeping a few free blocks. */
  if ( tm.free_blocks_n >= 4 ) {
    // fprintf(stderr, "  tm_block_free too many free blocks: %d\n", tm.free_blocks_n);
    os_free = 1;
  } else {
    os_free = 0;
  }
#endif

#if tm_USE_SBRK
  /* If b was the last block allocated from the OS: */
  if ( sbrk(0) == (void*) b->end ) {
    /* Reduce valid node ptr range. */
    tm_assert(tm_ptr_h == b->end);
    tm_ptr_h = b;

    os_free = 1;
  } else {
    os_free = 0;
  }
#endif

  if ( os_free ) {
    /* Global OS block stats. */
    tm_assert_test(tm.n[tm_B_OS]);
    -- tm.n[tm_B_OS];

    tm_assert_test(tm.n[tm_b_OS] > b->size);
    tm.n[tm_b_OS] -= b->size;

    /* Return it back to OS. */
    _tm_os_free_aligned(b, b->size);

    tm_msg("b f os b%p\n", (void*) b);
  } else {
    /* Remove from t->blocks list and add to free block list. */
    tm_list_remove_and_append(&tm.free_blocks, b);
    ++ tm.free_blocks_n;

    tm_list_set_color(b, tm_FREE_BLOCK);

    tm_msg("b f fl b%p %d\n", (void*) b, tm.free_blocks_n);
  }

  /* Reset block sweep iterator. */
  _tm_block_sweep_init();

  // tm_validate_lists();

  // fprintf(stderr, "  _tm_block_free(%p)\n", b);
  // tm_msg("b f b%p\n", (void*) b);
}


static 
tm_block * _tm_block_alloc_for_type(tm_type *t)
{
  tm_block *b;
  
  /* Allocate a new block */
  b = _tm_block_alloc(tm_block_SIZE);

  // fprintf(stderr, "  _tm_block_alloc(%d) => %p\n", tm_block_SIZE, b);

  if ( b ) {
    _tm_type_add_block(t, b);
  }

  // tm_msg("b a b%p t%p\n", (void*) b, (void*) t);

  return b;
}


/*********************************************************************/
/* Node creation/destruction. */


static __inline 
void _tm_node_init(tm_node *n, tm_block *b)
{
  tm_type *t;

  tm_assert_test(b);
  // _tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;
  tm_list_set_color(n, tm_WHITE);
  tm_list_init(&n->list);
  
  /* Type node counts. */
  ++ t->n[tm_TOTAL];
  ++ t->n[tm_WHITE];

  /* Block node counts */
  ++ b->n[tm_TOTAL];
  ++ b->n[tm_WHITE];
    
  /* Global node counts. */
  ++ tm.n[tm_TOTAL];
  ++ tm.n[tm_WHITE];
  
  /* Place on type's list/ */
  tm_node_set_color(n, b, tm_WHITE);
  
  // tm_validate_lists();

  // tm_msg("N n%p t%p\n", (void*) n, (void*) t);
}


static __inline 
void _tm_node_delete(tm_node *n, tm_block *b)
{
  tm_type *t;
  tm_color nc = tm_node_color(n);

  // _tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;
  tm_assert_test(nc == tm_WHITE);

#if 0 /* See _tm_block_reclaim_nodes() */
  /* Type node counts. */
  tm_assert_test(t->n[tm_TOTAL]);
  -- t->n[tm_TOTAL];
  tm_assert_test(t->n[nc]);
  -- t->n[nc];

  /* Block node counts. */
  tm_assert_test(b->n[tm_TOTAL] > 0);
  -- b->n[tm_TOTAL];
  tm_assert_test(b->n[nc]) > 0);
  -- b->n[nc];

  /* Global node counts. */
  tm_assert_test(tm.n[tm_TOTAL] > 0);
  -- tm.n[tm_TOTAL];
  tm_assert_test(tm.n[nc] > 0);
  -- tm.n[nc];
#endif

  /* Remove from type list. */
  tm_list_remove(n);

  // tm_validate_lists();

#if 0
  tm_msg("N d n%p[%lu] t%p\n", 
	 (void*) n,
	 (unsigned long) t->size,
	 (void*) t);
#endif
}


static __inline 
int _tm_node_alloc_some(tm_type *t, long left)
{
  int count = 0;
  size_t bytes = 0;
  tm_block *b;
  
  /* Get allocation block for type. */
  if ( ! (b = t->alloc_from_block) ) {
    b = t->alloc_from_block = _tm_block_alloc_for_type(t);
#if 0
    fprintf(stderr, "  _tm_block_alloc_for_type(%p) => %p\n",
	    (void*) t,
	    (void*) t->alloc_from_block);
#endif
    if ( ! b )
      goto done;
  }

  // _tm_block_validate(b);
  
  {
    /* The next node after allocation. */
    void *pe = tm_block_node_begin(b);
    
    while ( (pe = tm_block_node_next(b, tm_block_node_alloc(b))) 
	    <= tm_block_node_end(b)
	    ) {
      tm_node *n;
      
      // _tm_block_validate(b);
      
      n = tm_block_node_alloc(b);
      
      /* Increment allocation pointer. */
      b->alloc = pe;
      
      /* Initialize the node. */
      _tm_node_init(n, b);
      
      /* Accounting. */
      ++ count;
      bytes += t->size;
      
      if ( -- left <= 0 ) {
	goto done;
      }
    }
    
    /*
    ** Used up allocation block;
    ** Force new block allocation next time around. 
    */
    t->alloc_from_block = 0;
  }

  done:
#if 0
  if ( count )
    tm_msg("N i n%lu b%lu t%lu\n", count, bytes, t->n[tm_WHITE]);
#endif

  return count;
}

/************************************************************************/
/* Type creation. */

static __inline
void tm_type_init(tm_type *t, size_t size)
{
  /* Assign a type id. */
  t->id = ++ tm.type_id;

  /* Initialize type. */
  tm_list_init(&t->list);
  tm_list_set_color(&t->list, tm_LIVE_TYPE);
#if tm_name_GUARD
  t->name = "TYPE";
#endif
  t->size = size;
  tm_list_init(&t->blocks);
  tm_list_set_color(&t->blocks, tm_LIVE_BLOCK);
  
  /* Init node counts. */
  memset(t->n, 0, sizeof(t->n));
  
  /* Init node lists. */
  {
    int j;
    
    for ( j = 0; j < sizeof(t->color_list) / sizeof(t->color_list[0]); j ++ ) {
      tm_list_init(&t->color_list[j]);
      tm_list_set_color(&t->color_list[j], j);
    }
  }

  /* Force new block to be allocated. */
  t->alloc_from_block = 0;

  t->desc = 0;
}


static __inline 
tm_type *tm_type_new(size_t size)
{
  tm_type *t;

  /* Allocate new type desc. */

  /* Take from free list. */
  if ( tm.type_free ) {
    t = tm.type_free;
    tm.type_free = t->hash_next;
    t->hash_next = 0;
  } else {
    t = 0;
  }

  /* Initialize type. */
  tm_type_init(t, size);

  /* Add to global types list. */
  tm_list_insert(&tm.types, t);
  
  /*
  ** To avoid triggering a collection,
  ** allocate some nodes now so the free list is not empty.
  */
  _tm_node_alloc_some(t, tm_node_alloc_some_size);

  return t;
}


static __inline 
int tm_size_hash(size_t size)
{
  int i;

  /* IMPLEMENT: support for big allocs. */
  tm_assert(size <= tm_block_SIZE_MAX);

  /* Compute hash bucket index. */
  i = size / tm_ALLOC_ALIGN;

  /* Modulus into hash bucket index. */
  i %= tm_type_hash_LEN;

  return i;
}


static __inline 
tm_type *tm_size_to_type_2(size_t size)
{
  tm_type **tp, *t;
  int i;
  
  i = tm_size_hash(size);

  /* Look for type by size. */
  for ( tp = &tm.type_hash[i]; (t = (*tp)); tp = &t->hash_next ) {
    if ( t->size == size ) {
      /* Move t to front of hash bucket. */
      *tp = t->hash_next;
      t->hash_next = tm.type_hash[i];
      tm.type_hash[i] = t;
      break;
    }
  }

  return t;
}


static __inline 
tm_type *tm_type_new_2(size_t size)
{
  int i;
  tm_type *t;

  t = tm_type_new(size);
  
  i = tm_size_hash(t->size);

  /* Add to type hash */
  t->hash_next = tm.type_hash[i];
  tm.type_hash[i] = t;
  
#if 0
  tm_msg("t a t%p %lu\n", (void*) t, (unsigned long) size);
#endif

  return t;
}


tm_adesc *tm_adesc_for_size(tm_adesc *desc, int force_new)
{
  tm_type *t;

  if ( ! force_new ) {
    t = tm_size_to_type_2(desc->size);
    if ( t )
      return t->desc;
  }

  t = tm_type_new_2(desc->size);
  t->desc = desc;
  t->desc->hidden = t;

  return t->desc;
}


static __inline 
tm_type *tm_size_to_type(size_t size)
{
  tm_type *t;
  
  /* Align size to tm_ALLOC_ALIGN. */
  size = (size + (tm_ALLOC_ALIGN - 1)) & ~(tm_ALLOC_ALIGN - 1);

  /* Try to find existing type by size. */
  t = tm_size_to_type_2(size);

  if ( t )
    return t;

  /* Align size to power of two. */
#define POW2(i) if ( size <= (1UL << i) ) size = (1UL << i); else

  POW2(3)
  POW2(4)
  POW2(5)
  POW2(6)
  POW2(7)
  POW2(8)
  POW2(9)
  POW2(10)
  POW2(11)
  POW2(12)
  POW2(13)
  POW2(14)
  POW2(15)
  POW2(16)
  (void) 0;

#undef POW2

  t = tm_size_to_type_2(size);

  /* If a type was not found, create a new one. */
  if ( ! t ) {
    t = tm_type_new_2(size);
  }
  
  tm_assert_test(t->size == size);
  
  return t;
}


/***************************************************************************/
/* Sweeping */


static __inline 
void _tm_node_sweep(tm_node *n, tm_block *b)
{
  /* Make sure the node is unmarked. */
  tm_assert_test(tm_node_color(n) == tm_ECRU);
  // _tm_block_validate(b);

  /* Clear out data. */
  memset(tm_node_to_ptr(n), 0, b->type->size);

  /* Put it on the WHITE (free) list. */
  tm_node_set_color(n, b, tm_WHITE);

#if 0
  tm_msg("s n%p\n", n);
#endif

  /* Free the block? */
  if ( tm_block_unused(b) ) {
    _tm_block_free(b);
  }
}


static 
size_t _tm_node_sweep_some(int left)
{
  size_t count = 0, bytes = 0;
 
  if ( tm.n[tm_ECRU] ) {
    if ( _tm_check_sweep_error() )
      return 0;

    tm_node_LOOP(tm_ECRU);
    {
      tm_assert_test(tm_node_color(n) == tm_ECRU);

      _tm_node_sweep(n, tm_node_to_block(n));

      bytes += t->size;
      ++ count;

      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK(tm_ECRU);
      }
    }
    tm_node_LOOP_END(tm_ECRU);
  }

  /* Update allocation stats. */
  tm.n[tm_b] -= bytes;

#if 0
  if ( count ) 
    tm_msg("s c%lu b%lu l%lu\n", count, bytes, tm.n[tm_ECRU]);
#endif

  return tm.n[tm_ECRU];
}


static 
size_t _tm_node_sweep_some_for_type(tm_type *t, int left)
{
  size_t count = 0, bytes = 0;
  tm_node *n;

  while ( t->n[tm_ECRU] ) {
    n = tm_list_first(&t->color_list[tm_ECRU]);
    _tm_node_sweep(n, tm_node_to_block(n));
    
    bytes += t->size;
    ++ count;
    
    if ( -- left <= 0 ) {
      break;
    }
  }

  /* Update allocation stats. */
  tm.n[tm_b] -= bytes;

#if 0
  if ( count ) 
    tm_msg("s t%p c%lu b%lu l%lu\n", t, count, bytes, tm.n[tm_ECRU]);
#endif

  return t->n[tm_ECRU];
}


static 
void _tm_node_sweep_all()
{
  while ( tm.n[tm_ECRU] ) {
    tm_node_LOOP_INIT(tm_ECRU);
    _tm_node_sweep_some(tm.n[tm_ECRU]);
  }
}


/***************************************************************************/
/* Unmarking */


static 
int _tm_block_sweep_maybe(tm_block *b)
{
  return 0;
}


static 
int _tm_block_sweep_some()
{
  int count = 0;
  unsigned long bytes = 0;
  int left = tm_block_sweep_some_size;

#if tm_USE_SBRK
  /*
  ** Try to sweep last block allocated from OS first
  ** because we might be able to return it to the OS.
  */
#endif

#if 0
  if ( count ) 
    tm_msg("b s b%lu b%lu\n", (unsigned long) count, (unsigned long) bytes);
#endif

  return count;
}


/***************************************************************************/
/* Unmarking */


static 
size_t _tm_node_unmark_some(long left)
{
  size_t count = 0, bytes = 0;

  tm_node_LOOP(tm_BLACK);
  {
    tm_assert_test(tm_node_color(n) == tm_BLACK);
    
    tm_node_set_color(n, tm_node_to_block(n), tm_ECRU);
    
    bytes += t->size;
    ++ count;
    
    if ( -- left <= 0 ) {
      tm_node_LOOP_BREAK(tm_BLACK);
    }
  }
  tm_node_LOOP_END(tm_BLACK);

#if 0
  if ( count ) 
    tm_msg("u n%lu b%lu l%lu\n", count, bytes, tm.n[tm_BLACK]);
#endif

  return tm.n[tm_BLACK];
}


static 
void _tm_node_unmark_all()
{
  while ( tm.n[tm_BLACK] ) {
    tm_node_LOOP_INIT(tm_BLACK);
    _tm_node_unmark_some(tm.n[tm_TOTAL]);
  }
}


/***************************************************************************/
/* Full GC. */


static
void _tm_gc_clear_stats()
{
  tm.blocks_allocated_since_gc = 0;
  tm.blocks_in_use_after_gc = tm.n[tm_B];

  tm.nodes_allocated_since_gc = 0;
  tm.nodes_in_use_after_gc = tm.n[tm_NU] = tm.n[tm_TOTAL] - tm.n[tm_WHITE];

  tm.bytes_allocated_since_gc = 0;
  tm.bytes_in_use_after_gc = tm.n[tm_b];
}


void _tm_gc_full_type_inner(tm_type *type)
{
  int try = 0;

  tm_msg("gc {\n");

  _tm_sweep_is_error = 0;

  while ( try ++ < 2) {
    /* Unmark all marked nodes. */
    _tm_phase_init(tm_ALLOC);
    _tm_node_unmark_all();
    tm_assert_test(tm.n[tm_BLACK] == 0);

    /* Scan all roots, mark pointers. */
    _tm_phase_init(tm_ROOT);
    _tm_root_scan_all();
    
    /* Scan all marked nodes. */
    _tm_phase_init(tm_SCAN);
    _tm_node_scan_all();
    tm_assert_test(tm.n[tm_GREY] == 0);
    
    /* Sweep the unmarked nodes. */
    _tm_phase_init(tm_SWEEP);
    _tm_node_sweep_all();
    tm_assert_test(tm.n[tm_ECRU] == 0);
    
    /* Unmark all marked nodes. */
    _tm_phase_init(tm_ALLOC);
    _tm_node_unmark_all();
    tm_assert_test(tm.n[tm_BLACK] == 0);

    /* Sweep some blocks? */
    while ( _tm_block_sweep_some() ) {
    }

    _tm_phase_init(tm_ALLOC);
  }

  _tm_gc_clear_stats();

  tm_msg("gc }\n");
}


void _tm_gc_full_inner()
{
  _tm_gc_full_type_inner(0);
}


/***************************************************************************/
/* allocation */


static __inline 
void *_tm_type_alloc_node_from_free_list(tm_type *t)
{
  tm_node *n;

  if ( (n = tm_list_first(&t->color_list[tm_WHITE])) ) {
    char *ptr;

    /* Assert its coming from a valid block. */
    tm_assert_test(tm_node_to_block(n)->type == t);

    /* It better be free... */
    tm_assert_test(tm_node_color(n) == tm_WHITE);

    /* A ptr to node's data space. */
    ptr = tm_node_to_ptr(n);

    /* Clear data space. */
    memset(ptr, 0, t->size);
    
    /* Put it on the appropriate allocated list. */
    tm_node_set_color(n, tm_node_to_block(n), tm_phase_alloc[tm.phase]);

    /* Mark page used. */
    _tm_page_mark_used(ptr);

    /* Keep track of allocation amounts. */
    ++ tm.nodes_allocated_since_gc;
    tm.bytes_allocated_since_gc += t->size;
    tm.n[tm_b] += t->size;

#if 0
    tm_msg("N a n%p[%lu] t%p\n", 
	   (void*) n, 
	   (unsigned long) t->size,
	   (void*) t);
#endif

    return ptr;
  } else {
    return 0;
  }
}


void _tm_free_inner(void *ptr)
{
  tm_block *b;
  tm_node *n;

  n = tm_pure_ptr_to_node(ptr);
  b = tm_node_to_block(n);
  // _tm_block_validate(b);
  tm_assert(tm_node_color(n) != tm_WHITE);
  tm_node_set_color(n, b, tm_WHITE);
}


void *_tm_alloc_type_inner(tm_type *t)
{
  void *ptr;
#if tm_TIME_STAT
  tm_time_stat *ts;
#endif

  ++ tm.alloc_id;
  if ( tm.alloc_id == 5040 )
    tm_stop();

  tm.alloc_pass = 0;
  memset(tm.alloc_n, 0, sizeof(tm.alloc_n));

  tm.next_phase = (enum tm_phase) -1;
  
  // tm_validate_lists();

  //try_again:
  ++ tm.alloc_pass;

#if tm_TIME_STAT
  tm_time_stat_begin(ts = &tm.ts_phase[tm.phase]);
#endif

  switch ( tm.phase ) {
  case tm_ALLOC:
    /* Unmark some nodes. */
    if ( tm.n[tm_BLACK] ) {
      if ( ! _tm_node_unmark_some(tm_node_unmark_some_size) ) {
	tm.next_phase = tm_ROOT;
      }
    }

    /* Keep allocating until out of free nodes. */
    /* Don't keep allocating if allocated > total * tm_GC_THRESHOLD. */
#ifndef tm_GC_THRESHOLD
#define tm_GC_THRESHOLD 3 / 4
#endif
    if ( (! t->n[tm_WHITE]) ||
	 (t->n[tm_phase_alloc[tm.phase]] > t->n[tm_TOTAL] * tm_GC_THRESHOLD) ||
	 (tm.n[tm_phase_alloc[tm.phase]] > tm.n[tm_TOTAL] * tm_GC_THRESHOLD)
	 ) {
      tm.next_phase = tm_ROOT;
    }
    break;
    
  case tm_ROOT:
    if ( tm_root_scan_full ) {
      /* Scan all roots for pointers, scan marked nodes. */
      _tm_root_scan_all();
      tm.next_phase = tm_SCAN;
    } else {
      /* Scan some roots for pointers. */
      if ( ! _tm_root_scan_some() ) {
	tm.next_phase = tm_SCAN;
      }
    }
    break;
    
  case tm_SCAN:
    /* Scan a little bit. */
    if ( ! _tm_node_scan_some(tm_node_scan_some_size) ) {
      /* Scanning is complete. */
      
      /* 
      ** If the roots were mutated during mark,
      ** remark all roots.
      ** Rationale: A reference to a new node may have been
      ** copied from the unmarked stack to a global root.
      */
      if ( tm_root_scan_full || 
	   tm.data_mutations || 
	   tm.stack_mutations ) {
	/* Mark all roots. */
	_tm_root_scan_all();
      } else {
	/* Mark the stack. */
	_tm_stack_scan();
      }
      
      /* 
      ** If after marking the register and stack,
      ** no additional nodes were marked,
      ** begin sweeping.
      */
      if ( ! _tm_node_scan_some(tm_node_scan_some_size) ) {
	/* Force SWEEP phase here so new node gets allocated as GREY. */
	_tm_phase_init(tm_SWEEP);
	tm.next_phase = -1;
      }
    }
    break;
      
  case tm_SWEEP:
    /* 
    ** Don't bother remarking after root mutations
    ** because all newly allocated objects are marked "in use" during SWEEP.
    ** See tm_phase_alloc.
    */

    /* If there are nodes to be scanned, do them now. 
    ** Rational: the grey nodes may have new references 
    ** to nodes that would otherwise be sweeped.
    */
    if ( tm.n[tm_ECRU] ) {
      /* If scan any marked nodes now. */
      if ( tm.n[tm_GREY] ) {
	_tm_node_scan_some(tm_node_scan_some_size * 2);
	if ( ! tm.n[tm_GREY] ) {
	  /* For some reason we need to restart the sweep list. */
	  tm_node_LOOP_INIT(tm_ECRU);
	}
      }
      if ( ! tm.n[tm_GREY] ) {
	/* Try to immediately sweep nodes for the type we want first. */
	/* There are no nodes left to sweep. */
	if ( ! 
	     (
	      _tm_node_sweep_some_for_type(t, tm_node_sweep_some_size) ||
	      _tm_node_sweep_some(tm_node_sweep_some_size)
	      ) 
	     ) {
	  /* Start unmarking used nodes. */
	  tm.next_phase = tm_ALLOC;
	}
      }
    } else {
      /* There are no nodes left to sweep. */
      /* Start unmarking used nodes. */
      tm.next_phase = tm_ALLOC;
    }
    break;

  default:
    tm_abort();
    break;
  }
  
#if tm_TIME_STAT
  tm_time_stat_end(ts);
#endif

  /* Switch to new phase. */
  if ( tm.next_phase != (enum tm_phase) -1 )
    _tm_phase_init(tm.next_phase);

  /* Keep track of how many allocs per phase. */
  ++ tm.alloc_by_phase[tm.phase];

  /* Take one from the free list. */
  if ( ! (ptr = _tm_type_alloc_node_from_free_list(t)) ) {
#if 0
    /* Do we need to check memory pressure here for a full GC? */
    if ( tm.nodes_allocated_since_gc > 1000 &&
	 tm.nodes_allocated_since_gc > tm.nodes_in_use_after_gc * 3 / 4 ) {
      fprintf(stderr, "  TM: memory pressure high, full gc\n");
      _tm_gc_full_inner();
    }
#endif

    /* We must attempt to allocate from current block, a free block or from new block from OS. */
    _tm_node_alloc_some(t, tm_node_alloc_some_size);
    ptr = _tm_type_alloc_node_from_free_list(t);

    tm_assert_test(ptr);
  }

#if 0
  tm_msg("a %p[%lu]\n", ptr, (unsigned long) t->size);
#endif

  // tm_validate_lists();

  /* Validate tm_ptr_to_node() */
#if tm_ptr_to_node_TEST
  if ( ptr ) {
    char *p = ptr;
    tm_node *n = (void*) (p - tm_node_HDR_SIZE);
    tm_assert(tm_ptr_to_node(n) == 0);
    tm_assert(tm_ptr_to_node(p) == n);
    tm_assert(tm_ptr_to_node(p + 1) == n);
    tm_assert(tm_ptr_to_node(p + t->size / 2) == n);
    tm_assert(tm_ptr_to_node(p + t->size - 1) == n);
#if tm_ptr_AT_END_IS_VALID
    tm_assert(tm_ptr_to_node(p + t->size) == n);
#else
    tm_assert(tm_ptr_to_node(p + t->size) == 0);
#endif
  }
#endif

  return (void*) ptr;
}


void *_tm_alloc_inner(size_t size)
{
  tm_type *type = tm_size_to_type(size);
  tm.alloc_request_size = size;
  tm.alloc_request_type = type;
  return _tm_alloc_type_inner(type);  
}


void *_tm_alloc_desc_inner(tm_adesc *desc)
{
  tm_type *type = (tm_type*) desc->hidden;
  tm.alloc_request_size = type->size;
  tm.alloc_request_type = type;
  return _tm_alloc_type_inner(type);
}


void *_tm_realloc_inner(void *oldptr, size_t size)
{
  char *ptr = 0;
  tm_type *t, *oldt;

  oldt = tm_node_to_type(tm_pure_ptr_to_node(oldptr));
  t = tm_size_to_type(size);

  if ( oldt == t ) {
    ptr = oldptr;
  } else {
    ptr = _tm_alloc_inner(size);
    memcpy(ptr, oldptr, size < oldt->size ? size : oldt->size);
  }
  
  return (void*) ptr;
}


/***************************************************************************/
/* user level routines */


/* Avoid leaving garbage on the stack. */
/* Don't put this in user.c, so it cannot be optimized away. */

void __tm_clear_some_stack_words()
{
  int some_words[64];
  memset(some_words, 0, sizeof(some_words));
}


/***************************************************************************/


