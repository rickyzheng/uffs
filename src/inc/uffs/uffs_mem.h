/*
  This file is part of UFFS, the Ultra-low-cost Flash File System.
  
  Copyright (C) 2005-2009 Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

  UFFS is free software; you can redistribute it and/or modify it under
  the GNU Library General Public License as published by the Free Software 
  Foundation; either version 2 of the License, or (at your option) any
  later version.

  UFFS is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  or GNU Library General Public License, as applicable, for more details.
 
  You should have received a copy of the GNU General Public License
  and GNU Library General Public License along with UFFS; if not, write
  to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301, USA.

  As a special exception, if other files instantiate templates or use
  macros or inline functions from this file, or you compile this file
  and link it with other works to produce a work based on this file,
  this file does not by itself cause the resulting work to be covered
  by the GNU General Public License. However the source code for this
  file must still be made available in accordance with section (3) of
  the GNU General Public License v2.
 
  This exception does not invalidate any other reasons why a work based
  on this file might be covered by the GNU General Public License.
*/

#ifndef UFFS_MEM_H
#define UFFS_MEM_H

#include "uffs/uffs_device.h"

#ifdef __cplusplus
extern "C"{
#endif

#if defined(USE_NATIVE_MEMORY_ALLOCATOR)

#define HEAP_HASH_BIT	6							/* hash table bit */
#define HEAP_HASH_SIZE (1 << (HEAP_HASH_BIT - 1))	/* hash table size */
#define HEAP_HASH_MASK	(HEAP_HASH_SIZE - 1)		/* hash table mask */
#define GET_HASH_INDEX(p) ((((u32)(p)) >> 2) & HEAP_HASH_MASK)

/* memory alloc node  */
typedef struct HeapManagementNodeSt{
	int task_id;					/* who alloc this block? it's the caller's task id */
	struct HeapManagementNodeSt * next;	/* point to next node */
	void *p;						/* point to allocated block */
	int size;						/* block size */
} HeapMm;

typedef HeapMm* HeapHashTable;

/** \note: uffs_InitHeapMemory should be called before using native memory allocator on each device */
void uffs_InitHeapMemory(void *addr, int size);

URET uffs_initNativeMemAllocator(uffs_Device *dev);
int uffs_releaseNativeMemAllocator(uffs_Device *dev);

#endif //USE_NATIVE_MEMORY_ALLOCATOR


/** uffs native memory allocator */
typedef struct uffs_memAllocatorSt {
	URET (*init)(struct uffs_DeviceSt *dev);			/* init memory allocator, setup buffer sizes */
	URET (*release)(struct uffs_DeviceSt *dev);			/* release memory allocator (for dynamic memory allocation) */
	
	void * (*malloc)(struct uffs_DeviceSt *dev, unsigned int size); /* allocate memory (for dynamic memory allocation) */
	URET (*free)(struct uffs_DeviceSt *dev, void *p);   /* free memory (for dynamic memory allocation) */

	void * blockinfo_buffer;
	void * page_buffer;
	void * tree_buffer;
	void * one_page_buffer;

	int blockinfo_buffer_size;
	int page_buffer_size;
	int tree_buffer_size;
	int one_page_buffer_size;

#if defined(USE_NATIVE_MEMORY_ALLOCATOR)
	HeapHashTable tbl[HEAP_HASH_SIZE];
	int count;
	int maxused;
#endif

#if defined(USE_STATIC_MEMORY_ALLOCATOR)
	char *buf_start;
	int buf_size;
	int pos;
#endif

} uffs_MemAllocator;

#if defined(USE_NATIVE_MEMORY_ALLOCATOR)
void uffs_SetupNativeMemoryAllocator(uffs_MemAllocator *allocator);
#endif

#ifdef __cplusplus
}
#endif


#endif

