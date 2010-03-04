#include "tread.h"
#include "tredmill/debug.h"
#include "tredmill/tm_data.h"


/**
 * Validates a tread.
 */
void tm_tread_validate(tm_tread *t)
{
  tm_node *n;
  size_t c[tm__LAST];
  size_t j;

  memset(c, 0, sizeof(c));

  if ( t->free ) {
    n = t->free;
    do {
      ++ c[tm.colors.c1[tm_node_color(n)]];
      ++ c[tm_TOTAL];
      n = tm_node_next(n);
    } while ( n != t->free );

    tm_assert_equal(c[tm_WHITE], t->n[WHITE], "%lu");
    tm_assert_equal(c[tm_ECRU],  t->n[ECRU],  "%lu");
    tm_assert_equal(c[tm_GREY],  t->n[GREY],  "%lu");
    tm_assert_equal(c[tm_BLACK], t->n[BLACK], "%lu");
    tm_assert_equal(c[tm_TOTAL],  t->n[tm_TOTAL], "%lu");
  }

  /* All nodes from scan to top are GREY */
  j = 0;
  for ( n = t->scan; n != t->top; n = tm_node_next(n) ) {
    tm_assert_equal(tm_node_color(n), GREY, "%d");
    ++ j;
  }
  tm_assert_equal(j, c[tm_GREY], "%lu");

  /* All nodes from free to bottom are WHITE */
  if ( c[tm_WHITE] > 1 && 0 ) {
    j = 0;
    for ( n = t->free; n != t->bottom; n = tm_node_next(n) ) {
      tm_assert_equal(tm_node_color(n), WHITE, "%d");
      ++ j;
    }
    tm_assert_equal(j, c[tm_WHITE], "%lu");
  }
}

/**
 * Render a tread to a graphviz dot file.
 */
void tm_tread_render_dot(FILE *fp, tm_tread *t, const char *desc, int markn, tm_node **marks)
{
  static
    const char *fillcolor[] = {
    "white",
    "#C2B280",
    "grey",
    "black",
    0
  };

  static
    const char *fontcolor[] = {
    "black",
    "black",
    "black",
    "white",
    0,
  };
  
  static
    const char *style[] = {
    "filled,dotted",
    "filled",
    "filled",
    "filled",
    0,
  };

  tm_node *first = 0, *node, *prev, *next;

  fprintf(fp, "digraph dg { \n");
  fprintf(fp, "  label=\"%s\"\n", desc ? desc : "");

  if ( t->free ) {
    first = t->free;

    node = t->free;
    do {
      if ( first > node )
	first = node;
      node = tm_node_next(node);
    } while ( node != t->free );
  }

  if ( first ) {
    node = first;
    do {
      int c = tm.colors.c1[tm_node_color(node)];
      next = tm_node_next(node);
      int i, marked = 0;
      for ( i = 0; i < markn; ++ i ) {
	if ( marks[i] == node ) {
	  marked = 1;
	  break;
	}
      }

      fprintf(fp, "\"n%p\" [ fontsize=8, shape=\"%s\", color=\"%s\", fontcolor=\"%s\", fillcolor=\"%s\", style=\"%s\" ];\n", 
	      (void *) node,
	      marked ? "octagon" : "ellipse",
	      marked ? "red"     : "black",
	      fontcolor[c],
	      fillcolor[c],
	      style[c]);
      node = next;
    } while ( node != first );
  }
  else {
    fprintf(fp, "\"n%p\" [ label=\"0\", color=grey, shape=none ];\n", 
	    (void *) 0);
  }

  fprintf(fp, "\"free\"   [ shape=box, label=\"free: %d\" ];\n",
	  (int) t->n[WHITE]);
  fprintf(fp, "\"bottom\" [ shape=box ];\n");
  fprintf(fp, "\"top\"    [ shape=box ];\n");
  fprintf(fp, "\"scan\"   [ shape=box, label=\"scan: %d\" ];\n",
	  (int) t->n[GREY]);

  if ( first ) {
    node = first;
    prev = tm_node_prev(node);
    do {
      next = tm_node_next(node);

      fprintf(fp, "\"n%p\" -> \"n%p\" [ label=\"\",  color=\"black\" ];\n", 
	      (void *) node,
	      (void *) next);
#if 1
      fprintf(fp, "\"n%p\" -> \"n%p\" [ label=\"\",  color=\"grey\" ];\n",
	      (void *) node,
	      (void *) prev);
#endif

      prev = node;
      node = next;
    } while ( node != first );
  }

  fprintf(fp, "\"free\"   -> \"n%p\";\n", t->free);
  fprintf(fp, "\"bottom\" -> \"n%p\";\n", t->bottom);
  fprintf(fp, "\"top\"    -> \"n%p\";\n", t->top);
  fprintf(fp, "\"scan\"   -> \"n%p\";\n", t->scan);

  fprintf(fp, "}\n");
}

