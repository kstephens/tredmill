/* $Id: tm.c,v 1.11 2000-05-10 03:57:36 stephensk Exp $ */

#include "tm.h"
#include "internal.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

/****************************************************************************/
/* User control vars. */

long tm_node_alloc_some_size = 2;
long tm_root_mark_some_size = 512;
long tm_node_mark_some_size = 2;
long tm_node_sweep_some_size = 2;
long tm_block_sweep_some_size = 2;
long tm_node_unmark_some_size = 2;
int  tm_root_mark_full = 1;


/****************************************************************************/
/* Current status. */

int tm_marking = 0;
int tm_sweeping = 0;

/****************************************************************************/
/* Debug messages. */

FILE *tm_msg_file = 0;
const char *tm_msg_prefix = "";

static const char *tm_msg_enable_default = 
// "sNpwtFWVAg*r"
   "gFWA"
//  "gTFWA"
// "RgTFWA"
;

int tm_msg_enable_all = 0;


/****************************************************************************/
/* Safety */

void tm_validate_lists();

#if tm_block_GUARD
#define tm_block_validate(b) do { \
   tm_assert_test(b); \
   tm_assert_test(! ((void*) &tm <= (void*) (b) && (void*) (b) < (void*) (&tm + 1))); \
   tm_assert_test((b)->guard1 == tm_block_hash(b)); \
   tm_assert_test((b)->guard2 == tm_block_hash(b)); \
} while(0)
#else
#define tm_block_validate(b)
#endif


/****************************************************************************/
/* Names */

static const char *tm_color_name[] = {
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

static const char *tm_struct_name[] = {
  "FREE_BLOCK",
  "LIVE_BLOCK",
  "FREE_TYPE",
  "LIVE_TYPE",
  0
};

static const char *tm_phase_name[] = {
  "ALLOC",  /* tm_ALLOC */
  "ROOTS",  /* tm_ROOT */
  "MARK",   /* tm_MARK */
  "SWEEP",  /* tm_SWEEP */
  "UNMARK", /* tm_UNMARK */
  0
};


/**************************************************************************/
/* Allocation colors. */

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

/*
This table defines what color a newly allocated node
should be during the current allocation phase.

All phases except the SWEEP phase move newly allocated nodes
from WHITE to ECRU.
The SWEEP phase moves newly allocated nodes
from WHITE to BLACK, because, the ECRU lists are considered to be 
unmarked garbage and the BLACK lists are considered to
in use during the SWEEP phase.  

GREY might seem a better choice for SWEEP allocs but,
if any reference that has not already been found by MARK 
cannot possibly exist, there for cannot be stored within nodes 
allocated during SWEEP.
*/

#if 1
#define tm_SWEEP_ALLOC_COLOR tm_BLACK
#else
#define tm_SWEEP_ALLOC_COLOR tm_GREY
#endif

static const tm_color tm_phase_alloc[] = {
  tm_DEFAULT_ALLOC_COLOR,  /* tm_ALLOC */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_ROOT */
  tm_DEFAULT_ALLOC_COLOR,  /* tm_MARK */
  tm_SWEEP_ALLOC_COLOR,    /* tm_SWEEP */
  tm_DEFAULT_ALLOC_COLOR   /* tm_UNMARK */
};

#undef tm_DEFAULT_ALLOC_COLOR

/**************************************************************************/
/* Global allocator data */

struct tm_data tm;

/**************************************************************************/
/* Global color list iterators. */

#define tm_node_LOOP_INIT(C) \
  tm.tp[C] = tm_list_next(&tm.types); \
  tm.np[C] = tm_list_next(&tm.tp[C]->l[C]);

#define tm_node_LOOP(C) do { \
  while ( (void*) tm.tp[C] != (void*) &tm.types ) { \
    tm_type *t = tm.tp[C], *tp = tm_list_next(t); \
    while ( (void*) tm.np[C] != (void*) &t->l[C] ) { \
      tm_node *n = tm.np[C], *np = tm_list_next(n); \
      tm.np[C] = np;

#define tm_node_LOOP_BREAK() goto tm_node_STOP

#define tm_node_LOOP_END(C) \
    } ; \
    tm.tp[C] = tp; \
    tm.np[C] = tm_list_next(&tp->l[C]); \
  } \
  tm_node_LOOP_INIT(C); \
  tm_node_STOP: (void)0; \
} while ( 0 )


/****************************************************************************/
/* Debugging. */

unsigned long tm_alloc_id = 0;
static unsigned long tm_alloc_pass = 0;

static char _tm_msg_enable_table[256];

void tm_msg_enable(const char *codes, int enable)
{
  const unsigned char *r;
 
  enable = enable ? 1 : -1;
  for ( r = codes; *r; r ++ ) {
    /* '\1' enables all codes. */
    if ( *r == '\1' ) {
      memset(_tm_msg_enable_table, enable, sizeof(_tm_msg_enable_table));
      break;
    } else {
      _tm_msg_enable_table[*r] += enable;
    }
  }
}

static int _tm_msg_ignored = 0; 
/* True if last tm_msg() was disabled; used by tm_msg1() */

void tm_msg(const char *format, ...)
{
  va_list vap;

  /* Determine if we need to ignore the msg based on the first char in the format. */
  if ( (_tm_msg_ignored = ! _tm_msg_enable_table[(unsigned char) format[0]]) )
    return;

  /* Default tm_msg_file. */
  if ( ! tm_msg_file )
    tm_msg_file = stderr;

  if ( ! *format )
    return;

  /* Print header. */
  fprintf(tm_msg_file, "%s%s%c %6lu ",
	  tm_msg_prefix,
	  *tm_msg_prefix ? " " : "",
	  tm_phase_name[tm.phase][0], 
	  tm_alloc_id);
  if ( tm_alloc_pass > 1 )
    fprintf(tm_msg_file, "(pass %lu) ", tm_alloc_pass);

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
  if ( _tm_msg_ignored )
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
  static int in_abort;

  in_abort ++;

  if ( in_abort == 1 && tm.inited ) {
    tm_print_stats();
  }

  in_abort --;

  tm_fatal();
}


/**************************************************************************/
/* assertions, warnings. */

void _tm_assert(const char *expr, const char *file, int lineno)
{
  tm_msg("\n");
  tm_msg("Fatal assertion \"%s\" failed %s:%d ", expr, file, lineno);
}

#define tm_assert(X,Y...)do { if ( ! (X) ) {_tm_assert(#X, __FILE__, __LINE__); tm_msg1("" Y), tm_msg1("\n"), tm_abort();} } while(0)

#define tm_assert_test(X,Y...)do { if ( ! (X) ) {_tm_assert(#X, __FILE__, __LINE__); tm_msg1("" Y), tm_msg1("\n"), tm_abort();} } while(0)

#define tm_warn(X,Y...) ((X) ? 0 : (tm_msg1("\n"), tm_msg("WARNING assertion \"%s\" failed %s:%d ", #X, __FILE__, __LINE__), tm_msg1("" Y), tm_msg1("\n"), tm_stop(), 1))



/****************************************************************************/
/* Root sets. */

static int _tm_root_add_1(tm_root *a)
{
  int i;
#define MAX_ROOTS (sizeof(tm.roots)/sizeof(tm.roots[0]))

  if ( a->l >= a->h )
    return -1;

  /* Scan for empty slot. */
  for ( i = 0; i < MAX_ROOTS; ++ i ) {
    if ( tm.roots[i].name == 0 || tm.roots[i].l == tm.roots[i].h ) {
      break;
    }
  }

  if ( tm.nroots <= i ) {
    tm.nroots = i + 1;
  }

  tm_assert(i < MAX_ROOTS);
  tm_assert(i >= tm.root_datai);

  if ( tm.root_newi == -1 ) {
    tm.root_newi = i;
  }

  tm.roots[i] = *a;

  tm_msg("R a [%p,%p] %s %d\n", 
	 tm.roots[i].l, 
	 tm.roots[i].h,
	 tm.roots[i].name,
	 i);

  return i;
}
#undef MAX_ROOTS

static int tm_root_subtract(tm_root *a, tm_root *b, tm_root *c)
{
  const void *tmp;

  if ( a->l > a->h ) {
    tmp = a->l; a->l = a->h; a->h = tmp;
  }
  if ( b->l > b->h ) {
    tmp = b->l; b->l = b->h; b->h = tmp;
  }
  if ( b->l == b->h ) {
    return 0;
  }

  if ( (a->l == b->l) || (b->l <= a->l && a->h <= b->h) ) {
    /*
     * a   L-------------------------------------------H
     * b      L--------------------H
     * c   0
     */

    /* b in a */
    return -1; /* Delete flag */
  }
  if ( b->h <= a->l || b->l >= a->h ) {
    /*
     * a   L-------------H                        L--------------H
     * b                   L--------------------H
     * c   0
     */
    /* b out a */
    /* *c = *a; */
    return 0;
  }
  if ( a->l < b->l && b->h < a->h ) {
    /*
     * a            L------H
     * b   H-----------------------H
     * c1                  L-------H
     * c2  L--------H
     */
    /* b is in a */
    c[0] = *a;
    c[0].l = b->h;
    c[1] = *a;
    c[1].h = b->l;
    return 2;
  }
  if ( a->l < b->h && b->h <= a->h ) {
    /*
     * a          L---------------------H
     * b    L--------------------H
     * c                         L------H
     */
    /* b.h in a */
    *c = *a;
    c->l = b->h;
    return 1;
  }
  if ( a->l < b->l && b->l <= a->h ) {
    /*
     * a   L---------------------H
     * b             L--------------------H
     * c   L---------H
     */
    /* b.l in a */
    *c = *a;
    c->h = b->l;
    return 1;
  }

  tm_abort();
  return -1;
}

static void _tm_root_add(tm_root *a)
{
  int i;
  tm_root c[2];

  /* Scan anti-roots for possible splitting */
  for ( i = 0; i < tm.naroots; ++ i ) {
    switch ( tm_root_subtract(a, &tm.aroots[i], c) ) {
    case -1: /* deleted */
      return;
      break;
      
    case 0: /* OK */
      break;
      
    case 1: /* clipped */
      *a = c[0];
      break;
      
    case 2: /* split */
      _tm_root_add(&c[0]);
      *a = c[1];
      break;
    }
  }

  _tm_root_add_1(a);
}

int tm_root_add(const char *name, const void *l, const void *h)
{
  tm_root a;

  a.name = name;
  a.l = l;
  a.h = h;

  tm.root_newi = -1;

  tm_msg("R A [%p,%p] %s PRE\n", 
	 a.l, 
	 a.h,
	 a.name);

  _tm_root_add(&a);

  return tm.root_newi;
}


void tm_root_remove(const char * name, const void *l, const void *h)
{
  int i;
  tm_root *a, *b, c[2];

  /* Add to anti-roots list. */
  b = &tm.aroots[i = tm.naroots ++];
  b->name = name;
  b->l = l;
  b->h = h;

  tm_msg("R A [%p,%p] %s ANTI-ROOT %d\n", 
	 tm.aroots[i].l, 
	 tm.aroots[i].h,
	 tm.aroots[i].name,
	 i);

  /* Adding a anti-root might require splitting existing roots. */
  /* Do not split tm.root[0], it's the machine register set! */

  for ( i = tm.root_datai; i < tm.nroots; ++ i ) {
    int j;
     
    for ( j = 0; j < tm.naroots; ++ j ) {
      a = &tm.roots[i];
      b = &tm.aroots[j];
      
      switch ( tm_root_subtract(a, b, c) ) {
      case -1: /* deleted */
	a->l = a->h = 0;
	j = tm.naroots;
	break;
	
      case 1: /* clipped */
	*a = *c;
	break;
	
      case 2: /* split */
	*a = *c;
	_tm_root_add(&c[1]);
	i = 1; j = -1; /* restart */
	break;
      }
    }
  }
}


int tm_ptr_is_in_root_set(const void *ptr)
{
  int i;

  for ( i = 0; i < tm.nroots; ++ i ) {
    if ( tm.roots[i].l <= ptr && ptr < tm.roots[i].h )
      return i + 1;
  }

  return 0;
}


/****************************************************************************/
/* Phase support. */

static void tm_register_mark();
static void tm_stack_mark();

static void __tm_write_root_ignore(void *referent);
static void __tm_write_root_root(void *referent);
static void __tm_write_root_mark(void *referent);
static void __tm_write_root_sweep(void *referent);
void (*_tm_write_root)(void *referent) = __tm_write_root_ignore;

static void __tm_write_barrier_ignore(void *referent);
static void __tm_write_barrier_root(void *referent);
static void __tm_write_barrier_mark(void *referent);
static void __tm_write_barrier_sweep(void *referent);
void (*_tm_write_barrier)(void *referent) = __tm_write_barrier_ignore;

static void __tm_write_barrier_pure_ignore(void *referent);
static void __tm_write_barrier_pure_root(void *referent);
static void __tm_write_barrier_pure_mark(void *referent);
static void __tm_write_barrier_pure_sweep(void *referent);
void (*_tm_write_barrier_pure)(void *referent) = __tm_write_barrier_pure_ignore;

static int tm_stack_growth(char *ptr, int depth)
{
  char *top_of_stack = (char*) &top_of_stack;
  if ( depth ) {
    return tm_stack_growth(ptr, depth - 1);
  }
  return top_of_stack - ptr;
}

static void tm_root_loop_init()
{
  tm.rooti = tm.root_datai;
  tm.rp = tm.roots[tm.rooti].l;
  tm.data_mutations = tm.stack_mutations = 0;
}

static void tm_phase_init(int p)
{
  tm_msg("p %s\n", tm_phase_name[p]);

#if 0
  if ( tm_alloc_id == 1987 )
    tm_stop();

#endif

  switch ( (tm.phase = p) ) {
  case tm_ALLOC:
    tm_marking = 0;
    tm_sweeping = 0;

    /* Write barrier. */
    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_ROOT:
    tm_marking = 0;
    tm_sweeping = 0;

    tm.stack_mutations = tm.data_mutations = 0;

    /* Initialize root mark loop. */
    tm_root_loop_init();

    /* Write barrier. */
    _tm_write_root = __tm_write_root_root;
    _tm_write_barrier = __tm_write_barrier_root;
    _tm_write_barrier_pure = __tm_write_barrier_pure_root;
    break;

  case tm_MARK:
    tm_marking = 1;
    tm_sweeping = 0;

    tm.stack_mutations = tm.data_mutations = 0;

    /* Set up for marking. */
    tm_node_LOOP_INIT(tm_GREY);

    /* Write barrier. */
    _tm_write_root = __tm_write_root_mark;
    _tm_write_barrier = __tm_write_barrier_mark;
    _tm_write_barrier_pure = __tm_write_barrier_pure_mark;
    break;

  case tm_SWEEP:
    tm_marking = 0;
    tm_sweeping = 1;

    /* Set up for sweeping. */
    tm_node_LOOP_INIT(tm_ECRU);

    /* Write barrier. */
    _tm_write_root = __tm_write_root_sweep;
    _tm_write_barrier = __tm_write_barrier_sweep;
    _tm_write_barrier_pure = __tm_write_barrier_pure_sweep;
    break;

  case tm_UNMARK:
    tm_marking = 0;
    tm_sweeping = 0;

    /* Set up for unmarking. */
    tm_node_LOOP_INIT(tm_BLACK);

    /* Write barrier. */
    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
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


static __inline void tm_block_sweep_init()
{
  tm.bt = tm_list_next(&tm.types);
  tm.bb = tm.bt != tm_list_next(tm.bt) ?
    tm_list_next(&tm.bt->blocks) :
    0;
}



/****************************************************************************/
/* Init. */

void tm_init(int *argcp, char ***argvp, char ***envpp)
{
  int i;

  /* Time stat names, */
  tm.ts_alloc_os.name = "AOS";
  tm.ts_alloc.name = "A";
  tm.ts_free.name = "F";
  tm.ts_gc.name = "GC";
  for ( i = 0; i < (sizeof(tm.ts_phase)/sizeof(tm.ts_phase[0])); i ++ ) {
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

  if ( tm.inited ) {
    tm_msg("WARNING: tm_init() called more than once.\nf");
  }
  tm.initing ++;

  if ( ! argcp ) {
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
      l = (void*) ((unsigned long) (l) - ((unsigned long) (l) % tm_PAGESIZE));
      h = (void*) ((unsigned long) (h) + (tm_PAGESIZE - ((unsigned long) (h) % tm_PAGESIZE)));
      
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
    tm_set_stack_ptr(&i);
    tm_assert(tm_ptr_is_in_root_set(&i), ": &i = %p", &i);
  }

  /* Root marking. */
  tm_root_loop_init();

  /* Global lists. */
  tm_list_init(&tm.types);
  tm_list_set_color(&tm.types, tm_LIVE_TYPE);
  tm_list_init(&tm.free_blocks);
  tm_list_set_color(&tm.free_blocks, tm_FREE_BLOCK);

  /* Types. */

  /* Type desc free list. */
  for ( i = 0; i < sizeof(tm.type_reserve)/sizeof(tm.type_reserve[0]); i ++ ) {
    tm_type *t = &tm.type_reserve[i];
    t->hash_next = (void*) tm.type_free;
    tm.type_free = t;
  }
  
  /* Type desc hash table. */
  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm.type_hash[i] = 0;
  }

  /* Block sweep iterator. */
  tm_block_sweep_init();

  /* Start by allocating. */
  tm_phase_init(tm_ALLOC);

  /* Finally... initialized. */
  tm.initing --;
  tm.inited ++;
  tm_msg_enable("WF", 0);
}

/***************************************************************************/
/* Node color mgmt. */

#define tm_node_color(n) ((tm_color) tm_list_color(n))

static __inline void _tm_node_set_color(tm_node *n, tm_block *b, tm_color c)
{
  tm_type *t;
  int nc = tm_node_color(n);

  tm_assert_test(b);
  t = b->type;
  tm_assert_test(t);
  tm_assert_test(c <= tm_BLACK);

  tm_assert_test(tm.n[nc]);
  tm.n[nc] --;
  tm.n[c] ++;

  /* tm_alloc() stats. */
  tm.alloc_n[c] ++;
  tm.alloc_n[tm_TOTAL] ++;

  t->n[c] ++;
  b->n[c] ++;
  tm_assert_test(t->n[nc]);
  tm_assert_test(b->n[nc]);
  t->n[tm_node_color(n)] --;
  b->n[tm_node_color(n)] --;

#if 0
  /* Invalidate the "to" color iterator. */
  tm_node_LOOP_INIT(c);
#endif

  tm_list_set_color(n, c);
}

static __inline void tm_node_set_color(tm_node *n, tm_block *b, tm_color c)
{
  tm_type *t;

  tm_assert_test(b);
  tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;

  _tm_node_set_color(n, b, c);

  tm_list_remove_and_append(&t->l[c], n);
 
  //tm_validate_lists();
}

/****************************************************************************/
/* Low-level alloc. */

static __inline
void *tm_alloc_os(long size)
{
  void *ptr;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_alloc_os);
#endif

  ptr = sbrk(size);

#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_alloc_os);
#endif

  if ( tm.alloc_os_expected ) {
    tm_warn(ptr == tm.alloc_os_expected, "ptr = %p, expected = %p", ptr, tm.alloc_os_expected);
  }
  if ( ptr == 0 || ptr == (void*) -1L ) {
    ptr = 0;
    tm_msg("A 0 %ld\n", (long) size);
  } else
  if ( size > 0 ) {
    tm_msg("A a %p[%ld]\n", ptr, (long) size);
    tm.alloc_os_expected = (char *)ptr + size;
  } else if ( size < 0 ) {
    tm_msg("A d %p[%ld]\n", ptr, (long) size);
    tm.alloc_os_expected += size;
  }
#if 0 
  else {
    tm_msg("A z\n");
  }
#endif


  return ptr;
}

static __inline
void tm_block_init(tm_block *b)
{
  /* Initialize. */
  tm_list_init(&b->list);
  tm_list_set_color(b, tm_LIVE_BLOCK);
#if tm_name_GUARD
  b->name = "BLOCK";
#endif
  b->type = 0;
  b->alloc = b->begin;

#if tm_block_GUARD
  b->guard1 = b->guard2 = tm_block_hash(b);
#endif

  memset(b->n, 0, sizeof(b->n));

  /* Reset block sweep iterator. */
  tm_block_sweep_init();

  /* Remember the address of the first block allocated. */
  tm.block_last = b;
  if ( ! tm.block_first ) {
    tm.block_first = b;
  } else {
    /* Make sure heap grows up, other code depends on it. */
    tm_assert((void*) b >= (void*) tm.block_first);
  }
}


static
tm_block *tm_block_alloc(size_t size)
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
      if ( bi->size == size ) {
	tm_list_remove(bi);
	b = bi;
	tm_msg("b r b%p\n", (void*) b);
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
    size_t offset;

    /* Make sure it's size aligned. */
  alloc:
    b = tm_alloc_os(size);
    if ( ! b )
      return 0;
    
    /* If it not page aligned. */
    offset = (unsigned long) b % tm_block_SIZE;
    if ( offset ) {
      /*
      ** |<--- tm_BLOCK_SIZE --->|<--- tm_BLOCK_SIZE -->|
      ** +----------------------------------------------...
      ** |<--- offset --->|<--- size      --->|
      ** |                |<-lo->|<--- sh --->|
      ** +----------------------------------------------...
      **                  ^      ^
      **                  |      |
      **                  b      nb
      ** 
      */
      tm_block *nb;
      size_t lo = tm_block_SIZE - offset; /* left over */
      size_t sh = size - lo;              /* shrink */

      tm_msg("A al %lu %ld\n", (long) tm_block_SIZE, (long) offset);
      nb = tm_alloc_os(- sh);
      if ( ! nb )
	return 0;

      /* Left over. */
      lo = (char*) nb - (char*) b;
      if ( lo >= tm_block_SIZE ) {
#if 0
	b->size = lo;
	b->begin = (char*) b + tm_block_HDR_SIZE;
	b->end = (char*) b + lo;

	tm_block_init(b);
	tm_list_set_color(b, tm_FREE_BLOCK);

	tm_list_append(&tm.free_blocks, b);
#else
	tm_msg("A al l %p[%lu]\n", b, (unsigned long) lo); 
#endif
      } else {
	tm_msg("A al l %p[%lu]\n", b, (unsigned long) lo); 
      }

      /* Make sure we are starting at block_SIZE alignment. */
      offset = (unsigned long) nb % tm_block_SIZE;
      // tm_assert(offset == 0);

      /* Try again. */
      goto alloc;
    }
    
    /* Init bounds of the block's allocation space. */
    b->begin = (char*) b + tm_block_HDR_SIZE;
    b->end = (char*) b + size;

    /* Update valid node ptr range. */
    if ( tm_ptr_l > (void*) b->begin ) {
      tm_ptr_l = b->begin;
    }
    if ( tm_ptr_h < (void*) b->end ) {
      tm_ptr_h = b->end;
    }

    /* A new block was allocated from the OS. */
    tm.n[tm_B_OS] ++;
    if ( tm.n[tm_B_OS_M] < tm.n[tm_B_OS] )
      tm.n[tm_B_OS_M] = tm.n[tm_B_OS];

    tm.n[tm_b_OS] += size;
    if ( tm.n[tm_b_OS_M] < tm.n[tm_b_OS] )
      tm.n[tm_b_OS_M] = tm.n[tm_b_OS];
  }

  b->size = size;
  tm_block_init(b);
  /* Update stats. */
  tm.n[tm_B] ++;

  // tm_validate_lists();

  return b;
}

static __inline
void tm_block_free(tm_block *b)
{
  tm_assert_test(b);
  tm_assert_test(b->type);
  tm_assert_test(tm_list_color(b) == tm_LIVE_BLOCK);

  /* Update stats */
  tm_assert_test(tm.n[tm_B]);
  tm.n[tm_B] --;

  tm_assert_test(b->type->n[tm_B]);
  b->type->n[tm_B] --;

  /* Don't allocate nodes from it any more. */
  if ( b->type->ab == b ) {
    b->type->ab = 0;
  }

  /* Dissassociate with the type. */
  b->type = 0;

  /* If b was the last block allocated from the OS: */
  if ( tm_alloc_os(0) == (void*) b->end ) {
    /* Remove from free list. */
    tm_list_remove(b);
    tm.block_last = 0;

    /* Reduce valid node ptr range. */
    tm_assert(tm_ptr_h == b->end);
    tm_ptr_h = b;

    /* Return it back to OS. */
    tm_assert_test(tm.n[tm_B_OS]);
    tm.n[tm_B_OS] --;
    tm_assert_test(tm.n[tm_b_OS] > b->size);
    tm.n[tm_b_OS] -= b->size;

    tm_alloc_os(- b->size);
  } else {
    /* Remove from t->blocks list and add to free block list. */
    tm_list_remove_and_append(&tm.free_blocks, b);
    tm_list_set_color(b, tm_FREE_BLOCK);
  }


  /* Reset block sweep iterator. */
  tm_block_sweep_init();

  // tm_validate_lists();

  tm_msg("b f b%p\n", (void*) b);
}


static tm_block * tm_block_alloc_for_type(tm_type *t)
{
  tm_block *b;
  
  /* Allocate a new block */
  b = tm_block_alloc(tm_block_SIZE);

  if ( b ) {
    /* Add to type's block list. */
    b->type = t;
    t->n[tm_B] ++;
    tm_list_insert(&t->blocks, b);
  }

  tm_msg("b a b%p t%p\n", (void*) b, (void*) t);

  return b;
}


/*********************************************************************/
/* Node creation/destruction. */

static __inline void tm_node_init(tm_node *n, tm_block *b)
{
  tm_type *t;

  tm_assert_test(b);
  tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;
  tm_list_set_color(n, tm_WHITE);
  tm_list_init(&n->list);
  
  t->n[tm_TOTAL] ++;
  t->n[tm_WHITE] ++;
  b->n[tm_TOTAL] ++;
  b->n[tm_WHITE] ++;
    
  tm.n[tm_TOTAL] ++;
  tm.n[tm_WHITE] ++;
  
  tm_node_set_color(n, b, tm_WHITE);
  
  // tm_validate_lists();

  // tm_msg("N n%p t%p\n", (void*) n, (void*) t);
}

static __inline void tm_node_delete(tm_node *n, tm_block *b)
{
  tm_type *t;
  tm_color nc = tm_node_color(n);

  tm_block_validate(b);
  tm_assert_test(b->type);
  t = b->type;
  tm_assert_test(nc == tm_WHITE);

  tm_assert_test(t->n[tm_TOTAL]);
  t->n[tm_TOTAL] --;
  tm_assert_test(t->n[nc]);
  t->n[nc] --;

  tm_assert_test(tm.n[tm_TOTAL]);
  tm.n[tm_TOTAL] --;
  tm_assert_test(tm.n[nc]);
  tm.n[nc] --;

#if 0 /* Dont bother with b, since its going back on the block free list. */
  b->n[tm_TOTAL] --;
  b->n[nc] --;
#endif

  /* Remove from what ever list its on */
  tm_list_remove(n);

  // tm_validate_lists();

  tm_msg("N d n%p[%lu] t%p\n", 
	 (void*) n,
	 (unsigned long) t->size,
	 (void*) t);
}

static __inline int tm_node_alloc_some(tm_type *t, long left)
{
  int count = 0;
  size_t bytes = 0;

  /* First block alloc? */
  do {
    tm_block *b;

    if ( ! t->ab ) {
      t->ab = tm_block_alloc_for_type(t);
      if ( ! t->ab )
	break;
    }
    b = t->ab;
    //tm_block_validate(b);

    {
      /* The next node after allocation. */
      void *pe = tm_block_node_begin(b);

      while ( (pe = tm_block_node_next(b, tm_block_node_alloc(b))) <= tm_block_node_end(b) ) {
	tm_node *n;

	// tm_block_validate(b);

	n = tm_block_node_alloc(b);

	/* Increment allocation pointer. */
	b->alloc = pe;
	
	/* Initialize the node. */
	tm_node_init(n, b);

	/* Accounting. */
	count ++;
	bytes += t->size;

	if ( -- left <= 0 ) {
	  goto done;
	}
      }

      /*
      ** Used up t->ab;
      ** Force new block allocation next time around. 
      */
      t->ab = 0;
    }
    
  done:
  } while ( ! count );

  if ( count )
    tm_msg("N i n%lu b%lu t%lu\n", count, bytes, t->n[tm_WHITE]);

  tm_assert_test(t->n[tm_WHITE]);

  return count;
}

/************************************************************************/
/* Type creation. */

static __inline tm_type *tm_type_new(size_t size)
{
  tm_type *t;

  /* Allocate new type desc. */

  /* Take from free list. */
  if ( tm.type_free ) {
    t = tm.type_free;
    tm.type_free = t->hash_next;
    t->hash_next = 0;
  } else {
    tm_assert(tm.type_free);
    t = 0;
  }

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
    
    for ( j = 0; j < sizeof(t->l)/sizeof(t->l[0]); j ++ ) {
      tm_list_init(&t->l[j]);
      tm_list_set_color(&t->l[j], j);
    }
  }
  t->ab = 0;
  t->desc = 0;

  /* Add to global types list. */
  tm_list_insert(&tm.types, t);
  
  /*
  ** To avoid triggering a collection,
  ** allocate some nodes now so the free list is not empty.
  */
  tm_node_alloc_some(t, tm_node_alloc_some_size);

  return t;
}

/****************************************************************************/
/* Structure lookup. */

static __inline tm_type *tm_block_to_type(tm_block *b)
{
  return b->type;
}

static __inline tm_block *tm_ptr_to_block(char *p)
{
  char *b;
  size_t offset;

  /*
  ** A pointer directly at the end of block should be considered
  ** a pointer into the block before it.
  */
  offset = (((unsigned long) p) % tm_block_SIZE);
  b = p - offset;
  if ( offset == 0 ) {
    b -= tm_block_SIZE;
    tm_msg("P bb p%p b%p\n", (void*) p, (void*) b);
    // tm_stop();
  }

  return (void*) b;
}

static __inline tm_node *tm_pure_ptr_to_node(void *_p)
{
  return (tm_node*) (((char*) _p) - tm_node_HDR_SIZE);
}

static __inline void *tm_node_to_ptr(tm_node *n)
{
  return (void*) (n + 1);
}

static __inline tm_block *tm_node_to_block(tm_node *n)
{
  return tm_ptr_to_block(tm_node_to_ptr(n));
}

static __inline tm_node *tm_ptr_to_node(void *p)
{
  tm_block *b;

  /* Avoid out of range pointers. */
  if ( ! (tm_ptr_l <= p && p <= tm_ptr_h) )
    return 0;

  /* Get the block and type. */
  b = tm_ptr_to_block(p);

  tm_block_validate(b);

  /* Avoid untyped blocks. */
  if ( ! b->type )
    return 0;

  /* Avoid references to block headers or outsize the allocated space. */ 
  if ( p > tm_block_node_alloc(b) )
    return 0;

  if ( p < tm_block_node_begin(b) )
    return 0;

  /* Normalize p to node head by using type size. */
  {
    unsigned long pp = (unsigned long) p;
    unsigned long node_size = tm_block_node_size(b);
    tm_node *n;
    
    /*
    ** Translate away block header.
    */
    pp -= (unsigned long) b + tm_block_HDR_SIZE;

    /*
    ** If the pointer is directly after a node boundary
    ** assume its a pointer to the node before.
   */
    {
      unsigned long node_off = pp % node_size;

      if ( node_off == 0 && pp ) {
	/*
	** 
	** node0               node1
	** +---------------...-+---------------...-+...
	** | tm_node | t->size | tm_node | t->size |
	** +---------------...-+---------------...-+...
	** ^                   ^
	** |                   |
	** new pp              pp
	*/

	pp -= node_size;

	/*
	** Translate back to block header.
	*/
	pp += (unsigned long) b + tm_block_HDR_SIZE;
	
 	tm_msg("P nb p%p p0%p\n", (void*) p, (void*) pp);

	/*
	** If the pointer in the node header
	** its not a pointer into the node data.
	*/
      } else if ( node_off < tm_node_HDR_SIZE ) {
	/*
	** 
	** node0               node1
	** +---------------...-+---------------...-+...
	** | tm_node | t->size | tm_node | t->size |
	** +---------------...-+---------------...-+...
	**                         ^
	**                         |
	**                         pp
	*/
	return 0;
      } else {
	/*
	** Remove intra-node offset.
	*/

	/*
	** 
	** node0               node1
	** +---------------...-+---------------...-+...
	** | tm_node | t->size | tm_node | t->size |
	** +---------------...-+---------------...-+...
	**                     ^            ^
	**                     |            |
	**                     new pp       pp
	*/

	pp -= node_off;

	/*
	** Translate back to block header.
	*/
	pp += (unsigned long) b + tm_block_HDR_SIZE;
      }
    }


    /* It's a node. */
    n = (tm_node*) pp;

    /* Avoid references to free nodes. */
    if ( tm_node_color(n) == tm_WHITE )
      return 0;

    /* Must be okay! */
    return n;
  }
}

static __inline tm_type *tm_node_to_type(tm_node *n)
{
  tm_block *b = tm_ptr_to_block((char*) n);
  tm_block_validate(b);
  return b->type;
}

static __inline int tm_size_hash(size_t size)
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

static __inline tm_type *tm_size_to_type_2(size_t size)
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

static __inline tm_type *tm_type_new_2(size_t size)
{
  int i;
  tm_type *t;

  t = tm_type_new(size);
  
  i = tm_size_hash(t->size);

  /* Add to type hash */
  t->hash_next = tm.type_hash[i];
  tm.type_hash[i] = t;
  
  tm_msg("t a t%p %lu\n", (void*) t, (unsigned long) size);
  
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

static __inline tm_type *tm_size_to_type(size_t size)
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
/* Stats. */

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
    tm_block_validate(b);
    tm_assert(tm_list_color(b) == tm_FREE_BLOCK);
    tm_assert(b->type == 0);
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
      tm_block_validate(b);
      tm_assert(tm_list_color(b) == tm_LIVE_BLOCK);
      tm_assert(b->type == t);
      bn[tm_B] ++;
    }
    tm_list_LOOP_END;
    tm_assert(bn[tm_B] == t->n[tm_B]);

    /* Validate colored node lists. */
    for ( j = 0; j < sizeof(t->l)/sizeof(t->l[0]); j ++ ) {
      /* Validate lists. */
      tm_assert(tm_list_color(&t->l[j]) == j);
      tm_list_LOOP(&t->l[j], node);
      {
	tm_assert(tm_node_color(node) == j);
	tn[j] ++;
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
  tm_assert(n[tm_ECRU] == tm.n[tm_ECRU]);
  tm_assert(n[tm_GREY] == tm.n[tm_GREY]);
  tm_assert(n[tm_BLACK] == tm.n[tm_BLACK]);
  tm_assert(n[tm_TOTAL] == tm.n[tm_TOTAL]);
}

static void tm_print_utilization(const char *name, tm_type *t, size_t *n, int nn, size_t *sum)
{
  int j;

  tm_msg(name);

  /* Compute total number of nodes in use. */
  if ( nn > tm_NU ) {
    switch ( tm.phase ) {
    case tm_ALLOC:
    case tm_ROOT:
    case tm_MARK:
    case tm_UNMARK:
      n[tm_NU] = n[tm_ECRU] + n[tm_GREY] + n[tm_BLACK];
      break;
      
    case tm_SWEEP:
      n[tm_NU] = n[tm_GREY] + n[tm_BLACK];
      break;
      
    default:
      tm_abort();
    }
    if ( sum && sum != n )
      sum[tm_NU] += n[tm_NU];
  }

  /* Compute total number of bytes in use. */

  if ( sum != n ) {
    if ( nn > tm_b ) {
      n[tm_b] = t ? n[tm_NU] * t->size : 0;
      if ( sum )
	sum[tm_b] += n[tm_b];
    }

    /* Compute avg. bytes / node. */
    if ( nn > tm_b_NU ) {
      n[tm_b_NU] = n[tm_NU] ? n[tm_b] / n[tm_NU] : 0;
      if ( sum )
	sum[tm_b_NU] = sum[tm_NU] ? sum[tm_b] / sum[tm_NU] : 0;
    }

  }

  /* Print fields. */
  for ( j = 0; j < nn; j ++ ) {
    if ( sum && sum != n && j <= tm_B ) {
      sum[j] += n[j];
    }
    tm_msg1("%c%-4lu ", tm_color_name[j][0], (unsigned long) n[j]);
  }

  tm_msg1("\n");
}

void tm_print_stats()
{
  tm_type *t;
  size_t sum[tm__LAST3];

  memset(sum, 0, sizeof(sum));

  tm_msg_enable("X", 1);

  tm_msg("X { t tb%lu[%lu]\n",
	 tm.n[tm_B],
	 tm.n[tm_B] * tm_block_SIZE
	 );

  tm_list_LOOP(&tm.types, t);
  {
    tm_msg("X  t%p S%-6lu\n", (void*) t, (unsigned long) t->size, (unsigned) t->n[tm_B]);
    tm_print_utilization("X    ", t, t->n, sizeof(t->n)/sizeof(t->n[0]), sum);
  }
  tm_list_LOOP_END;

  /* current/max number of blocks/bytes allocated from OS is in tm.n[]. */
  sum[tm_B_OS] = tm.n[tm_B_OS];
  sum[tm_b_OS] = tm.n[tm_b_OS];
  sum[tm_B_OS_M] = tm.n[tm_B_OS_M];
  sum[tm_b_OS_M] = tm.n[tm_b_OS_M];

  tm_print_utilization("X  S ", 0, sum, sizeof(sum)/sizeof(sum[0]), sum);

  tm_msg("X }\n");

  tm_msg_enable("X", 0);

  tm_print_time_stats();

  //tm_validate_lists();
}

void tm_print_block_stats()
{
  tm_type *t;

  tm_block *b;

  tm_msg_enable("X", 1);

  tm_msg("X { b tb%lu[%lu]\n",
	 tm.n[tm_B],
	 tm.n[tm_B] * tm_block_SIZE
	 );

  tm_list_LOOP(&tm.types, t);
  {
    tm_msg("X   t%p s%lu \n", t, (unsigned long) t->size);
    
    tm_list_LOOP(&t->blocks, b);
    {
      int j;

      tm_block_validate(b);

      tm_msg("X    b%p s%lu ", (void*) b, (unsigned long) b->size);

      for ( j = 0; j < sizeof(b->n)/sizeof(b->n[0]); j ++ ) {
	tm_msg1("%c%-4lu ", tm_color_name[j][0], (unsigned long) b->n[j]);
      }
      
      tm_msg1("\n");
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;

  tm_msg("X }\n");

  tm_msg_enable("X", 0);
}



/***************************************************************************/
/* Marking */

static int tm_node_mark(tm_node *n)
{
  switch ( tm_node_color(n) ) {
  case tm_WHITE:
    /* Spurious pointer? */
    tm_abort();
    break;

  case tm_ECRU:
    /* The node has not been scheduled for marking; */
    /* schedule it for marking. */
    tm_node_set_color(n, tm_node_to_block(n), tm_GREY);
    tm_msg("m n%p\n", n);
    return 1;
    break;
    
  case tm_GREY:
    /* The node has already been scheduled for marking. */
    break;
    
  case tm_BLACK:
    /* The node has already been marked. */
    break;

  default:
    tm_abort();
    break;
  }

  return 0;
}

static tm_node * tm_possible_ptr_mark(void *p)
{
  tm_node *n = tm_ptr_to_node(p);
  if ( n && tm_node_mark(n) )
    return n;
    
  return 0;
}


/***************************************************************************/
/* root mark */

static void tm_range_mark(const void *b, const void *e);

static void tm_root_mark_id(int i)
{
  if ( tm.roots[i].l < tm.roots[i].h ) {
    tm_msg("r [%p,%p] %s\n", tm.roots[i].l, tm.roots[i].h, tm.roots[i].name);
    tm_range_mark(tm.roots[i].l, tm.roots[i].h);
  }
}


static void tm_register_mark()
{
  tm_root_mark_id(0);
}

void tm_set_stack_ptr(void *stackvar)
{
  *tm.stack_ptrp = (char*) stackvar - 64;
}

static void tm_stack_mark()
{
  tm_register_mark();
  tm_root_mark_id(1);
  tm.stack_mutations = 0;
}

static void tm_root_mark_all()
{
  int i;

  tm_msg("r G%lu B%lu {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  for ( i = 0; tm.roots[i].name; i ++ ) {
    tm_root_mark_id(i);
  }
  tm.data_mutations = tm.stack_mutations = 0;
  tm_root_loop_init();
  tm_msg("r G%lu B%lu }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
}


static int tm_root_mark_some()
{
  int result = 1;
  long left = tm_root_mark_some_size;

  tm_msg("r G%lu B%lu {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  tm_msg("r [%p,%p] %s\n", 
	 tm.roots[tm.rooti].l, 
	 tm.roots[tm.rooti].h,
	 tm.roots[tm.rooti].name 
	 );

  do {
    /* Try marking some roots. */
    while ( (void*) (tm.rp + sizeof(void*)) >= tm.roots[tm.rooti].h ) {
      tm.rooti ++;
      if ( ! tm.roots[tm.rooti].name ) {
	tm_root_loop_init();
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

    tm_possible_ptr_mark(* (void**) tm.rp);

    tm.rp += tm_PTR_ALIGN;

    left -= tm_PTR_ALIGN;
  } while ( left > 0 );

 done:
  tm_msg("r G%lu B%lu }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);

  return result; /* We're not done. */
}

/***************************************************************************/
/* Marking */

static void tm_range_mark(const void *b, const void *e)
{
  const char *p;

  for ( p = b; ((char*) p + sizeof(void*)) <= (char*) e; p += tm_PTR_ALIGN ) {
    tm_node *n;

    if ( (n = tm_possible_ptr_mark(* (void**) p)) ) {
      tm_msg("M n%p p%p\n", n, p);
    }
  }
}

static __inline void tm_node_mark_interior(tm_node *n, tm_block *b)
{
  tm_assert_test(tm_node_to_block(n) == b);
  tm_assert_test(tm_node_color(n) == tm_GREY);

  /* Move to marked list. */
  tm_node_set_color(n, b, tm_BLACK);

  /* Mark all possible pointers. */
  {
    const char *ptr = tm_node_to_ptr(n);
    tm_range_mark(ptr, ptr + b->type->size);
  }
}


static size_t tm_node_mark_some(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_GREY] ) {
    tm_node_LOOP(tm_GREY);
    {
      tm_node_mark_interior(n, tm_node_to_block(n));
      count ++;
      bytes += t->size;
      if ( (left -= t->size) <= 0 ) {
	tm_node_LOOP_BREAK();
      }
    }
    tm_node_LOOP_END(tm_GREY);
  }

  if ( count )
    tm_msg("M c%lu b%lu l%lu\n", count, bytes, tm.n[tm_GREY]);

  return tm.n[tm_GREY];
}

static void tm_node_mark_all()
{
  while ( tm.n[tm_GREY] ) {
    tm_node_LOOP_INIT(tm_GREY);
    tm_node_mark_some(tm.n[tm_TOTAL]);
  }
}


/***************************************************************************/
/* Sweeping */

#ifndef tm_sweep_is_error
int tm_sweep_is_error = 0;
#endif

static int tm_check_sweep_error()
{
  tm_type *t;
  tm_node *n;

  if ( tm_sweep_is_error ) {
    if ( tm.n[tm_ECRU] ) {
      tm_msg("Fatal %lu dead nodes; there should be no sweeping.\n", tm.n[tm_ECRU]);
      tm_stop();
      // tm_validate_lists();

      tm_list_LOOP(&tm.types, t);
      {
	tm_list_LOOP(&t->l[tm_ECRU], n);
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
      tm_phase_init(tm_ROOT);
      tm_root_mark_all();
      tm_phase_init(tm_MARK);
      tm_node_mark_all();

      if ( tm.n[tm_ECRU] ) {
	tm_msg("Fatal after root mark: still missing %lu references.\n",
	       (unsigned long) tm.n[tm_ECRU]);
      } else {
	tm_msg("Fatal after root mark: OK, missing references found!\n");
      }

      tm_list_LOOP(&tm.types, t);
      {
	tm_list_LOOP(&t->l[tm_ECRU], n);
	{
	  tm_node_set_color(n, tm_node_to_block(n), tm_BLACK);
	}
	tm_list_LOOP_END;
      }
      tm_list_LOOP_END;

      tm_phase_init(tm_ALLOC);
      tm_assert_test(tm.n[tm_ECRU] == 0);

      tm_stop();

      /* Clear worst alloc time. */
      memset(&tm.ts_alloc.tw, 0, sizeof(tm.ts_alloc.tw));

      return 1;
    }
  }
  return 0;
}

/*********************************************************************/

static __inline void tm_node_sweep(tm_node *n, tm_block *b)
{
  /* Make sure the node is unmarked. */
  tm_assert_test(tm_node_color(n) == tm_ECRU);
  tm_block_validate(b);

  /* Clear out data. */
  memset(tm_node_to_ptr(n), 0, b->type->size);

  /* Put it on the WHITE (free) list. */
  tm_node_set_color(n, b, tm_WHITE);
  
  tm_msg("s n%p\n", n);
}

static long tm_node_sweep_some(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_ECRU] ) {
    if ( tm_check_sweep_error() )
      return 0;
  
    tm_node_LOOP(tm_ECRU);
    {
      tm_node_sweep(n, tm_node_to_block(n));
      bytes += t->size;
      count ++;
      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK();
      }
    }
    tm_node_LOOP_END(tm_ECRU);
  }

  if ( count ) 
    tm_msg("s c%lu b%lu l%lu\n", count, bytes, tm.n[tm_ECRU]);

  return tm.n[tm_ECRU];
}

static size_t tm_node_sweep_some_for_type(tm_type *t)
{
  size_t count = 0, bytes = 0;
  tm_node *n;
  long left = 10;

  if ( t->n[tm_ECRU] ) {
    /* Move the global sweep pointer away from t. */
    if ( tm.tp[tm_ECRU] == t ) {
      tm.tp[tm_ECRU] = tm_list_next(t);
      tm.np[tm_ECRU] = tm_list_next(&tm.tp[tm_ECRU]->l[tm_ECRU]);
    }

    while ( (n = tm_list_first(&t->l[tm_ECRU])) ) {
      tm_node_sweep(n, tm_node_to_block(n));
      bytes += t->size;
      count ++;
      if ( -- left <= 0 ) {
	break;
      }
    }
  }

  if ( count ) 
    tm_msg("s t%p c%lu b%lu l%lu\n", t, count, bytes, tm.n[tm_ECRU]);

  return tm.n[tm_ECRU];
}

static void tm_nodes_sweep_all()
{
  while ( tm.n[tm_ECRU] ) {
    tm_node_LOOP_INIT(tm_ECRU);
    tm_node_sweep_some(tm.n[tm_TOTAL]);
  }
}

static int tm_block_sweep_maybe(tm_block *b, int *count, unsigned long *bytes)
{
  tm_block_validate(b);

  do {
#if 0
    tm_msg("V: t %s %p, b %s %p\n", 
	   tm_struct_name[tm_list_color(tm.bt)], tm.bt, 
	   tm_struct_name[tm_list_color(tm.bb)], tm.bb);
#endif
    
    if ( tm_warn(tm_list_next(b) != b) ) {
      tm_block_sweep_init();
      break;
    }
    if ( tm_warn(tm_list_color(b) == tm_LIVE_BLOCK, "b = %p", b) )
      continue;

    tm_assert_test(tm_list_color(b) == tm_LIVE_BLOCK);

    /*
    ** If all nodes in the block are free, 
    ** remove all nodes in the block from any lists.
    */
    if ( b->n[tm_WHITE] == b->n[tm_TOTAL] ) {
      tm_node *n;
      
      /* First block. */
      n = tm_block_node_begin(b);
      while ( (void*) n < tm_block_node_alloc(b) ) {
	/* Remove and advance. */
	tm_node_delete(n, b);
	n = tm_block_node_next(b, n);
      }
      
      (*count) ++;
      (*bytes) += b->size;

      /* Free the block. */
      tm_block_free(b);

#if 1
      /* The block was entirely cleaned, so quit now. */
      return 1;
#endif
    }
  } while ( 0 );
    
  return 0;
}

static int tm_block_sweep_some()
{
  int count = 0;
  unsigned long bytes = 0;
  int left = tm_block_sweep_some_size;

  if ( left > tm.n[tm_B] )
    left = tm.n[tm_B];

  /*
  ** Try to sweep last block allocated from OS first
  ** because we might be able to return it to the OS.
  */
  if ( tm.block_last && tm_block_sweep_maybe(tm.block_last, &count, &bytes) ) {
    left --;
  }

  while ( left -- > 0 ) {
    /* Try next block. */
    if ( tm.bb == 0 || (void*) tm.bb == (void*) &tm.bt->blocks ) {
      tm.bt = tm_list_next(tm.bt);
      if ( (void*) tm.bt == (void*) &tm.types ) {
	tm_block_sweep_init();
      } else {
	tm.bb = tm_list_next(&tm.bt->blocks);
      }
      left --;
      continue;
    }

    /* Is the current block totally free? */
    {
      tm_block *b = tm.bb;

      tm.bb = tm_list_next(b);

#if 1
      if ( tm_warn(b->type == tm.bt, "b = %p, b->type = %p, tm.bt = %p", b, b->type, tm.bt) )
	continue;
      tm_assert_test(b->type == tm.bt);
#endif

      if ( tm_block_sweep_maybe(b, &count, &bytes) ) {
	left = 0;
	break;
      }
    }
  }

  if ( count ) 
    tm_msg("b s b%lu b%lu\n", (unsigned long) count, (unsigned long) bytes);

  return count;
}


/***************************************************************************/
/* Unmarking */

static size_t tm_node_unmark_some(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_BLACK] ) {
    tm_node_LOOP(tm_BLACK);
    {
      tm_assert_test(tm_node_color(n) == tm_BLACK);
      tm_node_set_color(n, tm_node_to_block(n), tm_ECRU);
      bytes += t->size;
      count ++;
      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK();
      }
    }
    tm_node_LOOP_END(tm_BLACK);
  }

  if ( count ) 
    tm_msg("u n%lu b%lu l%lu\n", count, bytes, tm.n[tm_BLACK]);

  return tm.n[tm_BLACK];
}

static void tm_node_unmark_all()
{
  while ( tm.n[tm_BLACK] ) {
    tm_node_LOOP_INIT(tm_BLACK);
    tm_node_unmark_some(tm.n[tm_TOTAL]);
  }
}

/***************************************************************************/
/* full gc */

void tm_gc_full_inner()
{
  int try = 0;

  tm_msg("gc {\n");

  tm_sweep_is_error = 0;

  while ( try ++ < 2 ) {
    /* Mark all roots. */
    tm_phase_init(tm_ROOT);
    tm_root_mark_all();
    
    /* Mark all scheduled nodes. */
    tm_phase_init(tm_MARK);
    tm_node_mark_all();
    tm_assert_test(tm.n[tm_GREY] == 0);
    
    /* Sweep the nodes. */
    tm_phase_init(tm_SWEEP);
    tm_nodes_sweep_all();
    tm_assert_test(tm.n[tm_ECRU] == 0);
    
    /* Unmark the nodes. */
    tm_phase_init(tm_UNMARK);
    tm_node_unmark_all();
    tm_assert_test(tm.n[tm_BLACK] == 0);

    /* Sweep some blocks? */
    while ( tm_block_sweep_some() )
      ;

    tm_phase_init(tm_ALLOC);
  }

  tm_msg("gc }\n");
}

/***************************************************************************/
/* write barrier */


/*******************************************************************************/

/*
** tm_root_write() should be called after modifing a root ptr.
*/
void tm_mark(void *ptr)
{
  tm_possible_ptr_mark(ptr);
}

static void __tm_write_root_ignore(void *referent)
{
  tm_abort();
  /* DO NOTHING */
}

static void __tm_write_root_root(void *referent)
{
  tm_abort();
  tm_mark(* (void**) referent);
  tm_msg("w r p%p\n", referent);
}

static void __tm_write_root_mark(void *referent)
{
  tm_abort();
  tm_mark(* (void**) referent);
  tm_msg("w r p%p\n", referent);
}

static void __tm_write_root_sweep(void *referent)
{
  tm_abort();
  tm_msg("w r p%p\n", referent);
}


/*******************************************************************************/
/* General write barrier routine. */

/*
** tm_write_barrier(ptr) must be called after any ptrs
** within the node n are mutated.
*/
static __inline void tm_write_barrier_node(tm_node *n)
{
  switch ( tm_node_color(n) ) {
  case tm_GREY:
    /* All ready marked. */
    break;

  case tm_ECRU:
    /* Not reached yet. */
    break;

  case tm_BLACK:
    /*
    ** The node has already been marked. 
    ** The mutator may have introduced new pointers.
    ** Reschedule it to be remarked.
    */
    tm_node_set_color(n, tm_node_to_block(n), tm_GREY);
    tm_msg("w b n%p\n", n);
    break;

  default:
    tm_abort();
    break;
  }
}

static void __tm_write_barrier_ignore(void *ptr)
{
  /* DO NOTHING */
}

static void __tm_write_barrier_root(void *ptr)
{
  /* Currently marking roots.
  ** Don't know if this root has been marked yet or not.
  */

  /*
  ** If the ptr is a reference to a stack allocated object,
  ** do nothing, because the stack will be scanned atomically before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    tm.stack_mutations ++; 
    tm_msg("w s p%p\n", ptr);
    return;
  }

  /*
  ** If the ptr is a reference to the heap. 
  ** Do nothing, because we haven't started marking yet.
  */
  if ( tm_ptr_l <= ptr && ptr <= tm_ptr_h )
    return;

  /*
  ** Must be a pointer to statically allocated (root) object.
  ** Mark the root as being mutated.
  */
  tm.data_mutations ++;
  tm_msg("w r p%p\n", ptr);
}

static void __tm_write_barrier_mark(void *ptr)
{
  tm_node *n;

  tm_assert_test(tm.phase == tm_MARK);

  /*
  ** If the ptr is a reference to a stack allocated object,
  ** do nothing, because the stack will be marked before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    tm.stack_mutations ++; 
    // tm_msg("w s p%p\n", ptr);
    return;
  }

  /*
  ** If the ptr is a reference to a node,
  ** schedule it for remarking if it has already been marked.
  **
  ** If its not a reference to a node.
  ** It must be a reference to a statically allocated object.
  ** This means the data root set has (probably) been mutated.
  ** Must flag the roots for remarking before the sweep phase.
  **
  */
  if ( (n = tm_ptr_to_node(ptr)) ) {
    tm_write_barrier_node(n);
  } else {
    tm.data_mutations ++;
    tm_msg("w r p%p\n", ptr);
  }
}

static void __tm_write_barrier_sweep(void *ptr)
{
  tm_node *n;

  tm_assert_test(tm.phase == tm_SWEEP);

  /*
  ** If the ptr is a reference to a stack allocated object,
  ** do nothing, because the stack will be marked before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h ) {
    tm.stack_mutations ++;
    // tm_msg("w s p%p\n", ptr);
    return;
  }

  /*
  ** If the ptr is a reference to a node,
  ** schedule it for remarking if it has already been marked.
  **
  ** If its not a reference to a node,
  ** it must be a reference to a statically allocated object.
  ** This means the data root set has (probably) been mutated.
  ** Must flag the roots for remarking before the sweep phase.
  **
  */
  if ( (n = tm_ptr_to_node(ptr)) ) {
    tm_write_barrier_node(n);
  } else {
    tm.data_mutations ++;
    tm_msg("w r p%p\n", ptr);
  }
}

/*******************************************************************************/
/* the tm_write_barrier_pure(ptr) assumes ptr is directly from tm_alloc. */

static void __tm_write_barrier_pure_ignore(void *ptr)
{
  /* DO NOTHING */
}

static void __tm_write_barrier_pure_root(void *ptr)
{
  /* DO NOTHING */
}

static void __tm_write_barrier_pure_mark(void *ptr)
{
  tm_assert_test(tm.phase == tm_MARK);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}

static void __tm_write_barrier_pure_sweep(void *ptr)
{
  tm_assert_test(tm.phase == tm_SWEEP);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}

/***************************************************************************/
/* allocation */

int tm_alloc_pass_max = 0;

static __inline void *tm_node_alloc_free(tm_type *t)
{
  tm_node *n;

  if ( (n = tm_list_first(&t->l[tm_WHITE])) ) {
    char *ptr;

    /* It better be free... */
    tm_assert_test(tm_node_color(n) == tm_WHITE);

    /* A a ptr to node's data space. */
    ptr = tm_node_to_ptr(n);

    /* Clear data space. */
    memset(ptr, 0, t->size);
    
    /* Put it on the appropriate allocated list. */
    tm_node_set_color(n, tm_node_to_block(n), tm_phase_alloc[tm.phase]);

    tm_msg("N a n%p[%lu] t%p\n", 
	   (void*) n, 
	   (unsigned long) t->size,
	   (void*) t);

    return ptr;
  } else {
    return 0;
  }
}


void tm_free_inner(void *ptr)
{
  tm_block *b;
  tm_node *n;

  n = tm_pure_ptr_to_node(ptr);
  b = tm_node_to_block(n);
  tm_block_validate(b);
  tm_assert(tm_node_color(n) != tm_WHITE);
  tm_node_set_color(n, b, tm_WHITE);
}


void *tm_alloc_type_inner(tm_type *t)
{
  void *ptr;

  tm_alloc_id ++;
  if ( tm_alloc_id == 5040 )
    tm_stop();

  tm_alloc_pass = 0;
  memset(tm.alloc_n, 0, sizeof(tm.alloc_n));

  tm.next_phase = (enum tm_phase) -1;
  
  // tm_validate_lists();

  //try_again:
  tm_alloc_pass ++;

#if tm_TIME_STAT
  tm_time_stat_begin(&tm.ts_phase[tm.phase]);
#endif

  switch ( tm.phase ) {
  case tm_ALLOC:
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
    /* Scan some roots for pointers. */
    if ( ! tm_root_mark_some() ) {
      tm.next_phase = tm_MARK;
    }
    break;
    
  case tm_MARK:
    //mark:
    /* Mark a little bit. */
    if ( ! tm_node_mark_some(tm_node_mark_some_size) ) {
      /* 
      ** If the roots were mutated during mark,
      ** remark all roots.
      ** Rationale: A reference to a new node may have been
      ** copied from the unmarked stack to a global root.
      */
      if ( tm_root_mark_full || tm.data_mutations || tm.stack_mutations ) {
	/* Mark all roots. */
	tm_root_mark_all();
      } else {
	/* Mark the stack. */
	tm_stack_mark();
      }

      /* 
      ** If after marking the register and stack,
      ** no additional nodes were marked,
      ** begin sweeping.
      */
      if ( ! tm_node_mark_some(tm_node_mark_some_size) ) {
	/* Force SWEEP here so new node gets allocated as GREY. */
	tm_phase_init(tm_SWEEP);
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

    /* If there are pending marks, do them now. 
    ** Rational: the grey nodes may have new references in them.
    */
    if ( tm.n[tm_GREY] ) {
      tm_node_mark_some(tm_node_mark_some_size);
      if ( ! tm.n[tm_GREY] ) {
      sweep:
	/* Try to immediately sweep nodes for the type we want first. */
	tm_node_sweep_some_for_type(t);
	if ( ! tm_node_sweep_some(tm_node_sweep_some_size) ) {
	  tm.next_phase = tm_UNMARK;
	}
      }
    } else {
      goto sweep;
    }
    break;
    
  case tm_UNMARK:
    if ( ! tm_node_unmark_some(tm_node_unmark_some_size) ) {
      tm.next_phase = tm_ALLOC;

      /* Attempt to sweep some blocks. */
      if ( tm_block_sweep_some() ) {
	/*
	** If a block was swept, 
	** make sure the free list for t is filled.
	*/
	tm_msg("b swept block allocated for type %p\n", t);
	tm_node_alloc_some(t, 1);
      }
    }
    break;

  default:
    tm_abort();
    break;
  }
  
#if tm_TIME_STAT
  tm_time_stat_end(&tm.ts_phase[tm.phase]);
#endif

  /* Switch to new phase. */
  if ( tm.next_phase != (enum tm_phase) -1 )
    tm_phase_init(tm.next_phase);

  /* Take one from the free list. */
  if ( ! (ptr = tm_node_alloc_free(t)) ) {
    /* We must attempt to allocate from os. */
    tm_node_alloc_some(t, tm_node_alloc_some_size);
    ptr = tm_node_alloc_free(t);
    tm_assert_test(ptr);
  }  

  tm_msg("a %p[%lu]\n", ptr, (unsigned long) t->size);

  // tm_validate_lists();

  /* Validate tm_ptr_to_node() */
  if ( ptr ) {
    char *p = ptr;
    tm_node *n = (void*) (p - tm_node_HDR_SIZE);
    tm_assert_test(tm_ptr_to_node(p) == n);
    tm_assert_test(tm_ptr_to_node(p + t->size / 2) == n);
    tm_assert_test(tm_ptr_to_node(p + t->size) == n);
  }

  return (void*) ptr;
}

void *tm_alloc_inner(size_t size)
{
  return tm_alloc_type_inner(tm_size_to_type(size));  
}

void *tm_alloc_desc_inner(tm_adesc *desc)
{
  return tm_alloc_type_inner((tm_type*) desc->hidden);
}

void *tm_realloc_inner(void *oldptr, size_t size)
{
  char *ptr = 0;
  tm_type *t, *oldt;

  oldt = tm_node_to_type(tm_pure_ptr_to_node(oldptr));
  t = tm_size_to_type(size);

  if ( oldt == t ) {
    ptr = oldptr;
  } else {
    ptr = tm_alloc_inner(size);
    memcpy(ptr, oldptr, size < oldt->size ? size : oldt->size);
  }
  
  return (void*) ptr;
}

/***************************************************************************/
/* user level routines */

/* Avoid leaving garbage on the stack. */
/* Don't put this in user.c, so it cannot be optimized away. */

void _tm_clear_some_stack_words()
{
  int some_words[64];
  memset(some_words, 0, sizeof(some_words));
}

/***************************************************************************/


