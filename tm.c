/* $Id: tm.c,v 1.3 1999-06-16 08:35:07 stephensk Exp $ */
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

long tm_scan_some_roots_size = 2048;
long tm_scan_some_nodes_size = 2048;
long tm_sweep_some_nodes_size = 100;
long tm_unmark_some_nodes_size = 100;
long tm_alloc_some_nodes_size = 10;

int tm_marking = 0;
int tm_sweeping = 0;

FILE *tm_msg_file = 0;

static const char *_tm_msg_enable = "bwtNFg*";

/****************************************************************************/

static const char *tm_color_name[] = {
  "WHITE",
  "ECRU",
  "GREY",
  "BLACK",
  "TOTAL",
  "b",
  "D",
  "d",
  "M",
  "m",
  "W",
  "w",
  0
};

static const char *tm_phase_name[] = {
  "ALLOC",
  "ROOTS",
  "SCAN",
  "SWEEP",
  "UNMARK",
  0
};

#define tm_EALL tm_ECRU
//#define tm_EALL tm_GREY

static const tm_color tm_phase_alloc[] = {
  tm_EALL,  /* ROOTS */
  tm_EALL,  /* ALLOC */
  tm_EALL,  /* SCAN */
  tm_BLACK, /* SWEEP */
  tm_EALL   /* UNMARK */
};


struct tm_data tm;


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

static unsigned long tm_alloc_id = 0;
static unsigned long tm_alloc_pass = 0;

static char _tm_msg_enable_table[256];

static int _tm_msg_ignored = 0;

void tm_msg(const char *format, ...)
{
  va_list vap;

  if ( (_tm_msg_ignored = ! _tm_msg_enable_table[(unsigned char) format[0]]) )
    return;

  if ( ! tm_msg_file )
    tm_msg_file = stdout;

  fprintf(tm_msg_file, "tm: %3lu: ", tm_alloc_id);
  if ( tm_alloc_pass > 1 )
    fprintf(tm_msg_file, "(pass %lu): ", tm_alloc_pass);

  va_start(vap, format);
  vfprintf(tm_msg_file, format, vap);
  va_end(vap);

  // fgetc(stdin);
}

void tm_msg1(const char *format, ...)
{
  va_list vap;

  if ( _tm_msg_ignored )
    return;

  va_start(vap, format);
  vfprintf(tm_msg_file, format, vap);
  va_end(vap);
}

/****************************************************************************/

void tm_stop()
{
}

void tm_fatal()
{
  tm_msg("Fatal: Aborting!\n");

  abort();
}

void tm_abort()
{
  static int in_abort;

  in_abort ++;

  if ( in_abort == 1 ) {
    tm_print_stats();
  }

  in_abort --;

  tm_fatal();
}

void _tm_assert(const char *expr, const char *file, int lineno)
{
  tm_msg("\n");
  tm_msg("Fatal: assertion \"%s\" failed: %s: %d: \n", expr, file, lineno);

  tm_abort();
}
#define tm_assert(X) do { if ( ! (X) ) _tm_assert(#X, __FILE__, __LINE__); } while(0)

//#define tm_assert_test(X) (void)0
#define tm_assert_test(X) tm_assert(X)

 
/****************************************************************************/
/* Init. */

extern int _data_start__, _data_end__, _bss_start__, _bss_end__;

static void _tm_add_root(const char *name, const void *l, const void *h)
{
  if ( l >= h )
    return;

  tm.roots[tm.nroots].name = name;
  tm.roots[tm.nroots].l = l;
  tm.roots[tm.nroots].h = h;
  tm_msg("Root: added %s %d [%p, %p]\n", tm.roots[tm.nroots].name, tm.nroots, tm.roots[tm.nroots].l, tm.roots[tm.nroots].h);
  tm.nroots ++;
}

static void tm_add_root(const char *name, const void *_l, const void *_h)
{
  const void *l = _l, *h = _h;
  const void *tm_l, *tm_h;

  tm_l = &tm;
  tm_h = (&tm) + 1;

  if ( l <= tm_l && tm_l < h ) {
    _tm_add_root(name, l, tm_l);
    l = tm_h;
  }
  if ( l <= tm_l && tm_l < h ) {
    _tm_add_root(name, tm_h, h);
    l = h;
  }

  _tm_add_root(name, l, h);
}


/****************************************************************************/
/* Phase support. */

static void tm_scan_registers();
static void tm_scan_stacks();

static void __tm_write_root_ignore(void *referent);
static void __tm_write_root_root(void *referent);
void (*_tm_write_root)(void **refp) = __tm_write_root_ignore;

static void __tm_write_barrier_ignore(void *referent);
static void __tm_write_barrier_root(void *referent);
static void __tm_write_barrier_scan(void *referent);
void (*_tm_write_barrier)(void *referent) = __tm_write_barrier_ignore;

static void __tm_write_barrier_pure_ignore(void *referent);
static void __tm_write_barrier_pure_scan(void *referent);
void (*_tm_write_barrier_pure)(void *referent) = __tm_write_barrier_pure_ignore;

static void tm_init_phase(int p)
{
  switch ( (tm.phase = p) ) {
  case tm_ALLOC:
    tm_marking = 0;
    tm_sweeping = 0;

    _tm_write_root = __tm_write_barrier_root;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_ROOTS:
    tm.rooti = 2;
    tm.rp = tm.roots[tm.rooti].l;

    tm_marking = 0;
    tm_sweeping = 0;

    /* Enable root write barrier. */
    _tm_write_root = __tm_write_root_root;
    _tm_write_barrier = __tm_write_barrier_root;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_SCAN:
    /* Set up for marking and scanning. */
    tm_node_LOOP_INIT(tm_GREY);

    tm_marking = 1;
    tm_sweeping = 0;

    /* Enable write barrier. */
    _tm_write_root = __tm_write_root_root;
    _tm_write_barrier = __tm_write_barrier_scan;
    _tm_write_barrier_pure = __tm_write_barrier_pure_scan;
    break;

  case tm_SWEEP:
    /* Set up for sweeping. */
    tm_node_LOOP_INIT(tm_ECRU);

    tm_marking = 0;
    tm_sweeping = 1;

    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  case tm_UNMARK:
    /* Set up for unmarking. */
    tm_node_LOOP_INIT(tm_BLACK);

    tm_marking = 0;
    tm_sweeping = 0;

    _tm_write_root = __tm_write_root_ignore;
    _tm_write_barrier = __tm_write_barrier_ignore;
    _tm_write_barrier_pure = __tm_write_barrier_pure_ignore;
    break;

  default:
    tm_fatal();
    break;
  }

  tm_msg("p: new phase %s\n", tm_phase_name[p]);
  if ( 1 || p == tm_SWEEP ) {
    // tm_print_stats();
  }
  //tm_validate_lists();
}


void tm_init(int *argcp, char ***argvp, char ***envpp)
{
  int i;

  /* Msg ignore table. */
  {
    const unsigned char *r;

    for ( r = _tm_msg_enable; *r; r ++ ) {
      if ( *r == '\1' ) {
	memset(_tm_msg_enable_table, 1, sizeof(_tm_msg_enable_table));
      } else {
	_tm_msg_enable_table[*r] = 1;
      }
    }
    _tm_msg_enable_table[' '] = 1;
    _tm_msg_enable_table['\t'] = 1;
    _tm_msg_enable_table['\n'] = 1;
    _tm_msg_enable_table['\r'] = 1;

  }

  /* Possible pointer range. */
  tm_ptr_l = (void*) ~0UL;
  tm_ptr_h = 0;

  /* Roots. */
  tm.nroots = 0;
  _tm_add_root("registers", tm.jb, (&tm.jb) + 1);
  _tm_add_root("stack", &i, envpp);
  tm_add_root("initialized data", &_data_start__, &_data_end__);
  tm_add_root("uninitialized data", &_bss_start__, &_bss_end__);

  /* Root scanning. */
  tm.rooti = 2;
  tm.rp = tm.roots[tm.rooti].l;

  /* Global lists. */
  tm_list_init(&tm.types);
  tm_list_init(&tm.free_blocks);

  /* Types. */
  /* Type desc free list. */
  for ( i = 0; i < sizeof(tm.type_reserve)/sizeof(tm.type_reserve[0]); i ++ ) {
    tm_type *t = &tm.type_reserve[i];
    t->hash_next = (void*) tm.type_free;
    tm.type_free = t;
  }
  
  /* Type desc hash table. */
#if 0
  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm.type_hash[i] = 0;
  }
#endif

  /* Start by allocating. */
  tm_init_phase(tm_ALLOC);
}

/***************************************************************************/
/* Node color mgmt. */

#define tm_node_color(n) ((tm_color) tm_list_color(n))

static __inline void _tm_node_set_color(tm_node *n, tm_type *t, tm_color c)
{
  tm_assert_test(c <= tm_BLACK);

  tm_assert_test(tm.n[tm_node_color(n)]);
  tm.n[tm_node_color(n)] --;
  tm.n[c] ++;

  // tm_alloc() stats.
  tm.alloc_n[c] ++;
  tm.alloc_n[tm_TOTAL] ++;

  t->n[c] ++;
  tm_assert_test(t->n[tm_node_color(n)]);
  t->n[tm_node_color(n)] --;

#if 0
  /* Invalidate the "to" color iterator. */
  tm_node_LOOP_INIT(c);
#endif

  tm_list_set_color(n, c);
}

static __inline void tm_node_set_color(tm_node *n, tm_type *t, tm_color c)
{
  tm_assert_test(t);

  _tm_node_set_color(n, t, c);

  tm_list_remove_and_insert(&t->l[c], n);
}

/****************************************************************************/
/* Low-level alloc. */

void *tm_alloc_system(size_t size)
{
  return sbrk(size);
}

tm_block *tm_alloc_block(size_t size)
{
  tm_block *b;
  size_t offset;

  if ( (b = tm_list_take_first(&tm.free_blocks)) ) {
    tm_msg("block: %p reused\n", (void*) b);
  } else {
    /* Force alignment to a block! */
    b = tm_alloc_system(0);
    offset = (unsigned long) b;
    offset &= tm_block_SIZE - 1;
    if ( offset ) {
      tm_alloc_system(offset);
    }
    
    /* Force allocation to a multiple of a block! */
    size += tm_block_SIZE - 1;
    size &= ~(tm_block_SIZE - 1);
    
    b = tm_alloc_system(size);
    offset = (size_t) b;
    tm_assert((offset & (tm_block_SIZE - 1)) == 0);
    
    /* Remember the address of the first block allocated. */
    if ( ! tm.block_base ) {
      tm.block_base = b;
    }
  }

  /* Initialize. */
  tm.n[tm_B] ++;
  b->size = size;
  tm_list_set_color(b, tm_WHITE);
  b->type = 0;
  b->alloc = (char*) b + tm_block_HDR_SIZE;
  tm_list_init(&b->list);

  return b;
}

void tm_free_block(tm_block *b)
{
  tm_assert_test(tm.n[tm_B]);
  tm.n[tm_B] --;
  tm_list_remove_and_insert(&tm.free_blocks, b);
  tm_msg("block: %p free\n", (void*) b);
}


static tm_block * tm_alloc_new_type_block(tm_type *t)
{
  tm_block *b;
  
  /* Allocate a new block */
  b = tm_alloc_block(tm_block_SIZE);

  /* Mark it as in use. */
  tm_list_set_color(b, tm_ECRU);

  /* Add to type's block list. */
  b->type = t;
  t->n[tm_B] ++;
  tm_list_insert(&t->blocks, b);

  tm_msg("block: %p allocated for type %p\n", (void*) b, (void*) t);

  return b;
}


static __inline void tm_init_node(tm_node *n, tm_type *t)
{
  tm_list_set_color(n, tm_WHITE);
  
  t->n[tm_TOTAL] ++;
  t->n[tm_WHITE] ++;
  
  tm_list_init(&n->list);
  
  tm.n[tm_TOTAL] ++;
  tm.n[tm_WHITE] ++;
  
  tm_node_set_color(n, t, tm_WHITE);
  
  tm_msg("Init: new node %p type %p\n", (void*) n, (void*) t);
}

static __inline void tm_init_some_nodes(tm_type *t, long left)
{
  size_t count = 0, bytes = 0;
  tm_block *b;
  tm_node *n = 0;
  char *pe, *end;

  /* First block alloc? */
  do {
    if ( ! t->ab ) {
      t->ab = tm_alloc_new_type_block(t);
    }
    b = t->ab;
    
    end = (char*) b + b->size;
    while ( (pe = b->alloc + (tm_node_HDR_SIZE + t->size)) <= end ) {
      n = (tm_node *) b->alloc;
      b->alloc = pe;
      
      tm_init_node(n, t);
      count ++;
      bytes += t->size;
      if ( -- left <= 0 ) {
	goto done;
      }
    }
    
    /* Force new block allocation next time around. */
    t->ab = 0;
    
  done:
  } while ( ! count );

  if ( count ) tm_msg("init: %lu nodes, %lu bytes, %lu free\n", count, bytes, t->n[tm_WHITE]);
  tm_assert_test(t->n[tm_WHITE]);
}

static __inline tm_type *tm_make_new_type(size_t size)
{
  tm_type *t;

  /* Allocate new type desc. */

  /* Take from free list */
  if ( tm.type_free ) {
    t = tm.type_free;
    tm.type_free = t->hash_next;
    t->hash_next = 0;
  } else {
    tm_assert(tm.type_free);
    t = 0;
  }

  /* Initialize type. */
  t->size = size;
  t->n[tm_B] = 0;
  tm_list_init(&t->blocks);
  
  /* Add to global types list. */
  tm_list_insert(&tm.types, t);
  
  /* Init node counts. */
  memset(t->n, 0, sizeof(t->n));
  
  /* Init node lists. */
  {
    int j;
    
    for ( j = 0; j < sizeof(t->l)/sizeof(t->l[0]); j ++ ) {
      tm_list_init(&t->l[j]);
    }
  }

  /*
  ** To avoid triggering a collection,
  ** allocate some nodes now so the free list is not empty.
  */
  tm_init_some_nodes(t, tm_block_SIZE / (tm_node_HDR_SIZE + t->size));

  return t;
}

/****************************************************************************/

static __inline tm_type *tm_block_to_type(tm_block *b)
{
  return b->type;
}

static __inline tm_block *tm_ptr_to_block(char *p)
{
  tm_block *b;

  /*
  ** A pointer directly at the end of block should be considered
  ** a pointer into the block.
  */
  if ( (((unsigned long) p) & (tm_block_SIZE - 1)) == 0 ) {
    tm_msg("Ptr: %p at block boundary\n", (void*) p);
    p --;
  }

  b = (void*) (((unsigned long) p) & ~(tm_block_SIZE - 1));

  return b;
}

static __inline tm_node *tm_pure_ptr_to_node(void *_p)
{
  return (tm_node*) (((char*) _p) - tm_node_HDR_SIZE);
}

static __inline void *tm_node_to_ptr(tm_node *n)
{
  return (void*) (n + 1);
}

static __inline tm_node *tm_ptr_to_node(void *p)
{
  tm_block *b;
  tm_type *t;

  /* Avoid out of range pointers. */
  if ( ! (tm_ptr_l <= p && p <= tm_ptr_h) )
    return 0;

  /* Get the block and type. */
  b = tm_ptr_to_block(p);
  t = tm_block_to_type(b);

  /* Avoid untyped blocks. */
  if ( ! t )
    return 0;

  /* Avoid references to block headers. */ 
  if ( (char*) p < (char*) b + tm_block_HDR_SIZE )
    return 0;

  /* Avoid references outside block. */
  if ( (char*) p >= b->alloc )
    return 0;

  /* Normalize p to node head by using type size. */
  {
    unsigned long pp = (unsigned long) p;
    unsigned long node_size = tm_node_HDR_SIZE + t->size;
    tm_node *n;
    
    /*
    ** Translate away block header.
    */
    pp -= (unsigned long) b + tm_block_HDR_SIZE;

    /* If the pointer is directly after a node boundary
    ** assume its a pointer to the node before.
    */
    if ( pp % node_size == 0 ) {
      tm_msg("Ptr: %p at node boundary\n", (void*) p);
      pp --;
    }

    /*
    ** Remove inter-node offset.
    */
    pp -= pp % node_size;

    /*
    ** Translate back to block header.
    */
    pp += (unsigned long) b + tm_block_HDR_SIZE;

    /* Avoid references to node header. */
    if ( (unsigned long) p < pp + tm_node_HDR_SIZE )
      return 0;

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
  return b->type;
}

static __inline tm_type *tm_size_to_type(size_t size)
{
  tm_type **tp, *t;
  int i;
  
  /* Align size to tm_ALLOC_ALIGN. */
  size = (size + (tm_ALLOC_ALIGN - 1)) & ~(tm_ALLOC_ALIGN - 1);

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

  /* We don't support big allocs yet. */
  tm_assert(size <= tm_block_SIZE_MAX);

  /* Compute hash bucket index. */
  i = size / tm_ALLOC_ALIGN;

  /* Modulus into hash bucket index. */
  i %= tm_type_hash_LEN;

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

  if ( ! t ) {
    t = tm_make_new_type(size);

    /* Add to type hash */
    t->hash_next = tm.type_hash[i];
    tm.type_hash[i] = t;

    tm_msg("type: new type %p for size %lu\n", (void*) t, (unsigned long) size);
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
  tm_node *node;
  size_t n[tm__LAST];
  size_t tn[tm__LAST];

  memset(n, 0, sizeof(n));

  /* Validate types. */
  tm_list_LOOP(&tm.types, t);
  {
    /* Validate type totals. */
    memset(tn, 0, sizeof(n));

    for ( j = 0; j < tm__LAST; j ++ ) {
      /* Validate lists. */
      tm_list_LOOP(&t->l[j], node);
      {
	tm_assert(tm_node_color(node) == j);
	tn[j] ++;
      }
      tm_list_LOOP_END;

      tm_assert(t->n[j] == tn[j]);
      tn[tm_TOTAL] += tn[j];
      n[j] += tn[j];
    }
    tn[tm_TOTAL] /= 2;

    tm_assert(t->n[tm_TOTAL] == tn[tm_TOTAL]);
  }
  tm_list_LOOP_END;

  n[tm_TOTAL] /= 2;

  /* Validate global color lists. */
  tm_assert(n[tm_WHITE] == tm.n[tm_WHITE]);
  tm_assert(n[tm_ECRU] == tm.n[tm_ECRU]);
  tm_assert(n[tm_GREY] == tm.n[tm_GREY]);
  tm_assert(n[tm_BLACK] == tm.n[tm_BLACK]);
  tm_assert(n[tm_TOTAL] == tm.n[tm_TOTAL]);
}

static void tm_print_utilization(const char *name, tm_type *t, size_t *n)
{
  int j;

  tm_msg(name);

  switch ( tm.phase ) {
  case tm_ROOTS:
    n[tm_M] = 0;
    n[tm_D] = n[tm_ECRU] + n[tm_GREY] + n[tm_BLACK];
    break;

  case tm_ALLOC:
    n[tm_M] = 0;
    n[tm_D] = n[tm_ECRU] + n[tm_GREY] + n[tm_BLACK];
    break;

  case tm_SCAN:
    n[tm_M] = 0;
    n[tm_D] = n[tm_ECRU] + n[tm_GREY] + n[tm_BLACK];
    break;

  case tm_SWEEP:
    n[tm_M] = 0;
    n[tm_D] = n[tm_GREY] + n[tm_BLACK];
    break;

  case tm_UNMARK:
    n[tm_M] = 0;
    n[tm_D] = n[tm_ECRU] + n[tm_GREY] + n[tm_BLACK];
    break;

  default:
    tm_abort();
  }

  /* Definitaly used. */
  n[tm_PD] = n[tm_TOTAL] ? n[tm_D] * 100 / n[tm_TOTAL] : 0;

  /* Maybe used. */
  n[tm_PM] = n[tm_TOTAL] ? n[tm_M] * 100 / n[tm_TOTAL] : 0;

  if ( t ) {
    size_t blocks_size = t->n[tm_B] * tm_block_SIZE;
    size_t nodes_size = n[tm_D] * t->size;

    n[tm_W] = blocks_size - nodes_size;
    n[tm_PW] = blocks_size ? n[tm_W] * 100 / blocks_size : 0;
  }

  for ( j = 0; j < tm__LAST2; j ++ ) {
    tm_msg1("%c%-4lu ", tm_color_name[j][0], (unsigned long) n[j]);
  }

  tm_msg1("\n");
}

void tm_print_stats()
{
  tm_type *t;

  _tm_msg_enable_table['N'] ++;

  tm_msg("N: types: %s { blocks %lu[%lu]: \n",
	 tm_phase_name[tm.phase],
	 tm.n[tm_B],
	 tm.n[tm_B] * tm_block_SIZE
	 );

  tm_print_utilization("N: ", 0, tm.n);

  tm_list_LOOP(&tm.types, t);
  {
    tm_msg("N: %p S%-6lu\n", (void*) t, (unsigned long) t->size, (unsigned) t->n[tm_B]);
    tm_print_utilization("N: ", t, t->n);
  }
  tm_list_LOOP_END;
  tm_msg("N: }\n");

  _tm_msg_enable_table['N'] --;

  //tm_validate_lists();
}

void tm_print_block_stats()
{
  tm_type *t;

  tm_block *b;

  _tm_msg_enable_table['N'] ++;

  tm_msg("N: blocks: { \n");

  tm_list_LOOP(&tm.types, t);
  {
    tm_msg("N: type %p: ", t);
    
    tm_list_LOOP(&t->blocks, b);
    {
      tm_msg("N: %p size %lu\n", (void*) b, (unsigned long) b->size, (void*) b->type);
    }
    tm_list_LOOP_END;
  }
  tm_list_LOOP_END;

  tm_msg("N: }\n");

  _tm_msg_enable_table['N'] --;
}



/***************************************************************************/
/* Marking */

static int tm_mark_node(tm_node *n)
{
  switch ( tm_node_color(n) ) {
  case tm_WHITE:
    /* Spurious pointer? */
    tm_abort();
    break;

  case tm_ECRU:
    /* The node has not been scanned. */
    /* Schedule it for scanning. */
    tm_node_set_color(n, tm_node_to_type(n), tm_GREY);
    tm_msg("mark: %p\n", n);
    return 1;
    break;
    
  case tm_GREY:
    /* The node has been sceduled for scanning. */
    break;
    
  case tm_BLACK:
    /* The node has already been marked and scanned. */
    break;

  default:
    tm_abort();
    break;
  }

  return 0;
}

static tm_node * tm_mark_possible_ptr(void *p)
{
  tm_node *n = tm_ptr_to_node(p);
  if ( n && tm_mark_node(n) )
    return n;
    
  return 0;
}


/***************************************************************************/
/* root scan */

static void tm_scan_range(const void *b, const void *e);

static void tm_scan_root_id(int i)
{
  tm_msg("root: %s [%p, %p]\n", tm.roots[i].name, tm.roots[i].l, tm.roots[i].h);
  tm_scan_range(tm.roots[i].l, tm.roots[i].h);
}


static void tm_scan_registers()
{
  tm_scan_root_id(0);
}

void tm_set_stack_ptr(void *stackvar)
{
  tm.roots[1].l = (char*) stackvar - 64;
}

static void tm_scan_stacks()
{
  tm_scan_root_id(1);
}

static void tm_scan_all_roots()
{
  int i;

  tm_msg("root: %lu marked, %lu scanned {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  for ( i = 0; tm.roots[i].name; i ++ ) {
    tm_scan_root_id(i);
  }
  tm.root_mutations = 0;
  tm_msg("root: %lu marked, %lu scanned }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
}


static int tm_scan_some_roots()
{
  int result = 1;
  long left = tm_scan_some_roots_size;

  tm_msg("root: %lu marked, %lu scanned {\n", tm.n[tm_GREY], tm.n[tm_BLACK]);
  tm_msg("root: %s [%p, %p]\n", tm.roots[tm.rooti].name, tm.roots[tm.rooti].l, tm.roots[tm.rooti].h);

  do {
    /* Try scanning some roots. */
    while ( (void*) (tm.rp + sizeof(void*)) >= tm.roots[tm.rooti].h ) {
      tm.rooti ++;
      if ( ! tm.roots[tm.rooti].name ) {
	tm.rooti = 2;
	tm.rp = tm.roots[tm.rooti].l;
	tm_msg("root: done\n");
	tm.root_mutations = 0;
	result = 0;
	goto done;
      }

      tm_msg("root: %s [%p, %p]\n", tm.roots[tm.rooti].name, tm.roots[tm.rooti].l, tm.roots[tm.rooti].h);
      
      tm.rp = tm.roots[tm.rooti].l;
    }

    tm_mark_possible_ptr(* (void**) tm.rp);

    tm.rp += tm_PTR_ALIGN;

    left -= tm_PTR_ALIGN;
  } while ( left > 0 );

 done:
  tm_msg("root: %lu marked, %lu scanned }\n", tm.n[tm_GREY], tm.n[tm_BLACK]);

  return result; /* We're not done. */
}

/***************************************************************************/
/* Scannning */

static void tm_scan_range(const void *b, const void *e)
{
  const char *p;

  for ( p = b; ((char*) p + sizeof(void*)) <= (char*) e; p += tm_PTR_ALIGN ) {
    tm_node *n;

    if ( (n = tm_mark_possible_ptr(* (void**) p)) ) {
      tm_msg("Scan: found ref to %p @ %p\n", n, p);
    }
  }
}

static __inline void tm_scan_node(tm_node *n, tm_type *t)
{
  tm_assert_test(tm_node_to_type(n) == t);
  tm_assert_test(tm_node_color(n) == tm_GREY);

  /* Move to marked list. */
  tm_node_set_color(n, t, tm_BLACK);

  /* Scan all possible pointers. */
  {
    const char *ptr = tm_node_to_ptr(n);
    tm_scan_range(ptr, ptr + t->size);
  }
}


static size_t tm_scan_some_nodes(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_GREY] ) {
    tm_node_LOOP(tm_GREY);
    {
      tm_scan_node(n, t);
      count ++;
      bytes += t->size;
      if ( (left -= t->size) <= 0 ) {
	tm_node_LOOP_BREAK();
      }
    }
    tm_node_LOOP_END(tm_GREY);
  }

  if ( count ) tm_msg("Scan: %lu nodes, %lu bytes, %lu left\n", count, bytes, tm.n[tm_GREY]);
  return tm.n[tm_GREY];
}

static void tm_scan_all_nodes()
{
  while ( tm.n[tm_GREY] ) {
    tm_node_LOOP_INIT(tm_GREY);
    tm_scan_some_nodes(tm.n[tm_TOTAL]);
  }
}


/***************************************************************************/
/* Sweeping */

int tm_sweep_is_error = 0;

static int tm_check_sweep_error()
{
  tm_type *t;
  tm_node *n;

  if ( tm_sweep_is_error ) {
    if ( tm.n[tm_ECRU] ) {
      tm_msg("Fatal: %lu dead nodes: there should be no sweeping.\n", tm.n[tm_ECRU]);
      tm_validate_lists();

      tm_list_LOOP(&tm.types, t);
      {
	tm_list_LOOP(&t->l[tm_ECRU], n);
	{
	  tm_assert_test(tm_node_color(n) == tm_ECRU);
	  tm_msg("Fatal: node %p color %s size %lu should not be sweeped!\n", 
		 (void*) n, 
		 tm_color_name[tm_node_color(n)], 
		 (unsigned long) t->size);
	  {
	    void ** vpa = tm_node_to_ptr(n);
	    tm_msg("Fatal: cons (%d, %p)\n", (int) vpa[0], vpa[1]);
	  }
	}
	tm_list_LOOP_END;
      }
      tm_list_LOOP_END;
      
      tm_print_stats();

      tm_scan_all_roots();

      if ( tm.n[tm_ECRU] ) {
	tm_msg("Fatal: after root scan: still missing references.\n");
      } else {
	tm_msg("Fatal: after root scan: missing references found!\n");
      }

      tm_list_LOOP(&tm.types, t);
      {
	tm_list_LOOP(&t->l[tm_ECRU], n);
	{
	  tm_node_set_color(n, t, tm_BLACK);
	}
	tm_list_LOOP_END;
      }
      tm_list_LOOP_END;

      tm_init_phase(tm_ALLOC);
      tm_assert_test(tm.n[tm_ECRU] == 0);

      return 1;
    }
  }
  return 0;
}

/*********************************************************************/

static __inline void tm_sweep_node(tm_node *n, tm_type *t)
{
  tm_assert_test(tm_node_color(n) == tm_ECRU);
  tm_node_set_color(n, t, tm_WHITE);
  memset(tm_node_to_ptr(n), 0, t->size);
}

static long tm_sweep_some_nodes(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_ECRU] ) {
    if ( tm_check_sweep_error() )
      return 0;
  
    tm_node_LOOP(tm_ECRU);
    {
      tm_sweep_node(n, t);
      bytes += t->size;
      count ++;
      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK();
      }
    }
    tm_node_LOOP_END(tm_ECRU);
  }

  if ( count ) tm_msg("sweep: %lu nodes, %lu bytes, %lu left\n", count, bytes, tm.n[tm_ECRU]);
  return tm.n[tm_ECRU];
}

static size_t tm_sweep_some_nodes_for_type(tm_type *t)
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
      tm_sweep_node(n, t);
      bytes += t->size;
      count ++;
      if ( -- left <= 0 ) {
	break;
      }
    }

  }

  if ( count ) tm_msg("sweep: type %p: %lu nodes, %lu bytes, %lu left\n", t, count, bytes, tm.n[tm_ECRU]);
  return tm.n[tm_ECRU];
}

static void tm_sweep_all_nodes()
{
  while ( tm.n[tm_ECRU] ) {
    tm_node_LOOP_INIT(tm_ECRU);
    tm_sweep_some_nodes(tm.n[tm_TOTAL]);
  }
}

static void tm_sweep_some_blocks()
{
  /* Try one block at a time. */

  if ( ! tm.tb || tm.tb == &tm.types ) {
    tm.tb = tm_list_next(&tm.types);
    tm.bb = tm_list_next(&tm.tb->blocks);
  } else if ( tm.bb == &tm.tb->blocks ) {
    tm.tb = tm_list_next(tm.tb);
    tm.bb = tm_list_next(&tm.tb->blocks);
  }
  
  {
    tm_block *b = tm.bb;
    tm_type *t = tm.bb->type;
    char *p;

    p = b;
    p += tm_block_HDR_SIZE;

    /* Scan the block's nodes. */
    while ( p < b->alloc ) {
      /* Something is being used. */
      if ( tm_node_color((tm_node*) p) != tm_WHITE ) {
	return;
      }
      p += tm_node_HDR_SIZE + t->size;
    }

    /* Must be totally unused. */
    p = b;
    p += tm_block_HDR_SIZE;
    while ( p < b->alloc ) {
      tm_list_remove(p);
      p += tm_node_HDR_SIZE + t->size;
    }

    tm.bb = tm_list_next(tm.bb);
    tm_free_block(b);
  }

}

/***************************************************************************/
/* Unmarking */

static size_t tm_unmark_some_nodes(long left)
{
  size_t count = 0, bytes = 0;

  if ( tm.n[tm_BLACK] ) {
    tm_node_LOOP(tm_BLACK);
    {
      tm_assert_test(tm_node_color(n) == tm_BLACK);
      tm_node_set_color(n, t, tm_ECRU);
      bytes += t->size;
      count ++;
      if ( -- left <= 0 ) {
	tm_node_LOOP_BREAK();
      }
    }
    tm_node_LOOP_END(tm_BLACK);
  }

  if ( count ) tm_msg("unmark: %lu nodes, %lu bytes, %lu left\n", count, bytes, tm.n[tm_BLACK]);
  return tm.n[tm_BLACK];
}

static void tm_unmark_all_nodes()
{
  while ( tm.n[tm_BLACK] ) {
    tm_node_LOOP_INIT(tm_BLACK);
    tm_unmark_some_nodes(tm.n[tm_TOTAL]);
  }
}

/***************************************************************************/
/* full gc */

void tm_gc_full_inner()
{
  int try = 0;

  tm_msg("gc: {\n");

  tm_sweep_is_error = 0;

  while ( try ++ < 2 ) {
    /* Scan all roots */
    tm_init_phase(tm_ROOTS);
    tm_scan_all_roots();
    
    /* Scan all marked nodes */
    tm_init_phase(tm_SCAN);
    tm_scan_all_nodes();
    tm_assert_test(tm.n[tm_GREY] == 0);
    
    /* Sweep the nodes. */
    tm_init_phase(tm_SWEEP);
    tm_sweep_all_nodes();
    tm_assert_test(tm.n[tm_ECRU] == 0);
    
    /* Unmark the nodes. */
    tm_init_phase(tm_UNMARK);
    tm_unmark_all_nodes();
    tm_assert_test(tm.n[tm_BLACK] == 0);
    
    tm_init_phase(tm_ALLOC);
  }

  tm_msg("gc: }\n");
}

/***************************************************************************/
/* write barrier */

/*
** tm_root_write() should be called after modifing a root ptr.
*/
void tm_mark(void *ptr)
{
  tm_mark_possible_ptr(ptr);
}


void __tm_root_write_ignore(void **ptrp)
{
  /* DO NOTHING */
}
void __tm_root_write_root(void **ptrp)
{
  tm_mark(*ptrp);
  tm_msg("write barrier (root): %p\n", ptr);
}


/*
** tm_write_barrier(ptr) must be called before any ptrs
** within node ptr are mutated.
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
    ** The node has already been scanned.
    ** The mutator may introduce new pointers.
    ** Force it to be rescanned.
    */
    tm_node_set_color(n, tm_node_to_type(n), tm_GREY);
    tm_msg("write barrier: %p\n", n);
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
  tm_assert_test(tm.phase == tm_ROOTS);
  /* We are currently scanning roots.
  ** Don't know if this root has been scanned yet or not.
  */

  /*
  ** If the ptr is a reference to a stack allocated object.
  ** Do nothing, because the stack will be scanned before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h )
    return;

  /*
  ** If the ptr is a reference to the heap. 
  ** Do nothing, because the stack will be scanned before
  ** the sweep phase.
  */
  if ( tm_ptr_l <= ptr && ptr <= tm_ptr_h )
    return;

  /*
  ** Must be a pointer to statically allocated (root) object.
  ** Mark the root as being.
  */
  tm.root_mutations ++;
  tm_msg("write barrier (root): %p\n", ptr);

}
static void __tm_write_barrier_scan(void *ptr)
{
  tm_node *n;

  tm_assert_test(tm.phase == tm_SCAN);

  /*
  ** If the ptr is a reference to a stack allocated object.
  ** Do nothing, because the stack will be scanned before
  ** the sweep phase.
  */
  if ( (void*) &ptr <= ptr && ptr <= tm.roots[1].h )
    return;

  /*
  ** If the ptr is a reference to a node,
  ** Schedule it for rescanning if it has already been scanned.
  **
  ** If its not a reference to a node.
  ** It must be a reference to a statically allocated object.
  ** This means the global root set has (probably) been mutated.
  ** We must flag the roots for rescanning before the sweep phase.
  **
  */
  if ( (n = tm_ptr_to_node(ptr)) ) {
    tm_write_barrier_node(n);
  } else {
    tm.root_mutations ++;
    tm_msg("write barrier (root): %p\n", ptr);
  }
}

static void __tm_write_barrier_pure_ignore(void *ptr)
{
  /* DO NOTHING */
}

static void __tm_write_barrier_pure_scan(void *ptr)
{
  tm_assert_test(tm.phase == tm_SCAN);
  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}

/***************************************************************************/
/* allocation */

int tm_alloc_pass_max = 0;

static __inline void *tm_alloc_free_node(tm_type *t)
{
  tm_node *n;

  if ( (n = tm_list_first(&t->l[tm_WHITE])) ) {
    char *ptr;

    tm_assert_test(tm_node_color(n) == tm_WHITE);

    ptr = tm_node_to_ptr(n);

    memset(ptr, 0, t->size);
    
    if ( tm_ptr_l > (void*) ptr ) {
      tm_ptr_l = ptr;
    }
    if ( tm_ptr_h < (void*) (ptr + t->size) ) {
      tm_ptr_h = ptr + t->size;
    }
    
    /* Put it on the appropriate allocated list. */
    tm_node_set_color(n, t, tm_phase_alloc[tm.phase]);

    return ptr;
  } else {
    return 0;
  }
}

void tm_free(void *ptr)
{
  tm_type *t;
  tm_node *n;

  n = tm_pure_ptr_to_node(ptr);
  t = tm_node_to_type(n);

  tm_assert(tm_node_color(n) != tm_WHITE);
  tm_node_set_color(n, t, tm_WHITE);
}

void *tm_alloc_inner(size_t size)
{
  tm_type *t;
  void *ptr;

  tm_alloc_id ++;
  tm_alloc_pass = 0;
  memset(tm.alloc_n, 0, sizeof(tm.alloc_n));

  t = tm_size_to_type(size);

  tm.next_phase = -1;
  tm_alloc_pass ++;

  switch ( tm.phase ) {
  case tm_ALLOC:
    /* Keep allocating until we're out of free nodes. */
    if ( ! t->n[tm_WHITE] ) {
      tm.next_phase = tm_ROOTS;
    }
    break;
    
  case tm_ROOTS:
    /* Scan some roots for pointers. */
    if ( ! tm_scan_some_roots() ) {
      tm.next_phase = tm_SCAN;
    }
    break;
    
  case tm_SCAN:
    /* Scan a little bit. */
    if ( ! tm_scan_some_nodes(tm_scan_some_nodes_size) ) {

      /* If the root was mutated during scan, */
      if ( tm.root_mutations ) {
	/* Scan all roots. */
	tm_scan_all_roots();
      } else {
	/* Scan registers. */
	tm_scan_registers();
      
	/* Scan the stacks. */
	tm_scan_stacks();
      }

      /* After scanning the registers and stack, no additional nodes were marked. */
      if ( ! tm_scan_some_nodes(tm_scan_some_nodes_size) ) {
	tm.next_phase = tm_SWEEP;
      }
    }
    break;
    
  case tm_SWEEP:
    /* Try to immediately sweep nodes for the type we want first. */
    tm_sweep_some_nodes_for_type(t);
    if ( ! tm_sweep_some_nodes(tm_sweep_some_nodes_size) ) {
      tm.next_phase = tm_UNMARK;
    }

    /* Attempt to scan a block. */
    tm_sweep_some_blocks();
    break;
    
  case tm_UNMARK:
    if ( ! tm_unmark_some_nodes(tm_unmark_some_nodes_size) ) {
      tm.next_phase = tm_ALLOC;
    }
    break;
  }
  
  /* Take one from the free list. */
  if ( ! (ptr = tm_alloc_free_node(t)) ) {

#if 0 
    /*
    ** If there are still no possibly free nodes,
    ** then
    **    allocate a new page for the type.
    */
    switch ( tm.phase ) {
    case tm_SWEEP:
      if ( t->n[tm_ECRU] ) {
	tm_abort();
	break;
      }
      /* FALL THROUGH */
      
    case tm_UNMARK:
    case tm_ALLOC:
    case tm_ROOTS:
    case tm_SCAN:
      tm_init_some_nodes(t, tm_alloc_some_nodes_size);
      break;

#if 0      
    case tm_SCAN:
      /* There's probably no garbage. */
      if ( tm_alloc_pass > 100 || t->n[tm_ECRU] < t->n[tm_TOTAL] / 2) {
	tm_init_some_nodes(t, tm_alloc_some_nodes_size);
      } else {
	goto try_again;
      }
      break;
#endif

    default:
      tm_fatal();
      break;
    }
#else
    tm_init_some_nodes(t, tm_alloc_some_nodes_size);
#endif
      
    ptr = tm_alloc_free_node(t);
    tm_assert_test(ptr);
  }
  
  if ( tm.next_phase >= 0 )
    tm_init_phase(tm.next_phase);

  tm_msg("alloc: %lu: %p\n", (unsigned long) size, (void*) ptr);

  // tm_validate_lists();

  return (void*) ptr;
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

/* Avoid garbage on the stack. */
void _tm_clear_some_stack_words()
{
  int zero[16];
  memset(zero, 0, sizeof(zero));
}

/***************************************************************************/


