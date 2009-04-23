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

struct ubufm{
	void *node_pool;		/* data pool */
	unsigned int node_size; /* data struct size */
	unsigned int node_nums;	/* ubuf(data) num */
	void *freeList;			/* free list, used by ubuf internal */
	int lock;				/* buffer lock */
};

/*********************************************************************************************
*	global referenced variables
*********************************************************************************************/

/*********************************************************************************************
*	global function prototype
*********************************************************************************************/

/** init ubuffer data structure with given discriptor */
int uBufInit(struct ubufm *dis); 

/** release descriptor, delete lock */
int uBufRelease(struct ubufm *dis);

/** get buffer from pool */
void * uBufGet(struct ubufm *dis);

/**
 * get buffer from pool
 * this is Critical protect version,
 * you should use this version when multitherad 
 * access the same buffer pool
 */
void * uBufGetCt(struct ubufm *dis);

/** put buffer to pool */
int uBufPut(void *p, struct ubufm *dis);

/**
 * put buffer to pool, 
 * this is critical protect version,
 * you should use this version when multithread
 * access the same buffer pool
 */
int uBufPutCt(void *p, struct ubufm *dis);

/** get buffer pointer by index(offset) */
void * uBufGetBufByIndex(unsigned int idx, struct ubufm *dis);

/** get index by pointer */
int uBufGetIndex(void *p, struct ubufm *dis);


#ifdef __cplusplus
}
#endif

#endif

