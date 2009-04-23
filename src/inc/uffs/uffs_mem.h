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
typedef struct _heap_mm_node{
	int task_id;					/* who alloc this block? it's the caller's task id */
	struct _heap_mm_node * next;	/* point to next node */
	void *p;						/* point to allocated block */
	int size;						/* block size */
} HEAP_MM;

typedef HEAP_MM* HASHTBL;

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
	HASHTBL tbl[HEAP_HASH_SIZE];
	int count;
	int maxused;
#endif

#if defined(USE_STATIC_MEMORY_ALLOCATOR)
	char *buf_start;
	int buf_size;
	int pos;
#endif

} uffs_memAllocator;

#if defined(USE_NATIVE_MEMORY_ALLOCATOR)
void uffs_SetupNativeMemoryAllocator(uffs_memAllocator *allocator);
#endif

#ifdef __cplusplus
}
#endif


#endif

