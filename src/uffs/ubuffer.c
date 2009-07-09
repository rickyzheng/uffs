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
 * \file ubuffer.c
 * \brief Implement static, low cost, hight performance and fixed size buffer manager.
 *
 *	\author	Created by Ricky Zheng
 */
 
/******************************************************************************************
*	include files 
*******************************************************************************************/
#include "uffs/ubuffer.h"
#include "uffs/uffs_os.h"
#include <string.h>

/*

usage:

  #define NODE_SIZE	32
  #define NODE_NUMS	1024

  static int my_data_buf[NODE_SIZE * NODE_NUMS / sizeof(int)];
  static struct uffs_StaticBufSt dis;

  dis.node_pool = &my_data_buf;
  dis.node_size = NODE_SIZE;
  dis.node_nums = NODE_NUMS;

  uffs_StaticBufInit(&dis);

  void * p;
  p = uffs_StaticBufGet(&dis);
  ...
  uffs_StaticBufPut(p, &dis);

notice:
	NODE_SIZE's lowest 2 bit must be 0, and NODE_SIZE must be large then or equal to 4.
	NODE_NUM must be larger than 1.

*/

static int _InitLock(struct uffs_StaticBufSt *dis)
{
	if (!dis->lock) {
		dis->lock = uffs_SemCreate(1);
	}
	return 0;
}

static int _ReleaseLock(struct uffs_StaticBufSt *dis)
{
	if (dis->lock) {
		if (uffs_SemDelete(dis->lock) < 0) {
			return -1;
		}
		else {
			dis->lock = 0;
		}
	}	
	return 0;
}

static int _LockLock(struct uffs_StaticBufSt *dis)
{
	return uffs_SemWait(dis->lock);
}

static int _UnlockLock(struct uffs_StaticBufSt *dis)
{
	return uffs_SemSignal(dis->lock);
}

/* init ubuffer data structure with given descriptor */
int uffs_StaticBufInit(struct uffs_StaticBufSt *dis)
{
	unsigned int i;
	int *p1, *p2;

	if(dis == NULL) return -1;
	if(dis->node_nums < 1) return -1;

	_InitLock(dis);
	_LockLock(dis);
	//initialize free_list. 
	dis->free_list = dis->node_pool;
	p1 = p2 = (int *)(dis->node_pool);
	for (i = 1; i < dis->node_nums; i++) {
		p2 = (int *)((char *)(dis->node_pool) + i * dis->node_size);
		*p1 = (int)p2;
		p1 = p2;
	}
	*p2 = (int)NULL;	//end of free_list
	_UnlockLock(dis);

	return 0;
}

/** release descriptor, delete lock **/
int uffs_StaticBufRelease(struct uffs_StaticBufSt *dis)
{
	_ReleaseLock(dis);
	return 0;
}


/* get buffer from pool */
void * uffs_StaticBufGet(struct uffs_StaticBufSt *dis)
{
	void *p;

	p = dis->free_list;
	if (p) {
		dis->free_list = (void *)(*((int *)(dis->free_list)));
	}
	return p;
}

/* get buffer from pool
 * this is Critical protect version,
 * you should use this version when multi-therad 
 * access the same buffer pool
 */
void * uffs_StaticBufGetCt(struct uffs_StaticBufSt *dis)
{
	void *p;

	_LockLock(dis);
	p = dis->free_list;
	if (p) {
		dis->free_list = (void *)(*((int *)(dis->free_list)));
	}
	_UnlockLock(dis);

	return p;
}

/* put buffer to pool */
int uffs_StaticBufPut(void *p, struct uffs_StaticBufSt *dis)
{
	if (p) {
		*((int *)p) = (int)(dis->free_list);
		dis->free_list = p;
		return 0;
	}
	return -1;
}

/* put buffer to pool, 
 * this is critical protect version,
 * you should use this version when multithread
 * access the same buffer pool
 */
int uffs_StaticBufPutCt(void *p, struct uffs_StaticBufSt *dis)
{
	if (p) {
		_LockLock(dis);
		*((int *)p) = (int)(dis->free_list);
		dis->free_list = p;
		_UnlockLock(dis);
		return 0;
	}
	return -1;
}

/* get buffer pointer by index(offset) */
void * uffs_StaticBufGetByIndex(unsigned int idx, struct uffs_StaticBufSt *dis)
{
	return (char *)(dis->node_pool) + idx * dis->node_size;
}

/* get index by pointer */
int uffs_StaticBufGetIndex(void *p, struct uffs_StaticBufSt *dis)
{
	return ((char *)p - (char *)(dis->node_pool)) / dis->node_size;
}
