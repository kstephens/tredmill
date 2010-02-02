/** \file init.c
 * \brief Initialization
 *
 * - $Id: tm.c,v 1.20 2009-08-01 10:47:31 stephens Exp $
 */


#include "tm.h"
#include "internal.h"

/****************************************************************************/
/*! \defgroup control Control */
/*@{*/

/*! Nodes to parcel from a tm_block per tm_alloc(). */
long tm_node_parcel_some_size = 8;

/*! Words to scan per tm_alloc(). */
long tm_node_scan_some_size = 32 * 2; 

/*! Nodes to sweep per tm_alloc(). */
long tm_node_sweep_some_size = 8;

/*! Nodes to unmark per tm_alloc(). */
long tm_node_unmark_some_size = 8;

/*! Number of blocks per tm_alloc() to sweep after sweep phase. */
long tm_block_sweep_some_size = 2;

/*! Minimum number of tm_blocks to retain on block free list.*/
int tm_block_min_free = 4;

/* Soft OS allocation limit in bytes. */
size_t tm_os_alloc_max = 64 * 1024 * 1024; /* 64MiB */

/*! If true, all roots are scanned atomically before moving to the SCAN phase.  */
int    tm_root_scan_full = 1;

/*@}*/


/****************************************************************************/
/* \defgroup initialization Initialization */
/*@{*/

/**
 * Determine the direction of stack growth.
 */
static 
int tm_stack_growth(char *ptr, int depth)
{
  char *top_of_stack = (char*) &top_of_stack;
  if ( depth ) {
    return tm_stack_growth(ptr, depth - 1);
  }
  return top_of_stack - ptr;
}


static void _tm_block_sweep_init();

/**
 * API: Initialize the tm allocator.
 *
 * This should be called from main():
 *
 * <pre>
 * int main(int argc, char **argv, char **envp)
 * {
 *   tm_init(&argc, &argv, &envp);
 *   ...
 * }
 * </pre>
 */
void tm_init(int *argcp, char ***argvp, char ***envpp)
{
  int i;

  tm_assert(sizeof(unsigned long) == sizeof(void *));

  /*! Initialize allocation log. */
  tm_alloc_log_init();

  /*! Initialize colors. */
  tm_colors_init(&tm.colors);

  /*! Initialize allocation colors. */
  tm.alloc_color = tm_ECRU;

  /*! Initialize time stat names. */
  tm.ts_os_alloc.name = "tm_os_alloc";
  tm.ts_os_free.name = "tm_os_free";
  tm.ts_alloc.name = "tm_alloc";
  tm.ts_free.name = "tm_free";
  tm.ts_gc.name = "gc";
  tm.ts_gc_inner.name = "gc_inner";
  tm.ts_barrier.name = "tm_barrier";
  tm.ts_barrier_pure.name = "tm_barrier_p";
  tm.ts_barrier_root.name = "tm_barrier_r";
  tm.ts_barrier_black.name = "tm_barrier B";

  for ( i = 0; i < (sizeof(tm.p.ts_phase) / sizeof(tm.p.ts_phase[0])); ++ i ) {
    tm.p.ts_phase[i].name = tm_phase_name[i];
  }

  /*! Initialize tm_msg() ignore table. */
  if ( tm_msg_enable_all ) {
    tm_msg_enable("\1", 1);
  } else {
    tm_msg_enable(tm_msg_enable_default, 1);
    tm_msg_enable(" \t\n\r", 1);
  }

  tm_msg_enable("WF", 1);
  // tm_msg_enable("b", 1);

  tm_list_assert_layout();

  /*! Warn if already initalized. */
  if ( tm.inited ) {
    tm_msg("WARNING: tm_init() called more than once.\nf");
  }
  tm.initing ++;

  /*! Error if argcp and argvp are not given. */
  if ( ! argcp || ! argvp ) {
    tm_msg("WARNING: tm_init() not called from main().\n");
  }

  /*! Default envpp = &environ, if not given. */
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

  /*! Initialize possible pointer range. */
  tm_ptr_l = (void*) ~0UL;
  tm_ptr_h = 0;

  /*! Initialize root sets. */
  tm.nroots = 0;
  tm.data_mutations = tm.stack_mutations = 0;

  tm.root_datai = -1;

  /*! Initialize root set for the register set, using a jmpbuf struct. */
  /* A C jmpbuf struct contains the saved registers set, hopefully. */
  tm_root_add("register", &tm.jb, (&tm.jb) + 1);

  /*! Initialize roots set for the stack. */

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


  /*! Remove anti-roots: do not scan tm's internal data structures. */
  tm_root_remove("tm", &tm, &tm + 1);

  /*! Initialize root set for initialized and uninitialize (zeroed) data segments. */
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

#if 0
    fprintf(stderr, "__data_start = %d\n", __data_start);
    fprintf(stderr, "__bss_start = %d\n", __bss_start);
    fprintf(stderr, "_end = %d\n", _end);
#endif

#if 1
    tm_assert(&__data_start < &__bss_start && &__bss_start < &_end);
    tm_root_add("initialized data", &__data_start, &__bss_start);
    tm_root_add("uninitialized data", &__bss_start, &_end);
#endif
  }
#define tm_roots_data_segs
#endif

#ifndef tm_roots_data_segs
#error must specify how to find the data segment(s) for root marking.
#endif


  /*! IMPLEMENT: Support dynamically-loaded library data segments. */

  /*! Dump the tm_root sets. */
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

  /*! Validate tm_root sets. */
  {
    extern int _tm_user_bss[], _tm_user_data[];

    tm_assert(tm_ptr_is_in_root_set(_tm_user_bss),  ": _tm_user_bss = %p", _tm_user_bss);
    tm_assert(tm_ptr_is_in_root_set(_tm_user_data), ": _tm_user_data = %p", _tm_user_data);
    _tm_set_stack_ptr(&i);
    tm_assert(tm_ptr_is_in_root_set(&i), ": &i = %p", &i);
  }

  /*! Initialize root marking loop. */
  _tm_root_loop_init();

  /*! Initialize global tm_type list. */
  tm_type_init(&tm.types, 0);
  tm_list_set_color(&tm.types, tm_LIVE_TYPE);

  /*! Initialize tm_node color iterators. */
#define X(C) \
  tm.node_color_iter[C].color = C; \
  tm_node_LOOP_INIT(C)
  
  X(tm_WHITE);
  X(tm_ECRU);
  X(tm_GREY);
  X(tm_BLACK);

#undef X

  /*! Initialize page managment. */
  memset(tm.page_in_use, 0, sizeof(tm.page_in_use));

  /*! Initialize tm_block free list. */
  tm_list_init(&tm.free_blocks);
  tm_list_set_color(&tm.free_blocks, tm_FREE_BLOCK);
  tm.free_blocks_n = 0;

  /* Types. */

  /*! Initialize tm_type free list. */
  for ( i = 0; i < sizeof(tm.type_reserve)/sizeof(tm.type_reserve[0]); ++ i ) {
    tm_type *t = &tm.type_reserve[i];
    t->hash_next = (void*) tm.type_free;
    tm.type_free = t;
  }
  
  /*! Initialize size to tm_type hash table. */
  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm.type_hash[i] = 0;
  }

  /*! Initialize block sweep iterator. */
  _tm_block_sweep_init();

  /*! Initialize phase: start by unmarking. */
  _tm_phase_init(tm_UNMARK);
  
  /*! Set up write barrier hooks. */
  _tm_write_barrier = __tm_write_barrier;
  _tm_write_barrier_pure = __tm_write_barrier_pure;
  _tm_write_barrier_root = __tm_write_barrier_root;
  
  /*! Mark system as initialized. */
  -- tm.initing;
  ++ tm.inited;
  tm_msg_enable("WF", 0);
}


/*@}*/

/***************************************************************************/


