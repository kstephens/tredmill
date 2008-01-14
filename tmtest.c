/* $Id: tmtest.c,v 1.13 2008-01-14 00:08:02 stephens Exp $ */
#include "tm.h"
#include <stdio.h>
#include <stdlib.h> /* rand() */
#include <string.h> /* memset() */
#include <assert.h>

#include "internal.h" /* tm_abort() */


static int nalloc = 1000;
static int nsize = 100;

static void my_prompt()
{
#if 0
      fprintf(stderr, "Press return.\n");
      fgetc(stdin);
#endif 
}


#define my_rand(R) ((unsigned long) ((rand() >> 4) & 32767) * (R) / 32768)

static const char _run_sep[] = "*************************************************************\n";

static const char *_run_name = "<UNKNOWN>";

#define my_gc_full() tm_gc_full()
#define my_print_stats() tm_print_stats(); tm_print_block_stats()

#define SWEEP_IS_ERROR 0


static void out_of_memory()
{
  
}

static int my_alloc_id = 1;
static void *my_alloc(size_t size)
{
  void *ptr;

  ptr = tm_alloc(size);

  if ( ptr == 0 && size != 0 ) {
    fprintf(stderr, "\ntmtest: %s: OUT OF MEMORY!!\n", _run_name);
    out_of_memory();
    tm_abort();
    return 0; /* NOTREACHED */
  }

  my_alloc_id = tm.alloc_id;

#if 0
  if ( tm_sweep_is_error ) {
    tm_gc_full();
  }
#endif

  {
    static unsigned long mc = 0;
    mc ++;
#if 0
    if ( mc % (nalloc / 2) == 0 ) {
      tm_msg(_run_sep);
      tm_msg1("* %s allocation check \n", _run_name);
      my_print_stats();
      my_prompt();
    }
#endif

#if 0 
    if ( mc == 20 )
      abort();
#endif
  }

  return ptr;
}


static void _run_test(const char *name, void (*func)())
{
  tm_msg_prefix = name;
  _run_name = name;
  tm_msg(_run_sep);
  tm_msg("* %s\n", _run_name);
  tm_msg(" my_alloc_id = %d\n", my_alloc_id);

  func();
  my_prompt();

  tm_msg(_run_sep);
  tm_msg("* %s END\n", _run_name);
  tm_msg(" my_alloc_id = %d\n", my_alloc_id);
  my_gc_full();
  my_print_stats();

  tm_msg(_run_sep);
  tm_msg("\n");
  my_prompt();

  my_gc_full();
}


static void _end_test()
{
  tm_msg(_run_sep);
  tm_msg("* %s POST\n", _run_name);

  my_print_stats();
  my_prompt();

  tm_msg(_run_sep);
  tm_msg("* %s POST GC\n", _run_name);
  my_gc_full();
  my_print_stats();
  my_prompt();

  tm_msg(_run_sep);
  tm_msg("* %s POST END \n", _run_name);
}


#define run_test(X) _run_test(#X, &X)
#define end_test() _end_test()


static void test1()
{
  int i;
  int size = 0;
  
  tm_msg("*: random sizes.\n");

  for ( i = 0; i < nalloc; i ++ ) {
    size = my_rand(nsize);
    my_alloc(size);
  }

  end_test();
}


static void test2()
{
  int i;
  int size = 0;

  tm_msg("*: increasing sizes.\n");
  
  for ( i = 0; i < nalloc; i ++ ) {
    size = i * nsize / nalloc;
    my_alloc(size);
  }

  end_test();
}


static void test3()
{
  int i;
  char *roots[nalloc];

  memset(roots, 0, sizeof(roots));

  tm_msg("*: check for end-of-node ptr validitity.\n");

  tm_gc_full();
  tm_gc_full();
  tm_sweep_is_error = SWEEP_IS_ERROR;

  for ( i = 0; i < nalloc; i ++ ) {
    size_t size = (1 << (i & 3)) * 8;

    assert(roots[i] == 0);

    /* A pow of two */
    roots[i] = my_alloc(size);
    /* Move ptr to end-of-node */
    roots[i] += size;
  }

  tm_gc_full();

  tm_sweep_is_error = 0;

  end_test();
}


typedef struct my_cons {
  struct my_cons *car, *cdr;
  int visited;
} my_cons;



static int print_my_cons_list_internal(my_cons *_p)
{
  int result = 0;
  my_cons *p = _p;

  if ( ((unsigned long) p) & 3 ) {
    tm_msg1("%lu", (long) p >> 2);
    ++ result;
  }
  else if ( p == 0 ) {
    tm_msg1("()");
  }
  else {
    ++ _p->visited;

    do {
      if ( p != _p && p->visited ) {
	tm_msg1("#%p", (void*) p);
	break;
      } else {
	my_cons *car = p->car;
	
	p->car = (void*) ((-1 << 2) + 1);
	++ p->visited;
	result += print_my_cons_list_internal(car);
	-- p->visited;
	p->car = car;

	if ( (((unsigned long) p->cdr) & 3) == 0 ) {
	  tm_msg1(" ");
	  p = p->cdr;
	} else {
	  tm_msg1(" . ");
	  ++ p->visited;
	  result += print_my_cons_list_internal(p->cdr);
	  -- p->visited;
	  break;
	}

      }
    } while ( p );
    
    if ( _p ) {
      -- _p->visited;
    }

    tm_msg1(")");
  }

  return result;
}


static int print_my_cons_list(const char *name, my_cons *p)
{
  int result;

  tm_msg("*: %s list: {\n", name);
  result = print_my_cons_list_internal(p);
  tm_msg1("\n");
  tm_msg("*: %s list: }\n", name);

  fprintf(stderr, "%s list depth = %d\n", name, result);
 
  return result;
}


static void *my_int(int i)
{
  return (void*) (i << 2) + 1;
}


static void *my_cons_(void *d, int nsize)
{
  my_cons *c;
  int size;

  if ( nsize ) {
    size = sizeof(*c) + my_rand(nsize);
  } else {
    size = sizeof(*c);
  }
  c = my_alloc(size);
  tm_write_barrier(&c);

  c->car = my_int(my_alloc_id);
  c->cdr = d;
  c->visited = 0;
  tm_write_barrier_pure(c);

  return c;
}


static void test4()
{
  int i;
  my_cons *root = 0;

  tm_gc_full();
  tm_sweep_is_error = SWEEP_IS_ERROR;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(root, nsize);
    root = c;
    // print_my_cons_list(root);
  }

  tm_sweep_is_error = 0;

  print_my_cons_list("test4 root", root);

  end_test();

  print_my_cons_list("test4 root", root);

  root = 0;
}


static my_cons *test5_root = 0;


static void test5()
{
  int i;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(test5_root, nsize);
    if ( (i & 3) == 0 ) {
      test5_root = c;
      tm_write_barrier(&test5_root);
    }
    // print_my_cons_list(root);
  }

  print_my_cons_list("test5 test5_root", test5_root);

  end_test();

  print_my_cons_list("test5 test5_root", test5_root);

  test5_root = 0;
  tm_write_barrier(&test5_root);

  tm_gc_full();
}


static void test6()
{
  int i;
  my_cons *root = 0;
  my_cons **rp = &root;

  tm_gc_full();

  tm_sweep_is_error = SWEEP_IS_ERROR;

  tm_msg_enable("w", 1);
  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(0, nsize);
    *rp = c;
    tm_write_barrier(&*rp);
    rp = &(c->cdr);
  }

  tm_sweep_is_error = 0;

  print_my_cons_list("test6", root);

  end_test();

  print_my_cons_list("test6", root);
  tm_msg_enable("w", 0);

  root = 0;
  rp = 0;
}


#if 1
static void test7()
{
  int i;
  my_cons *root = 0, *roots[100];

  memset(roots, 0, sizeof(roots));
  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(root, nsize);

    root = c;
    tm_write_barrier(&root);

    if ( (i & 3) == 0 ) {
      my_cons **p = &roots[my_rand(sizeof(roots) / sizeof(roots[0]))];
      if ( *p ) {
	(*p)->cdr = c;
	tm_write_barrier_pure(*p);
      } else {
	*p = c;
	tm_write_barrier(&*p);
      }
    }
  }

#if 0
  print_my_cons_list("test7", root);
#endif

  end_test();

  root = 0;
  tm_write_barrier(&root);

  memset(roots, 0, sizeof(roots));
}
#endif


#if 1
static void test8()
{
  int i;
  my_cons *root = 0, *roots[20];

  memset(roots, 0, sizeof(roots));
  do { 
    for ( i = 0; i < nalloc; ++ i ) {
      my_cons *c = my_cons_(0, nsize);
      
      root = c;
      tm_write_barrier(&root);

      /* If c is not garbage. */
      if ( my_rand(2) == 0 ) {
	my_cons **p = &roots[my_rand(sizeof(roots) / sizeof(roots[0]))];

	/* chain c->cdr to root[p] */
	if ( my_rand(10) ) {
	  c->cdr = *p;
	  tm_write_barrier_pure(c);
	} else {
	  /* Put on root list. */
	  *p = c;
	  tm_write_barrier(&*p);
	}
      }
    }

    print_my_cons_list("test8 root", root);
    for ( i = 0; i < sizeof(roots) / sizeof(roots[0]); ++ i ) {
      char buf[32];
      sprintf(buf, "test8 roots[%d]", i);
      print_my_cons_list(buf, roots[i]);
    }
    tm_print_stats();
  } while ( tm.alloc_id < 40000000 );

  end_test();

  root = 0;
  tm_write_barrier(&root);
  memset(roots, 0, sizeof(roots)); 
  tm_write_barrier(&roots);
}
#endif


int main(int argc, char **argv, char **envp)
{
  fprintf(stderr, "%s: running\n", argv[0]);

  tm_os_alloc_max = 32 * 1024 * 1024;

  srand(0x54ae3523);

#if 1
  tm_init(&argc, &argv, &envp);
#endif

  tm_msg(_run_sep);
  tm_msg("* %s\n", "START");
  
  //tm_msg_enable("T", 1);

  run_test(test1);
  run_test(test2);
  run_test(test3);
  run_test(test4);
  run_test(test5);
  run_test(test6);
  run_test(test7);
  run_test(test8);

  tm_gc_full();
  tm_print_stats();

  tm_msg(_run_sep);
  tm_msg("* %s\n", "END");

  fprintf(stderr, "%s: stopped\n", argv[0]);

  return 0;
}


