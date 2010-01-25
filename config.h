/** \file config.h
 * \brief Configuration.
 *
 * $Id: internal.h,v 1.17 2009-08-01 10:47:31 stephens Exp $
 */
#ifndef _tredmill_CONFIG_H
#define _tredmill_CONFIG_H


/****************************************************************************/
/*! \defgroup configuration Configuration */
/*@{*/

#ifndef PAGESIZE

/*! The mininum size memory block in the OS. */
#define PAGESIZE (1 << 13) /* 8192 */
#endif

#ifndef tm_PAGESIZE
/*! The size of blocks allocated from the OS. */
#define tm_PAGESIZE PAGESIZE
#endif

#ifndef tm_TIME_STAT
#define tm_TIME_STAT 1 /*!< If true, enable timing stats. */
#endif

#ifndef tm_name_GUARD
#define tm_name_GUARD 0 /*!< If true, enable name guards in internal structures. */
#endif

#ifndef tm_block_GUARD
#define tm_block_GUARD 0 /*!< If true, enable data corruption guards in internal structures. */
#endif

#ifndef tm_GC_THRESHOLD
#define tm_GC_THRESHOLD 3 / 4
#endif

/*! Size of tm_node headers. */
#define tm_node_HDR_SIZE sizeof(struct tm_node)

/*! Size of tm_block headers. */
#define tm_block_HDR_SIZE sizeof(struct tm_block)

/*! The maxinum size tm_node that can be allocated from a single tm_block. */
#define tm_block_SIZE_MAX  (tm_block_SIZE - tm_block_HDR_SIZE)


/**
 * Configuration constants.
 */
enum tm_config {
  /*! Nothing smaller than this is actually allocated. */
  tm_ALLOC_ALIGN = 8, 

  /*! Alignment of pointers. */
  tm_PTR_ALIGN = __alignof(void*),

  /*! Size of tm_block. Allocations from operating system are aligned to this size. */
  tm_block_SIZE = tm_PAGESIZE,

  /*! Operating system pages are aligned to this size. */
  tm_page_SIZE = tm_PAGESIZE,

  /*! The addressable range of process memory in 1024 blocks. */
  tm_address_range_k = 1UL << (sizeof(void*) * 8 - 10),

  /*! Huh? */
  tm_block_N_MAX = sizeof(void*) / tm_block_SIZE,
};


/*! Mask to align tm_block pointers. */ 
#define tm_block_SIZE_MASK ~(((unsigned long) tm_block_SIZE) - 1)

/*! Mask to align page pointers. */ 
#define tm_page_SIZE_MASK ~(((unsigned long) tm_page_SIZE) - 1)

/*@}*/

#endif
