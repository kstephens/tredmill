/* $Id: tmtest.c,v 1.5 1999-12-28 20:42:17 stephensk Exp $ */
#include "tm.h"
#include <stdio.h>
#include <stdlib.h> /* rand() */
#include <assert.h>

static int nalloc = 1000;
static int nsize = 100;

static void my_prompt()
{
#if 0
      fprintf(stderr, "Press return.\n");
      fgetc(stdin);
#endif 
}

#define my_rand(R) ((unsigned long) (rand() & 32767) * (R) / 32768)

static const char _run_sep[] = "*************************************************************\n";

static const char *_run_name = "<UNKNOWN>";

#define my_gc_full() tm_gc_full()
#define my_print_stats() tm_print_stats(); tm_print_block_stats()

static void _run_test(const char *name, void (*func)())
{
  tm_msg_prefix = name;
  _run_name = name;
  tm_msg(_run_sep);
  tm_msg("* %s\n", _run_name);

  func();
  my_prompt();

  tm_msg(_run_sep);
  tm_msg("* %s END\n", _run_name);
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

static void *my_alloc(size_t size)
{
  void *ptr;

  ptr = tm_alloc(size);

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
  tm_sweep_is_error = 1;

  for ( i = 0; i < nalloc; i ++ ) {
    size_t size = (1 << (i & 3)) * 8;

    assert(roots[i] == 0);

    /* A pow of two */
    roots[i] = tm_alloc(size);
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

static void print_my_cons_list_internal(my_cons *p)
{
  if ( ((unsigned long) p) & 3 ) {
    tm_msg1("%lu", (long) p >> 2);
  } else {
    tm_msg1("(");
    
    while ( p ) {
      if ( p->visited ) {
	tm_msg1("#%p", (void*) p);
	break;
      } else {
	my_cons *car = p->car;
	
	p->visited ++;
	p->car = (void*) ((-1 << 2) + 1);
	print_my_cons_list_internal(car);
	p->car = car;
	if ( (((unsigned long) p->cdr) & 3) == 0 ) {
	  tm_msg1(" ");
	  p = p->cdr;
	} else {
	  tm_msg1(" . ");
	  print_my_cons_list_internal(p->cdr);
	  break;
	}
      }
    }
    
    tm_msg1(")");
  }
}

static void print_my_cons_list(const char *name, my_cons *p)
{
  tm_msg("*: %s list: {\n", name);
  print_my_cons_list_internal(p);
  tm_msg1("\n");
  tm_msg("*: %s list: }\n", name);
}

static void *my_int(int i)
{
  return (void*) (i << 2) + 1;
}

static void *my_cons_(void *a, void *d, int nsize)
{
  my_cons *c;
  int size;

  if ( nsize ) {
    size = sizeof(*c) + my_rand(nsize);
  } else {
    size = sizeof(*c);
  }
  c = my_alloc(size);
  c->car = a;
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
  tm_sweep_is_error = 1;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(my_int(i), root, nsize);
    root = c;
    // print_my_cons_list(root);
  }

  tm_sweep_is_error = 0;

  print_my_cons_list("test4", root);

  end_test();

  print_my_cons_list("test4", root);
}

static my_cons *test5_root = 0;

static void test5()
{
  int i;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(my_int(i), test5_root, nsize);
    if ( (i & 3) == 0 ) 
      test5_root = c;
    // print_my_cons_list(root);
  }

  print_my_cons_list("test5", test5_root);

  end_test();

  print_my_cons_list("test5", test5_root);

  test5_root = 0;
}

static void test6()
{
  int i;
  my_cons *root = 0;
  my_cons **rp = &root;

  tm_gc_full();
  print_my_cons_list("test6", test5_root);

  tm_sweep_is_error = 1;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(my_int(i), 0, nsize);
    *rp = c;
    tm_write_barrier(*rp);
    rp = &(c->cdr);
  }

  tm_sweep_is_error = 0;

  print_my_cons_list("test6", root);

  end_test();

  print_my_cons_list("test6", root);
}

#if 1
static void test7()
{
  int i;
  my_cons *root = 0, *roots[100];

  memset(roots, 0, sizeof(roots));
  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c = my_cons_(my_int(i), root, nsize);

    root = c;

    if ( (i & 3) == 0 ) {
      int size = my_rand(nsize);
      my_cons **p = &roots[size % 100];
      if ( *p ) {
	(*p)->cdr = c;
	tm_write_barrier_pure(*p);
      } else {
	*p = c;
	tm_write_root((void**) p);
      }
    }
  }

  print_my_cons_list("test7", root);

  end_test();
}
#endif

int main(int argc, char **argv, char **envp)
{

  srand(0x54ae3523);

  tm_msg(_run_sep);
  tm_msg("* %s\n", "START");
  
  tm_init(&argc, &argv, &envp);

  run_test(test1);
  run_test(test2);
  run_test(test3);
  run_test(test4);
  run_test(test5);
  run_test(test6);
  run_test(test7);

  tm_gc_full();
  tm_print_stats();

  tm_msg(_run_sep);
  tm_msg("* %s\n", "END");

  return 0;
}


