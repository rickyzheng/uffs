/*
    Copyright (C) 2005-2008  Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/
/** 
 * \file uffs_config.h
 * \brief basic configuration of uffs
 * \author Ricky Zheng
 */

#ifndef _UFFS_CONFIG_H_
#define _UFFS_CONFIG_H_

/**
 * \def MAX_CACHED_BLOCK_INFO
 * \note uffs cache the block info for opened directories and files,
 *       a practical value is 5 ~ MAX_OBJECT_HANDLE
 */
#define MAX_CACHED_BLOCK_INFO	10

/** 
 * \def MAX_PAGE_BUFFERS
 * \note the bigger value will bring better read/write performance.
 *       but few writing performance will be improved when this 
 *       value is become larger than 'max pages per block'
 */
#define MAX_PAGE_BUFFERS		33

/**
 * \def MAX_DIRTY_PAGES_IN_A_BLOCK 
 * \note this value should be between '2' and 'max pages per block'.
 *       the smaller the value the frequently the buffer will be flushed.
 */
#define MAX_DIRTY_PAGES_IN_A_BLOCK 32


/** \def MAX_PATH_LENGTH */
#define MAX_PATH_LENGTH 128

/**
 * \def USE_STATIC_MEMORY_ALLOCATOR
 * \note uffs will use static memory allocator if this is defined.
 *       to use static memory allocator, you need to provide memory
 *       buffer when creating uffs_Device.
 *
 *       use UFFS_MAIN_BUFFER_SIZE() to calculate memory buffer size.
 */
#define USE_STATIC_MEMORY_ALLOCATOR

/**
 * \def USE_NATIVE_MEMORY_ALLOCATOR
 * \note  the native memory allocator should only be used for
 *        tracking memory leak bugs or tracking memory consuming.
 *        In your final product, you either disable the native memory
 *        allocator or use the system heap as the memory pool for the
 *        native memory allocator.
 */
#define USE_NATIVE_MEMORY_ALLOCATOR

/** 
 * \def FLUSH_BUF_AFTER_WRITE
 * \note UFFS will write all data directly into flash in 
 *       each 'write' call if you enable this option.
 *       (which means lesser data lost when power failue but lower writing performance)
 *       we recomment not open this define for normal applications.
 */
//#define FLUSH_BUF_AFTER_WRITE

/**
 * \def TREE_NODE_USE_DOUBLE_LINK
 * \note: enable double link tree node will speed up insert/delete operation,
 */
#define TREE_NODE_USE_DOUBLE_LINK

/** 
 * \def MAX_OBJECT_HANDLE
 * maximum number of object handle 
 */
#define MAX_OBJECT_HANDLE	10

/**
 * \def MINIMUN_ERASED_BLOCK
 *  UFFS will not allow appending or creating new files when the free/erased block
 *  is lower then MINIMUN_ERASED_BLOCK.
 */
#define MINIMUN_ERASED_BLOCK 2

/**
 * \def CHANGE_MODIFY_TIME
 * If defined, closing a file which is opened for writing/appending will
 * update the file's modify time as well. Disable this feature will save a
 * lot of writing activities if you frequently open files for write and close it.
 */
#define CHANGE_MODIFY_TIME

/**
 * \def ENABLE_TAG_CHECKSUM 
 */
#define ENABLE_TAG_CHECKSUM 0		// do not set it to 1 if your NAND flash only have 8 bytes spare !


/* micros for calculating buffer sizes */
#define UFFS_BLOCK_INFO_BUFFER_SIZE(n_pages_per_block) ((sizeof(uffs_blockInfo) + sizeof(uffs_pageSpare) * n_pages_per_block) * MAX_CACHED_BLOCK_INFO)
#define UFFS_PAGE_BUFFER_SIZE(n_page_size) ((sizeof(uffs_Buf) + n_page_size) * MAX_PAGE_BUFFERS)
#define UFFS_TREE_BUFFER_SIZE(n_blocks) (sizeof(TreeNode) * n_blocks)

#define UFFS_STATIC_BUFF_SIZE(n_pages_per_block, n_page_size, n_blocks) (UFFS_BLOCK_INFO_BUFFER_SIZE(n_pages_per_block) + UFFS_PAGE_BUFFER_SIZE(n_page_size) + UFFS_TREE_BUFFER_SIZE(n_blocks) + n_page_size)

#endif
