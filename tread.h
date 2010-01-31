#ifndef _tredmill_TREAD_H
#define _tredmill_TREAD_H


struct tm_tread {
  tm_node *free;
  tm_node *bottom;
  tm_node *top;
  tm_node *scan;
  size_t  n[tm_LAST];
  int white, ecru, grey, black;
};

void tm_tread_init(tm_tread *t)
{
  t->free = t->bottom = t->top = t->scan = 0;
  t->white = 0;
  t->ecru = 1;
  t->grey = 2;
  t->black = 3;
}

void tm_tread_add_white(tm_node *t, tm_node *n)
{
  if ( ! t->free ) {
    t->free = t->bottom = t->top = t->scan = n;
  } else {
    tm_list_insert(t->bottom, n);
  }
}

tm_node *tm_tread_take_white(tm_tread *t)
{
  tm_node *n;

  if ( t->free != t->bottom ) {
    n = t->free = tm_list_next(t->free);
    tm_list_set_color(n, t->black);
  }
  return n;
}

int tm_tread_scan(tm_tread *t)
{
  if ( t->scan != t->top ) {
    n = t->scan = tm_list_prev(t->scan);
    tm_list_set_color(n, t->black);
    tm_node_scan(n, t->type);
    return 1;
  } else {
    return 0;
  }
}
int tm_tread_mark(tm_tread *t, tm_node *n)
{
  if ( tm_node_color(n) == t->black || tm_node_color(n) == t->grey ) {
    return 0;
  } else {
    tm_list_append(t->top, n);
    tm_list_set_color(n, t->ecru);
    t->top = n;
    return 1;
  }
}


int tm_tread_mutation(tm_tread *t, tm_node *n)
{
  if ( tm_node_color(n) == t->black ) {
    tm_list_append(t->scan, n);
    tm_list_set_color(n, t->ecru);
    t->scan = n;
    return 1;
  } else {
    return 0;
  }
}

int tm_tread_flipQ(tm_tread *t)
{
  return t->scan == t->top;
}

int tm_tread_flip(tm_tread *t)
{
  void *p;

  p = bottom;
  bottom = top;
  top = p;

  t->ecru = t->white;
  t->black = t->ecru;
}

#endif
