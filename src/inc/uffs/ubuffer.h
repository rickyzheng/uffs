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

/**
 *
 *	\file ubuffer.h
 *	\brief fast/static buffer management
 *	\author	Created by Ricky
 *
 */

#ifndef ___UBUFFER_H___
#define ___UBUFFER_H___


#ifdef __cplusplus
extern "C"{
#endif
/*********************************************************************************************
*	global referenced macro defines & type defines
*********************************************************************************************/

struct uffs_StaticBufSt {
	void *node_pool;		/* data pool */
	unsigned int node_size; /* data struct size */
	unsigned int node_nums;	/* ubuf(data) num */
	void *free_list;		/* free list, used by internal */
	int lock;				/* buffer lock */
};

/*********************************************************************************************
*	global referenced variables
*********************************************************************************************/

/*********************************************************************************************
*	global function prototype
*********************************************************************************************/

/** init ubuffer data structure with given discriptor */
int uffs_StaticBufInit(struct uffs_StaticBufSt *dis); 

/** release descriptor, delete lock */
int uffs_StaticBufRelease(struct uffs_StaticBufSt *dis);

/** get buffer from pool */
void * uffs_StaticBufGet(struct uffs_StaticBufSt *dis);

/**
 * get buffer from pool
 * this is Critical protect version,
 * you should use this version when multitherad 
 * access the same buffer pool
 */
void * uffs_StaticBufGetCt(struct uffs_StaticBufSt *dis);

/** put buffer to pool */
int uffs_StaticBufPut(void *p, struct uffs_StaticBufSt *dis);

/**
 * put buffer to pool, 
 * this is critical protect version,
 * you should use this version when multithread
 * access the same buffer pool
 */
int uffs_StaticBufPutCt(void *p, struct uffs_StaticBufSt *dis);

/** get buffer pointer by index(offset) */
void * uffs_StaticBufGetByIndex(unsigned int idx, struct uffs_StaticBufSt *dis);

/** get index by pointer */
int uffs_StaticBufGetIndex(void *p, struct uffs_StaticBufSt *dis);


#ifdef __cplusplus
}
#endif

#endif

