/* $Id: tmtest.c,v 1.1 1999-06-09 07:00:51 stephensk Exp $ */
#include "tm.h"
#include <stdio.h>
#include <stdlib.h> /* rand() */

static int nalloc = 8000;
static int nsize = 20;

static void my_prompt()
{
#if 0
      fprintf(stderr, "Press return.\n");
      fgetc(stdin);
#endif 
}

static const char _run_sep[] = "*************************************************************\n";

static const char *_run_name = "<UNKNOWN>";

static void _run_test(const char *name, void (*func)())
{
  _run_name = name;
  tm_msg(_run_sep);
  tm_msg("* %s\n", _run_name);

  func();
  my_prompt();

  tm_msg(_run_sep);
  tm_msg("* %s END\n", _run_name);
  //tm_gc_full();
  //tm_print_stats();

  tm_msg(_run_sep);
  tm_msg("\n");
  my_prompt();

  //tm_gc_full();
}

static void _end_test()
{
  tm_msg(_run_sep);
  tm_msg("* %s POST\n", _run_name);

  tm_print_stats();
  my_prompt();

  tm_msg(_run_sep);
  tm_msg("* %s POST GC\n", _run_name);
  // tm_gc_full();
  // tm_print_stats();
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
      tm_print_stats();
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
  
  for ( i = 0; i < nalloc; i ++ ) {
    size = rand() % nsize;
    my_alloc(size);
  }

  end_test();
}

static void test2()
{
  int i;
  int size = 0;
  
  for ( i = 0; i < nalloc; i ++ ) {
    size = i * nsize / nalloc;
    my_alloc(size);
  }

  end_test();
}

typedef struct my_cons {
  struct my_cons *car, *cdr;
} my_cons;

static void print_my_cons_list(my_cons *p)
{
  tm_msg("Test: list: ( ");
  for ( ; p; p = p->cdr ) {
    tm_msg1("%d ", (int) p->car);
  }
  tm_msg1(")\n");
}

static void test3()
{
  int i;
  my_cons *root = 0;

  tm_gc_full();
  tm_sweep_is_error = 1;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c;
    int size = sizeof(*c) + (rand() % nsize);

    c = my_alloc(size);
    c->car = (void*) i;
    c->cdr = root;
    tm_write_barrier_pure(c);

    root = c;
    // print_my_cons_list(root);
  }

  tm_sweep_is_error = 0;

  print_my_cons_list(root);

  end_test();

  print_my_cons_list(root);
}

static void test4()
{
  int i;
  my_cons *root = 0;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c;
    int size = sizeof(*c) + (rand() % nsize);

    c = my_alloc(size);
    c->car = (void*) i;
    c->cdr = root;
    tm_write_barrier_pure(c);

    if ( (i & 3) == 0 ) 
      root = c;
    // print_my_cons_list(root);
  }

  print_my_cons_list(root);

  end_test();

  print_my_cons_list(root);
}

static void test5()
{
  int i;
  my_cons *root = 0;
  my_cons **rp = &root;

  tm_gc_full();
  tm_sweep_is_error = 1;

  for ( i = 0; i < nalloc; i ++ ) {
    my_cons *c;
    int size = sizeof(*c) + (rand() % nsize);

    c = my_alloc(size);
    c->car = (void*) i;
    c->cdr = 0;
    tm_write_barrier_pure(c);

    *rp = c;
    tm_write_barrier(*rp);
    rp = &(c->cdr);
  }

  tm_sweep_is_error = 0;

  print_my_cons_list(root);

  end_test();

  print_my_cons_list(root);
}

int main(int argc, char **argv, char **envp)
{
  tm_init(&argc, &argv, &envp);

  run_test(test5);
  run_test(test4);
  run_test(test3);
  run_test(test2);
  run_test(test1);

  tm_gc_full();
  tm_print_stats();

  return 0;
}

