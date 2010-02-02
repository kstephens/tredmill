#define tm_tread_UNIT_TEST 1
#include "tread.h"

#include <stdio.h>

static 
void render_dot(tm_tread *t, tm_node *mark, const char *desc);

#define N 60
static int seed;
static int nodes_parceled = 0;
static tm_colors colors;
static tm_node nodes[N];
static tm_tread _t, *t = &_t;

void tm_tread_mark_roots(tm_tread *t)
{
  int j;

  // render_dot(t, 0, "flipped");

  fprintf(stderr, "  tm_tread_mark_roots(%p)\n", (void*) t);
  for ( j = 0; j < 4; ++ j ) {
    int i = rand() % nodes_parceled;
    tm_node *n = &nodes[i];
    if ( tm_node_color(n) == t->c->c[tm_ECRU] ) {
      // fprintf(stderr, "    %p\n", (void*) n);
      tm_tread_mark(t, n);
      // render_dot(t, n, "mark root @ ");
    }
  }
}


void tm_node_scan(tm_node *n)
{
  if ( (((unsigned long) n) / sizeof(tm_node)) % 2 == 0 ) {
    int i = rand() % nodes_parceled;
    tm_node *r = &nodes[i];
    if ( tm_node_color(r) == t->c->c[tm_ECRU] ) {
      // fprintf(stderr, "    %p => %p\n", (void*) n, (void*) r);
      tm_tread_mark(t, r);
    }
    // render_dot(t, n, "scanned @ ");
  }
}


int tm_tread_more_white(tm_tread *t)
{
  int result = 0;
  int i;

  fprintf(stderr, "  tm_tread_more_white(%p)\n", (void*) t);
  for ( i = 0; i < 4; ++ i ) {
    if ( nodes_parceled < N ) {
      tm_node *n = &nodes[nodes_parceled ++];
      // fprintf(stderr, "    %p\n", (void*) n);
      tm_tread_add_white(t, n);
      // render_dot(t, n, "more white @ ");
      result ++;
    } else {
      break;
    }
  }
  return result;
}


static 
void render_dot(tm_tread *t, tm_node *mark, const char *desc)
{
  FILE *fp;
  static int filename_i = 0;
  char filename_dot[64];
  char filename_svg[64];
  char filename_html[64];

  sprintf(filename_dot, "images/tread-%03d.dot", filename_i);
  sprintf(filename_svg, "images/tread-%03d.svg", filename_i);
  sprintf(filename_html, "images/tread-%03d.html", filename_i);

  fp = fopen(filename_dot, "w+");

  if ( mark ) {
    static char buf[64];
    sprintf(buf, "%s %p", desc, mark);
    desc = buf;
  }

  tm_tread_render_dot(fp, t, desc, 1, &mark);

  fclose(fp);
  fprintf(stderr, "wrote %s\n", filename_dot);

  {
    char cmd[128];
    sprintf(cmd, "neato -Tsvg:cairo:cairo -o %s %s", filename_svg, filename_dot);
    if ( system(cmd) ) {
      fprintf(stderr, "command %s: failed\n", cmd);
      exit(1);
    }
  }


  fp = fopen(filename_html, "w+");
  fprintf(fp,""
	  "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
	  "<html xmlns=\"http://www.w3.org/1999/xhtml\" dir=\"ltr\">\n"
	  "  <head>\n"
	  "<title>%s</title>\n"
	  "</head>\n"
	  "<body>\n"
	  "<h1>%s %d</h1>\n"
	  "<pre>%5d W %5d E %5d G %5d B %5d T</pre>\n"
	  "<div>\n"
	  "<table>\n"
	  "<tr>\n"
	  "  <td align=\"center\"><a href=\"tread-%03d.html\">&lt;&lt;</td>\n"
	  "  <td align=\"center\"><a href=\"tread-%03d.svg\">###</a></td>\n"
	  "  <td align=\"center\"><a href=\"tread-%03d.html\">&gt;&gt;</a></td>\n"
	  "</tr>\n"
	  "</table>\n"
	  "</div>\n"
	  "<div>\n"
	  "<table>\n"
	  "<tr>\n"
	  "  <td><object data=\"tread-%03d.svg\" type=\"image/svg+xml\" /></td>\n"
	  "  <td><object data=\"tread-%03d.svg\" type=\"image/svg+xml\" /></td>\n"
	  "  <td><object data=\"tread-%03d.svg\" type=\"image/svg+xml\" /></td>\n"
	  "</tr>\n"
	  "</table>\n"
	  "</div>\n"
	  "</body>\n"
	  "</html>\n"
	  "%s",
	  filename_html,
	  filename_html, seed,
	  t->n[t->c->c[tm_WHITE]], t->n[t->c->c[tm_ECRU]], t->n[t->c->c[tm_GREY]], t->n[t->c->c[tm_BLACK]], t->n[tm_TOTAL],
	  filename_i - 1,
	  filename_i,
	  filename_i + 1,
	  filename_i - 1,
	  filename_i,
	  filename_i + 1,
	  ""
	  );
  fclose(fp);
  fprintf(stderr, "  wrote %s\n", filename_html);

  ++ filename_i;
}


int main(int argc, char **argv)
{
  int i;
  tm_node *n = 0;

  if ( argc > 1 ) {
    seed = (atoi(argv[1]));
  } else {
    time_t t;
    time(&t);
    seed = (t ^ getpid());
  }
  srand(seed);

  tm_colors_init(&colors);
  tm_tread_init(t);
  t->c = &colors;
  render_dot(t, 0, "initialized");

  for ( i = 0; i < 2; ++ i ) {
    tm_tread_more_white(t);
    render_dot(t, 0, "more white");
  }

  for ( i = 0; i < N * 4; ++ i ) {
    n = tm_tread_allocate(t);
    render_dot(t, n, "allocate => ");
    if ( n == 0 )
      break;

    if ( rand() % 2 == 0 ) {
      int i = rand() % nodes_parceled;
      tm_node *r = &nodes[i];
      if ( tm_node_color(r) != t->c->c[tm_WHITE] ) {
	tm_tread_mutation(t, r);
	render_dot(t, r, "mutation @ ");
      }
    }
  }

  render_dot(t, n, n ? "FINISHED" : "out of memory");
  
  return 0;
}

