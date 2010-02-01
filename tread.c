#include "tread.h"


const char *__tm_color_name[] = { 
  "WHITE",
  "ECRU",
  "GREY",
  "BLACK",
  0
};

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
      int c = t->c1[tm_node_color(node)];
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
	  (int) t->n[t->c[tm_WHITE]]);
  fprintf(fp, "\"bottom\" [ shape=box ];\n");
  fprintf(fp, "\"top\"    [ shape=box ];\n");
  fprintf(fp, "\"scan\"   [ shape=box, label=\"scan: %d\" ];\n",
	  (int) t->n[t->c[tm_GREY]]);

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

