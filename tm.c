/* $Id: tm.c,v 1.1 1999-06-09 07:00:50 stephensk Exp $ */
#include "tm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <setjmp.h>
#include <string.h>
#include <sys/times.h>

#include "list.h"

/****************************************************************************/

typedef enum tm_color {
  tm_color_WHITE,    /* free, in free list. */
  tm_color_ECRU,     /* allocated, not marked. */
  tm_color_GREY,     /* marked, not scanned. */
  tm_color_BLACK,    /* marked and scanned. */
  tm_color_VIOLET,     /* to be unmarked. */
  tm_color_YELLOW,    /* to be freed. */
  tm_color_TOTAL,
  tm_color__LAST
} tm_color;

static const char *tm_color_name[] = {
  "WHITE",
  "ECRU",
  "GREY",
  "BLACK",
  "VIOLET",
  "YELLOW",
  "TOTAL",
  "_LAST",
  0
};

typedef struct tm_node {
  tm_list _list;        /* The current list for the node. */
  unsigned color : 3;   /* The current color for the node. */
  double data[1];
} tm_node;

typedef struct tm_block {
  tm_list _list;        /* Type's lock list */
  unsigned color : 3;   /* The current color for the node. */
  size_t size;          /* Blocks actual size (including this hdr), multiple of tm_block_SIZE. */
  struct tm_type *type; /* The type the block is assigned to. */
  char *alloc;          /* The allocation pointer for this block. */
} tm_block;

typedef struct tm_type {
  tm_list _list;       /* Type hash bucket list. */
  size_t size;         /* Size of elements. */
  tm_list blocks;       /* List of blocks allocated for this type. */
  unsigned int nblocks;  /* Total blocks dedicated to this type. */
  unsigned long n[tm_color__LAST]; /* Total elements. */
  tm_list free_list;   /* List of free elements. */
} tm_type;

enum {
  tm_ALLOC_ALIGN = 8,

  tm_PTR_ALIGN = 1, /* __alignof(void*), */

  tm_node_HDR_SIZE = sizeof(tm_node) - sizeof(double),
  tm_block_HDR_SIZE = sizeof(tm_block),

  tm_block_SIZE = 8 * 1024,

  tm_block_SIZE_MAX = tm_block_SIZE - tm_block_HDR_SIZE,

  tm_PTR_RANGE = 512 * 1024 * 1024,
  tm_block_N_MAX = tm_PTR_RANGE / tm_block_SIZE,
};

/*
                                            
[ free list (white) ]    <------------------ [ marked list (black) ]
   |                                           ^
   |                                           |
   | allocation                                | sweeping
   V                                           |
[ allocated list (grey) ] -----------------> [ scanned list (ecru) ]
                            scanning
*/

/****************************************************************************/
/* Internal data. */

typedef struct tm_data {
  /* Valid pointer range. */
  void *ptr_range[2];

  /* Block type. */
  void *block_base;
  unsigned long block_bitmap[tm_block_N_MAX / (sizeof(unsigned long)*8)];

  /* type hash table. */
  tm_type type_reserve[32], *type_free;
#ifndef tm_type_hash_LEN
#define tm_type_hash_LEN 101
#endif
  tm_list type_hash[tm_type_hash_LEN];

  /* Global node lists and counts. */
  tm_list color_list[tm_color__LAST];
  unsigned long color_n[tm_color__LAST];

  /* Current tm_alloc() list change stats. */
  unsigned long alloc_n[tm_color__LAST];
  
  /* Current tm_alloc() timing. */
  struct tms alloc_time;

  /* Roots */
  struct {
    const char *name;
    const void *l, *h;
  } roots[8];
  int nroots;

  /* Register roots. */
  jmp_buf jb;

} tm_data;

static struct tm_data tm_;

#define tm_ptr_l tm_.ptr_range[0]
#define tm_ptr_h tm_.ptr_range[1]
#define tm_block_list (&tm_.block_list)
#define tm_block_free_list (&tm_.block_free_list)
#define tm_type_reserve tm_.type_reserve
#define tm_type_free tm_.type_free
#define tm_type_hash tm_.type_hash
#define tm_roots tm_.roots
#define tm_nroots tm_.nroots
#define tm_jb tm_.jb
#define tm_color_list tm_.color_list
#define tm_color_n tm_.color_n


/****************************************************************************/
/* Debugging. */

static unsigned long tm_alloc_id = 0;
static unsigned long tm_alloc_pass = 0;

FILE *tm_msg_file = 0;

static char _tm_msg_ignore_table[256];

static const char *_tm_msg_ignore = "arSscumTRtbg";

static int _tm_msg_ignored = 0;

void tm_msg(const char *format, ...)
{
  va_list vap;

  if ( (_tm_msg_ignored = _tm_msg_ignore_table[(unsigned char) format[0]]) )
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
/* Init. */

extern int _data_start__, _data_end__, _bss_start__, _bss_end__;

static void _tm_add_root(const char *name, const void *l, const void *h)
{
  if ( l >= h )
    return;

  tm_roots[tm_nroots].name = name;
  tm_roots[tm_nroots].l = l;
  tm_roots[tm_nroots].h = h;
  tm_msg("Root: added %s %d [%p, %p]\n", tm_roots[tm_nroots].name, tm_nroots, tm_roots[tm_nroots].l, tm_roots[tm_nroots].h);
  tm_nroots ++;
}

static void tm_add_root(const char *name, const void *_l, const void *_h)
{
  const void *l = _l, *h = _h;
  const void *tm_l, *tm_h;

  if ( l >= h )
    return;

  tm_l = &tm_;
  tm_h = (&tm_) + 1;

  if ( l <= tm_l && tm_l < h ) {
    _tm_add_root(name, l, tm_l);
    l = tm_h;
  }
  if ( l <= tm_l && tm_l < h ) {
    _tm_add_root(name, tm_h, h);
    l = h;
  }

  if ( l >= h )
    return;

  _tm_add_root(name, l, h);
}

void tm_init(int *argcp, char ***argvp, char ***envpp)
{
  int i;

  /* Initialize msg ignore table. */
  {
    const unsigned char *r;

    for ( r = _tm_msg_ignore; *r; r ++ ) {
      _tm_msg_ignore_table[*r] = 1;
    }
  }

  /* Initialize possible pointer range. */
  tm_ptr_l = (void*) ~0UL;
  tm_ptr_h = 0;

  /* Initialize roots. */
  _tm_add_root("registers", tm_jb, (&tm_jb) + 1);
  _tm_add_root("stack", &i, envpp);
  tm_add_root("initialized data", &_data_start__, &_data_end__);
  tm_add_root("uninitialized data", &_bss_start__, &_bss_end__);


  /* Initialize global lists. */
  for ( i = 0; i < sizeof(tm_color_list)/sizeof(tm_color_list[0]); i ++ ) {
    tm_list_init(&tm_color_list[i]);
  }

  /* Initialize types. */
  {
    int i;
    tm_type *t;
    
    /* Intialize type desc free list. */
    for ( t = tm_type_reserve; t < tm_type_reserve + sizeof(tm_type_reserve)/sizeof(tm_type_reserve[0]); t ++ ) {
      t->_list.next = (void*) tm_type_free;
      tm_type_free = t;
    }
    
    /* Initialize type desc hash table. */
    for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
      tm_list_init(&tm_type_hash[i]);
    }
  }
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
  assert((offset & (tm_block_SIZE - 1)) == 0);
  
  /* Initialize and add to global block list. */
  b->size = size;
  b->color = tm_color_WHITE;
  b->type = 0;
  b->alloc = (char*) b + tm_block_HDR_SIZE;
  tm_list_init(&b->_list);

  if ( ! tm_.block_base ) {
    tm_.block_base = b;
  }

  return b;
}

static __inline void tm_node_set_color(tm_node *n, tm_type *t, tm_color c);

static tm_block * tm_alloc_new_type_block(tm_type *type)
{
  tm_block *b;
  
  /* Allocate a new block */
  b = tm_alloc_block(tm_block_SIZE);

  /* Mark it as in use. */
  b->color = tm_color_ECRU;

  /* Add to type's block list. */
  b->type = type;
  type->nblocks ++;
  tm_list_insert(&type->blocks, b);

  tm_msg("block: %p allocated for type %p\n", (void*) b, (void*) type);

  return b;
}


static void tm_init_node(tm_type *type, tm_node *n)
{
  n->color = tm_color_WHITE;
  
  type->n[tm_color_TOTAL] ++;
  type->n[tm_color_WHITE] ++;
  
  tm_list_init(&n->_list);
  
  tm_color_n[tm_color_TOTAL] ++;
  tm_color_n[tm_color_WHITE] ++;
  
  tm_node_set_color(n, type, tm_color_WHITE);
  
  tm_msg("alloc: new node %p\n", (void*) n);
}

static void tm_alloc_some_nodes(tm_type *type)
{
  tm_block *b;
  char *p, *pe, *end;

  /* First block alloc? */
  b = tm_alloc_new_type_block(type);
  
  end = (char*) b + b->size;
  for ( p = b->alloc; pe = p + tm_node_HDR_SIZE + type->size, pe <= end; p = pe ) {
    tm_init_node(type, (tm_node *) p);
    b->alloc = pe;
  }
}

/****************************************************************************/

static __inline tm_type *tm_block_to_type(tm_block *b)
{
  return b->type;
}

static __inline tm_block *tm_ptr_to_block(void *p)
{
  tm_block *b = (void*) (((unsigned long) p) & ~(tm_block_SIZE - 1));
  return b;
}

static __inline tm_node *tm_pure_ptr_to_node(void *_p)
{
  return (tm_node*) (((char*) _p) - tm_node_HDR_SIZE);
}

static __inline void *tm_node_to_ptr(tm_node *n)
{
  return (void*) (n->data);
}

static __inline tm_node *tm_ptr_to_node(void *p)
{
  tm_block *b;
  tm_type *t;

  /* Avoid out of range pointers. */
  if ( ! (tm_ptr_l <= p && p < tm_ptr_h) )
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

    pp -= (unsigned long) b + tm_block_HDR_SIZE;
    pp /= tm_node_HDR_SIZE + t->size;
    pp *= tm_node_HDR_SIZE + t->size;
    pp += (unsigned long) b + tm_block_HDR_SIZE;

    /* Avoid references to node header. */
    if ( (unsigned long) p < pp + tm_node_HDR_SIZE )
      return 0;

    /* Must be okay! */
    return (tm_node*) pp;
  }
}

static __inline tm_type *tm_node_to_type(tm_node *n)
{
  tm_block *b = tm_ptr_to_block(n);
  return b->type;
}

static __inline tm_type *tm_size_to_type(size_t size)
{
  tm_type *t;
  int i;
  
  /* Align size fo tm_ALLOC_ALIGN. */
  size = (size + (tm_ALLOC_ALIGN - 1)) & ~(tm_ALLOC_ALIGN - 1);

  assert(size <= tm_block_SIZE_MAX);

  /* Compute hash bucket index. */
  i = size / tm_ALLOC_ALIGN;
  
  if ( ! (t = tm_list_first(&tm_type_hash[i])) ) {
    /* Allocate new type desc. */
    assert(tm_type_free);
    t = tm_type_free;
    tm_type_free = (void*) t->_list.next;
    
    /* Initialize type. */
    t->size = size;
    t->nblocks = 0;
    tm_list_init(&t->blocks);
    memset(t->n, 0, sizeof(t->n));
    tm_list_init(&t->free_list);
    
    /* Add to type hash */
    tm_list_insert(&tm_type_hash[i], t);

    tm_msg("type: new type %p for size %lu\n", (void*) t, (unsigned long) size);
  }
  
  assert(t->size == size);
  
  return t;
}


/***************************************************************************/
/* Color mgmt. */

static __inline tm_type *tm_node_to_type(tm_node *n);

#define tm_node_color(n) ((tm_color) ((n)->color))

static __inline void _tm_node_set_color(tm_node *n, tm_type *t, tm_color c)
{
  //  assert(c <= tm_color_BLACK);

  tm_color_n[tm_node_color(n)] --;
  tm_color_n[c] ++;

  // tm_alloc() stats.
  tm_.alloc_n[c] ++;
  tm_.alloc_n[tm_color_TOTAL] ++;

  t->n[c] ++;
  t->n[tm_node_color(n)] --;

  n->color = c;

}

static __inline void tm_node_set_color(tm_node *n, tm_type *t, tm_color c)
{
  if ( ! t ) 
    t = tm_node_to_type(n);

  _tm_node_set_color(n, t, c);

  if ( c == tm_color_WHITE ) {
    memset(tm_node_to_ptr(n), 0, t->size);
    tm_list_remove_and_insert(&t->free_list, n);
  } else {
    tm_list_remove_and_insert(&tm_color_list[c], n);
  }
}

/***************************************************************************/
/* Stats. */

void tm_validate_lists()
{
  int i, j;
  tm_type *t;
  tm_node *n;
  unsigned long c[tm_color__LAST];
  unsigned long count;

  memset(c, 0, sizeof(c));
  count = 0;

  /* Validate types. */
  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm_list_LOOP(&tm_type_hash[i], t);
    {
      /* Validate type totals. */
      count = 0;
      for ( j = 0; j < tm_color__LAST; j ++ ) {
	if ( j != tm_color_TOTAL ) {
	  count += t->n[j];
	}
      }
      assert(t->n[tm_color_TOTAL] == count);

       /* Validate free list. */
      tm_list_LOOP(&t->free_list, n);
      {
	assert(tm_node_color(n) == tm_color_WHITE);
	c[tm_color_WHITE] ++;
      }
      tm_list_LOOP_END;
    }
    tm_list_LOOP_END;
  }

  /* Validate global color lists. */
  for ( j = 0; j < tm_color__LAST; j ++ ) {
    tm_list_LOOP(&tm_color_list[j], n);
    {
      int i = j;

      /* The YELLOW list contains ECRU nodes. */
      if ( j == tm_color_YELLOW )
	i = tm_node_color(n);

      assert(tm_node_color(n) == i);
      c[j] ++;
    }
    tm_list_LOOP_END;
   
    if ( j != tm_color_TOTAL ) {
      c[tm_color_TOTAL] += c[j];
    }
  }

  assert(c[tm_color_ECRU] == tm_color_n[tm_color_ECRU]);
  assert(c[tm_color_GREY] == tm_color_n[tm_color_GREY]);
  assert(c[tm_color_BLACK] == tm_color_n[tm_color_BLACK]);
  assert(c[tm_color_VIOLET] == tm_color_n[tm_color_VIOLET]);
  assert(c[tm_color_YELLOW] == tm_color_n[tm_color_YELLOW]);
  // assert(c[tm_color_TOTAL] == tm_color_n[tm_color_TOTAL]);
}

static void tm_print_utilization(const char *name, const unsigned long *n)
{
  int j;
  unsigned long in_use;

  tm_msg("%s", name);

  for ( j = 0; j < tm_color__LAST; j ++ ) {
    tm_msg1("%c%-4lu ", tm_color_name[j][0], (unsigned long) n[j]);
  }
  in_use = n[tm_color_ECRU] + n[tm_color_GREY] + n[tm_color_BLACK] + n[tm_color_VIOLET];

  tm_msg1("U/T%lu%% \n", 
	  n[tm_color_TOTAL] ? in_use * 100 / n[tm_color_TOTAL] : 0);
}

void tm_print_stats()
{
  int i;
  tm_type *t;

  tm_msg("N: types: { \n");

  tm_print_utilization("N: ", tm_color_n);

  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm_list_LOOP(&tm_type_hash[i], t);
    {
      tm_msg("N: %p s %6lu nb %3u\n", (void*) t, (unsigned long) t->size, (unsigned) t->nblocks);
      tm_print_utilization("N: ", t->n);
    }
    tm_list_LOOP_END;
  }
  tm_msg("N: }\n");

  //tm_validate_lists();
}

void tm_print_block_stats()
{
  int i;
  tm_type *t;

  tm_block *b;

  tm_msg("N: blocks: { \n");

  for ( i = 0; i < tm_type_hash_LEN; i ++ ) {
    tm_list_LOOP(&tm_type_hash[i], t);
    {
      tm_msg("N: type %p: ", t);
      
      tm_list_LOOP(&t->blocks, b);
      {
	tm_msg("N: %p size %lu\n", (void*) b, (unsigned long) b->size, (void*) b->type);
      }
      tm_list_LOOP_END;
    }
    tm_list_LOOP_END;
  }

  tm_msg("N: }\n");
}



/***************************************************************************/
/* Marking */

static int tm_mark_node(tm_node *n)
{
  /* Move it to the scanned list. */
  switch ( tm_node_color(n) ) {
  case tm_color_WHITE:
  case tm_color_YELLOW:
    /* The node is not active. */
    break;
    
  case tm_color_ECRU:
  case tm_color_VIOLET:
    /* The node has not been scanned. */
    /* Schedule it for scanning. */
    tm_node_set_color(n, 0, tm_color_GREY);
    tm_msg("mark: %p\n", n);
    return 1;
    break;
    
  case tm_color_GREY:
    /* The node has been sceduled for scanning. */
    break;
    
  case tm_color_BLACK:
    /* The node has already been marked and scanned. */
    break;

  default:
    abort();
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
/* root marking */

static void tm_scan_range(const void *b, const void *e);

static void tm_set_stack_ptr(void *stackvar)
{
  tm_roots[1].l = stackvar;
}

static void tm_scan_root_id(int i)
{
  tm_msg("root: %s [%p, %p]\n", tm_roots[i].name, tm_roots[i].l, tm_roots[i].h);
  tm_scan_range(tm_roots[i].l, tm_roots[i].h);
}

static void tm_scan_stack()
{
  tm_scan_root_id(1);
}

static void tm_scan_all_roots()
{
  int i;

  tm_msg("root: { \n");
  for ( i = 0; tm_roots[i].name; i ++ ) {
    tm_scan_root_id(i);
  }
  tm_msg("root: %lu marked, %lu scanned }\n", tm_color_n[tm_color_GREY], tm_color_n[tm_color_BLACK]);
}

static void tm_scan_some_roots()
{
  static int i = -1;
  static const char *rp = 0;

  /* Try scanning some roots. */
  while ( rp == 0 || (void*) rp >= tm_roots[i].h ) {
    i ++;
    if ( ! tm_roots[i].name )
      i = 0;
    tm_msg("root: %s [%p, %p]\n", tm_roots[i].name, tm_roots[i].l, tm_roots[i].h);
    rp = tm_roots[i].l;
  }

  tm_mark_possible_ptr(* (void**) rp);

  rp += tm_PTR_ALIGN;
}

/***************************************************************************/
/* Scannning */

#define tm_scanning (! tm_list_empty(&tm_color_list[tm_color_GREY]))
#define tm_marking tm_scanning

static tm_node *tm_mark_possible_ptr(void *p);

static void tm_scan_range(const void *b, const void *e)
{
  const char *p;

  for ( p = b; (void*) p < e; p += tm_PTR_ALIGN ) {
    tm_node *n;

    if ( (n = tm_mark_possible_ptr(* (void**) p)) ) {
      tm_msg("Scan: found ref to %p @ %p\n", n, p);
    }
  }
}

static void tm_scan_node(tm_node *n)
{
  tm_type *t = tm_node_to_type(n);

  /* Move to marked list. */
  tm_node_set_color(n, t, tm_color_BLACK);

  /* Scan all possible pointers. */
  {
    const char *ptr = tm_node_to_ptr(n);
    tm_scan_range(ptr, ptr + t->size);
  }
}


static void tm_scan_some_nodes()
{
  tm_node *n;

  if ( (n = tm_list_first(&tm_color_list[tm_color_GREY])) ) {
    tm_scan_node(n);
    tm_msg("Scan: %p\n", (void*) n);
  }
}

static void tm_scan_all_nodes()
{
  tm_node *n;
  unsigned long count = 0;

  while ( (n = tm_list_first(&tm_color_list[tm_color_GREY])) ) {
    tm_scan_node(n);
    count ++;
  }
  if ( count ) tm_msg("Scan: %lu nodes\n", count);
}

/***************************************************************************/
/* Sweeping */

#define tm_sweeping (! tm_list_empty(&tm_color_list[tm_color_YELLOW]))

#define tm_sweeping_okay 1

static void tm_sweep_all_nodes()
{
  tm_node *n;
  int count = 0;

  if ( ! tm_sweeping_okay ) {
    while ( (n = tm_list_first(&tm_color_list[tm_color_YELLOW])) ) {
      tm_node_set_color(n, 0, tm_color_WHITE);
      count ++;
    }
    if ( count ) tm_msg("sweep: %lu nodes\n", count);
  }
}

static void tm_sweep_some_nodes()
{
  tm_node *n;

  if ( tm_sweeping_okay ) {
    if ( (n = tm_list_first(&tm_color_list[tm_color_YELLOW])) ) {
      tm_node_set_color(n, 0, tm_color_WHITE);
      tm_msg("sweep: %p\n", n);
    }
  }
}

/***************************************************************************/
/* Unmarking */

#define tm_unmarking (! tm_list_empty(&tm_color_list[tm_color_VIOLET]))

#define tm_unmark_okay 1

static void tm_unmark_all_nodes()
{
  tm_node *n;
  int count = 0;

  if ( tm_unmark_okay ) {
    while ( (n = tm_list_first(&tm_color_list[tm_color_VIOLET])) ) {
      tm_node_set_color(n, 0, tm_color_ECRU);
      tm_msg("unmark: %p\n", n);
      count ++;
    }

    if ( count ) tm_msg("unmark: %lu nodes\n", count);
  }
}

static void tm_unmark_some_nodes()
{
  tm_node *n;

  if ( tm_unmark_okay ) {
    if ( (n = tm_list_first(&tm_color_list[tm_color_VIOLET])) ) {
      tm_node_set_color(n, 0, tm_color_ECRU);
      tm_msg("unmark: %p\n", n);
    }
  }
}


int tm_sweep_is_error = 0;

static void tm_unmark_and_sweep()
{
  /*
  ** If there are no nodes left to mark,
  ** everything left in the allocated list
  ** must be garbage.
  */
  if ( ! tm_marking ) {
    /* Force a full root scan to be sure. */
    tm_scan_all_roots();

    // tm_validate_lists();

    if ( ! tm_marking ) {
      tm_node *n;
	  
      tm_msg("collecting: %lu free, %lu marking, %lu scanned\n",
	     tm_color_n[tm_color_ECRU],
	     tm_color_n[tm_color_GREY],
	     tm_color_n[tm_color_BLACK]);

      if ( tm_sweep_is_error ) {
	if ( ! tm_list_empty(&tm_color_list[tm_color_ECRU]) ) {
	  tm_list_LOOP(&tm_color_list[tm_color_ECRU], n);
	  {
	    tm_msg("fatal: node %p color %d size %lu should not be sweeped!\n", (void*) n, tm_node_color(n), (unsigned long) tm_node_to_type(n)->size);
	    {
	      void ** vpa = tm_node_to_ptr(n);
	      tm_msg("fatal: cons (%d, %p)\n", (int) vpa[0], vpa[1]);
	    }
	  }
	  tm_list_LOOP_END;

	  abort();
	}
      }

      /* Force sweeped nodes to actually be VIOLET so they can be remarked. */
      tm_list_append_list(&tm_color_list[tm_color_YELLOW], &tm_color_list[tm_color_ECRU]);
      tm_list_LOOP(&tm_color_list[tm_color_YELLOW], n);
      {
	_tm_node_set_color(n, tm_node_to_type(n), tm_color_YELLOW);
      }
      tm_list_LOOP_END;

      /* Force unmarked nodes to actually be VIOLET so they can be remarked. */
      tm_list_append_list(&tm_color_list[tm_color_VIOLET],  &tm_color_list[tm_color_BLACK]);
      tm_list_LOOP(&tm_color_list[tm_color_VIOLET], n);
      {
	_tm_node_set_color(n, tm_node_to_type(n), tm_color_VIOLET);
      }
      tm_list_LOOP_END;
    }
  }
}


/***************************************************************************/
/* full gc */

static void tm_gc_full_inner()
{
  tm_node *n;

  tm_msg("gc: start\n");

  /* Sweep the nodes. */
  tm_sweep_all_nodes();

  /* Unmark the nodes. */
  tm_unmark_all_nodes();

  /* Put all used nodes back on unmarked list. */
  while ( (n = tm_list_first(&tm_color_list[tm_color_GREY])) ) {
    tm_node_set_color(n, 0, tm_color_ECRU);
  }
  while ( (n = tm_list_first(&tm_color_list[tm_color_BLACK])) ) {
    tm_node_set_color(n, 0, tm_color_ECRU);
  }

  /* Scan all roots */
  tm_scan_all_roots();

  /* Scan all marked nodes */
  tm_scan_all_nodes();

  tm_unmark_and_sweep();

  assert(! tm_marking);

  /* Sweep the nodes. */
  tm_sweep_all_nodes();

  /* Unmark the nodes. */
  tm_unmark_all_nodes();

  tm_msg("gc: complete\n");
}


/***************************************************************************/
/* allocation */

int tm_alloc_pass_max = 0;

static void *tm_alloc_inner(size_t size)
{
  tm_type *t;
  tm_node *n = 0;
  char *ptr = 0;

  tm_alloc_id ++;
  tm_alloc_pass = 1;
  memset(tm_.alloc_n, 0, sizeof(tm_.alloc_n));

  t = tm_size_to_type(size);
  
  do {
    /* Scan some roots for pointers. */
    tm_scan_some_roots();

    /* Scan a little bit. */
    tm_scan_some_nodes();

    /* Take one from the free list. */
    if ( (n = tm_list_take_first(&t->free_list)) ) {

      ptr = tm_node_to_ptr(n);

      if ( tm_ptr_l > (void*) ptr ) {
	tm_ptr_l = ptr;
      }
      if ( tm_ptr_h < (void*) (ptr + t->size) ) {
	tm_ptr_h = ptr + t->size;
      }

      /* Put it on the marked list. */
      /* Assume we are allocating it to use it. */
      tm_node_set_color(n, t, tm_color_GREY);

      break;
    }

    /* Check if we can sweep and unmark. */
    tm_unmark_and_sweep();

    /* Sweep some nodes. */
    tm_sweep_some_nodes();

    /* Unmark some nodes. */
    tm_unmark_some_nodes();
        
    /*
    ** If there are still no possibly free nodes,
    ** then
    **    allocate a new page for the type.
    */
    if ( ! (t->n[tm_color_WHITE] + t->n[tm_color_ECRU] + t->n[tm_color_YELLOW] + t->n[tm_color_VIOLET]) ) {
      tm_alloc_some_nodes(t);
    }

    tm_alloc_pass ++;
  } while ( 1 );

  tm_msg("alloc: %lu: %p\n", (unsigned long) size, (void*) n);

  if ( tm_alloc_pass_max < tm_alloc_pass ) {
    tm_alloc_pass_max = tm_alloc_pass;
    tm_msg("pass: max passes %d\n", tm_alloc_pass_max);
    tm_print_utilization("pass: ", tm_.alloc_n);
  }

  return ptr;
}

/***************************************************************************/
/* user level routines */

static void _tm_clear_some_stack_words()
{
  int zero[16];
  memset(zero, 0, sizeof(zero));
}


#define tm_clear_some_stack_words() \
_tm_clear_some_stack_words(); \
setjmp(tm_jb); \
tm_scan_root_id(0)

/*
** tm_write_barrier(ptr) must be called before any ptrs
** within node ptr are mutated.
*/
__inline void tm_write_barrier_node(tm_node *n)
{
  switch ( tm_node_color(n) ) {
  case tm_color_ECRU:
  case tm_color_GREY:
    /* Not scanned yet. */
    break;

  case tm_color_BLACK:
  case tm_color_VIOLET:
    /*
    ** The node has already been scanned.
    ** The mutator may introduce new pointers.
    ** Force it to be rescanned.
    */
    tm_node_set_color(n, 0, tm_color_GREY);
    tm_msg("write barrier: %p\n", n);
    break;

  default:
  case tm_color_WHITE:
  case tm_color_YELLOW:
    abort();
    break;
  }
}

void tm_write_barrier(void *ptr)
{
  tm_node *n;

  if ( (n = tm_ptr_to_node(ptr)) )
    tm_write_barrier_node(n);
}

void tm_write_barrier_pure(void *ptr)
{
  if ( ! ptr )
    return;

  tm_write_barrier_node(tm_pure_ptr_to_node(ptr));
}

/*
** tm_root_write() should be called after modifing a root ptr.
*/
void tm_root_write(void **ptrp)
{
  tm_mark_possible_ptr(* ptrp);
}

void *tm_alloc(size_t size)
{
  void *ptr = 0;

  if ( size == 0 )
    return 0;

  memset(&tm_.alloc_time, 0, sizeof(tm_.alloc_time));
  times(&tm_.alloc_time);
  tm_msg("Times before: %lu %lu %lu %lu\n", 
	 tm_.alloc_time.tms_utime,
	 tm_.alloc_time.tms_stime,
	 tm_.alloc_time.tms_cutime,
	 tm_.alloc_time.tms_cstime);

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  ptr = tm_alloc_inner(size);

  times(&tm_.alloc_time);
  tm_msg("Times after: %lu %lu %lu %lu\n", 
	 tm_.alloc_time.tms_utime,
	 tm_.alloc_time.tms_stime,
	 tm_.alloc_time.tms_cutime,
	 tm_.alloc_time.tms_cstime);

  return ptr;
}

void tm_gc_full()
{
  void *ptr = 0;

  tm_clear_some_stack_words();
  tm_set_stack_ptr(&ptr);
  tm_gc_full_inner();
}

/***************************************************************************/


