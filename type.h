/** \file type.h
 * \brief Type.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_TYPE_H
#define _tredmill_TYPE_H

#include "tredmill/list.h"
#include "tredmill/tread.h"


/****************************************************************************/
/*! \defgroup type Type */
/*@{*/

/**
 * A tm_type represents information about all tm_nodes of a specific size.
 *
 * -# How many tm_nodes of a given color exists.
 * -# Lists of tm_nodes by color.
 * -# Lists of tm_blocks used to parcel tm_nodes.
 * -# The tm_block for initializing new nodes from.
 * -# A tm_adesc user-level descriptor.
 * .
 */
typedef struct tm_type {
  /*! All types list: tm.types */
  tm_list list;

#if tm_name_GUARD
  /*! A name for debugging. */
  const char *name;
#endif

  /*! The type id: tm.type_id */
  int id;

  /*! Hash table next ptr: tm.type_hash[]. */
  struct tm_type *hash_next;  

  /*! Size of each tm_node. */
  size_t size;

  /*! List of blocks allocated for this type. */                
  tm_list blocks;     

  /*! Number of nodes, indexed by tm_color: includes tm_TOTAL, tm_B, tm_NU, tm_b, tm_b_NU stats. */        
  size_t *n; /* == &tread.n */

  /*! The tread for this type. */
  tm_tread tread;

#if 0
  /*! Lists of node by color; see tm_node.list. */
  tm_list color_list[tm_TOTAL];
#endif

  /*! The current block we are parceling from. */ 
  struct tm_block *parcel_from_block;

  /*! User-specified descriptor handle. */ 
  struct tm_adesc *desc;
} tm_type;


#define tm_type_tread(t) (&(t)->tread)

void tm_type_init(tm_type *t, size_t size);
tm_type *tm_type_new(size_t size);
struct tm_adesc *tm_adesc_for_size(struct tm_adesc *desc, int force_new);
tm_type *tm_size_to_type(size_t size);

void *tm_type_alloc_node_from_free_list(tm_type *t);

struct tm_block * _tm_type_alloc_block(tm_type *t);
void _tm_type_add_block(tm_type *t, struct tm_block *b);
void _tm_type_remove_block(tm_type *t, struct tm_block *b);

int tm_type_parcel_or_alloc_node(tm_type *t);
int tm_type_parcel_some_nodes(tm_type *t, long left);


/*@}*/

#include "tredmill/tread.h"
#include "tredmill/block.h"

#endif
