#ifndef tm_MARK_H
#define tm_MARK_H


/***************************************************************************/
/* Marking */

static __inline
int _tm_node_mark(tm_node *n)
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


static __inline 
tm_node * _tm_mark_possible_ptr(void *p)
{
  tm_node *n = tm_ptr_to_node(p);
  if ( n && _tm_node_mark(n) )
    return n;
    
  return 0;
}

void _tm_root_loop_init();

void _tm_register_mark();

void _tm_set_stack_ptr(void *stackvar);
void _tm_stack_mark();

void _tm_root_mark_all();
int _tm_root_mark_some();

void _tm_range_mark(const void *b, const void *e);

size_t _tm_node_mark_some(long left);
void _tm_node_mark_all();

#endif
