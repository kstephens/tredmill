/** \file node.h
 * \brief Node.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_NODE_H
#define _tredmill_NODE_H

#include "tredmill/list.h"

/****************************************************************************/
/*! \defgroup node Node */
/*@{*/


/**
 * An allocation node representing the data ptr returned from tm_alloc().
 *
 * The data ptr return by tm_alloc() from a tm_node is immediately after its tm_list header.
 *
 * tm_nodes are parceled from tm_blocks.
 * Each tm_node has a color, which places it on
 * a colored list for the tm_node's tm_type.
 *
 * A tm_node's tm_block is determined by the tm_node's address.
 *
 * A tm_node's tm_type is determined by the tm_block it resides in.
 *
 */
typedef struct tm_node {
  /*! The current type list for the node. */
  /*! tm_type.color_list[tm_node_color(this)] list. */
  tm_list list; 
} tm_node;

/*! The color of a tm_node. */
#define tm_node_color(n) ((tm_color) tm_list_color(n))

/*! A pointer to the data of a tm_node. */
#define tm_node_ptr(n) ((void*)(((tm_node*) n) + 1))

/*! Return the tm_node's tm_type. */
#define tm_node_type(n) tm_block_type(tm_node_to_block(n))

/**
 * An allocation for a large node.
 */
typedef struct tm_node_large {
  /*! The current type list for the node. */
  /*! tm_type.color_list[tm_node_color(this)] list. */
  tm_list list;

  /*! The large buffer for the data of this node. */
  void *ptr;
} tm_node_large;


/**
 * Colored Node Iterator.
 */
typedef struct tm_node_iterator {
  /*! The color of nodes to iterate on. */
  int color;
  /*! The next tm_node. */
  tm_node *node_next;
  /*! The current tm_type. */
  struct tm_type *type;
  /*! The current tm_node. */
  tm_node *node;

  /*! The node scheduled for interior scanning. */
  void *scan_node;
  /*! The current scanning position in the tm_node_ptr() data region. */
  void *scan_ptr;
  /*! End of scan region. */
  void *scan_end;
  /*! Size of scan region. */
  size_t scan_size;
} tm_node_iterator;


/*@}*/

#endif
