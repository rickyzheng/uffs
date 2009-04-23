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
 * \file uffs_blockInfo.c
 * \brief block information cache system manipulations
 * \author Ricky Zheng, created 10th May, 2005
 */

#include "uffs/uffs_blockInfo.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_os.h"

#include <string.h>

#define PFX "bc:"

#define UFFS_CLONE_BLOCK_INFO_NEXT ((uffs_blockInfo *)(-2))

/** \brief before block info cache is enable, this function should be called to initialize it
 * \param[in] dev uffs device
 * \param[in] maxCachedBlocks maximum cache buffers to be allocated
 * \return result of initialization
 *		\retval U_SUCC successful
 *		\retval U_FAIL failed
 */
URET uffs_InitBlockInfoCache(uffs_Device *dev, int maxCachedBlocks)
{
	uffs_blockInfo * blockInfos = NULL;
	uffs_pageSpare * pageSpares = NULL;
	void * buf = NULL;
	uffs_blockInfo *work = NULL;
	int size, i, j;

	if(dev->bc.head != NULL) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"block info cache has been inited already, now release it first.\n");
		uffs_ReleaseBlockInfoCache(dev);
	}

	size = ( 
			sizeof(uffs_blockInfo) +
			sizeof(uffs_pageSpare) * dev->attr->pages_per_block
			) * maxCachedBlocks;

	if (dev->mem.blockinfo_buffer_size == 0) {
		if (dev->mem.malloc) {
			dev->mem.blockinfo_buffer = dev->mem.malloc(dev, size);
			if (dev->mem.blockinfo_buffer) dev->mem.blockinfo_buffer_size = size;
		}
	}
	if (size > dev->mem.blockinfo_buffer_size) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"Block cache buffer require %d but only %d available.\n", size, dev->mem.blockinfo_buffer_size);
		return U_FAIL;
	}
	buf = dev->mem.blockinfo_buffer;

	memset(buf, 0, size);

	dev->bc.internalBufHead = buf;
	dev->bc.maxBlockCached = maxCachedBlocks;

	size = 0;
	blockInfos = (uffs_blockInfo *)buf;
	size += sizeof(uffs_blockInfo) * maxCachedBlocks;

	pageSpares = (uffs_pageSpare *)((int)buf + size);

	//initialize block info
	work = &(blockInfos[0]);
	dev->bc.head = work;
	work->refCount = 0;
	work->prev = NULL;
	work->next = &(blockInfos[1]);
	work->blockNum = UFFS_INVALID_BLOCK;

	for( i = 0; i < maxCachedBlocks - 2; i++) {
		work = &(blockInfos[i+1]);
		work->prev = &(blockInfos[i]);
		work->next = &(blockInfos[i+2]);
		work->refCount = 0;
		work->blockNum = UFFS_INVALID_BLOCK;
	}
	//the last node
	work = &(blockInfos[i+1]);
	work->prev = &(blockInfos[i]);
	work->next = NULL;
	work->blockNum = UFFS_INVALID_BLOCK;
	work->refCount = 0;
	dev->bc.tail = work;

	//initialize spares
	work = dev->bc.head;
	for( i = 0; i < maxCachedBlocks; i++) {
		work->spares = &(pageSpares[i*dev->attr->pages_per_block]);
		for(j = 0; j < dev->attr->pages_per_block; j++) {
			work->spares[j].expired = 1;
		}
		work->expiredCount = dev->attr->pages_per_block;
		work = work->next;
	}
	return U_SUCC;
}

/**
 * \brief release all allocated memory in initialize process, 
 *			this function should be called when unmount file system
 * \param[in] dev uffs device
 */
URET uffs_ReleaseBlockInfoCache(uffs_Device *dev)
{
	uffs_blockInfo *work;

	if(dev->bc.head) {
		for(work = dev->bc.head; work != NULL; work = work->next) {
			if(work->refCount != 0) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX "There have refed block info cache, release cache fail.\n");
				return U_FAIL;
			}
		}
		if (dev->mem.free) dev->mem.free(dev, dev->bc.internalBufHead);
	}

	dev->bc.head = dev->bc.tail = NULL;
	dev->bc.internalBufHead = NULL;
	return U_SUCC;
}

static void _BreakBcFromList(uffs_Device *dev, uffs_blockInfo *bc)
{
	if(bc->prev) {
		bc->prev->next = bc->next;
	}
	if(bc->next) {
		bc->next->prev = bc->prev;
	}
	if(dev->bc.head == bc) dev->bc.head = bc->next;
	if(dev->bc.tail == bc) dev->bc.tail = bc->prev;
}

static void _InsertToBcListTail(uffs_Device *dev, uffs_blockInfo *bc)
{
	bc->next = NULL;
	bc->prev = dev->bc.tail;
	bc->prev->next = bc;
	dev->bc.tail = bc;
}

static void _MoveBcToTail(uffs_Device *dev, uffs_blockInfo *bc)
{
	_BreakBcFromList(dev, bc);
	_InsertToBcListTail(dev, bc);
}

/**
 * \brief check spare data, set .checkOk value.
 * \param[in] dev uffs device
 * \param[in,out] spare page spare
 */
void uffs_CheckPageSpare(uffs_Device *dev, uffs_pageSpare *spare)
{
#if defined(ENABLE_TAG_CHECKSUM) && ENABLE_TAG_CHECKSUM == 1
	if(uffs_CalTagCheckSum(&(spare->tag)) == spare->tag.checkSum) {
		spare->checkOk = 1;
	}
	else {
		spare->checkOk = 0;
	}
#else
	dev = dev;
	spare->checkOk = 1;
#endif
}

/** 
 * \brief load page spare data to given block info structure with given page number
 * \param[in] dev uffs device
 * \param[in] work given block info to be filled with
 * \param[in] page given page number to be read from, if #UFFS_ALL_PAGES is presented, it will read
 *			all pages, otherwise it will read only one given page.
 * \return load result
 * \retval U_SUCC successful
 * \retval U_FAIL fail to load
 * \note work->blockNum must be set before load block info
 */
URET uffs_LoadBlockInfo(uffs_Device *dev, uffs_blockInfo *work, int page)
{
	int i;
	uffs_pageSpare *spare;

	if(page == UFFS_ALL_PAGES) {
		for(i = 0; i < dev->attr->pages_per_block; i++) {
			spare = &(work->spares[i]);
			if(spare->expired == 0) continue;
			
			if( dev->flash->LoadPageSpare(dev, work->blockNum, i, &(spare->tag)) == U_FAIL ) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX "load block %d page %d spare fail.", work->blockNum, i);
				return U_FAIL;
			}
			uffs_CheckPageSpare(dev, spare);
			spare->expired = 0;
			work->expiredCount--;
		}
	}
	else {
		if(page < 0 || page >= dev->attr->pages_per_block) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX "page out of range !\n");
			return U_FAIL;
		}
		spare = &(work->spares[page]);
		if(spare->expired != 0) {
			if( dev->flash->LoadPageSpare(dev, work->blockNum, page, &(spare->tag)) == U_FAIL ) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX "load block %d page %d spare fail.", work->blockNum, page);
				return U_FAIL;
			}
			uffs_CheckPageSpare(dev, spare);
			spare->expired = 0;
			work->expiredCount--;
		}
	}
	return U_SUCC;
}


/** 
 * \brief find a block cache with given block number
 * \param[in] dev uffs device
 * \param[in] block block number
 * \return found block cache
 * \retval NULL cache not found
 * \retval non-NULL found cache pointer
 */
uffs_blockInfo * uffs_FindBlockInfoInCache(uffs_Device *dev, int block)
{
	uffs_blockInfo *work;
	
	//search cached block
	for(work = dev->bc.head; work != NULL; work = work->next) {
		if(work->blockNum == block) {
			work->refCount++;
			return work;
		}
	}
	return NULL;
}


/** 
 * \brief Find a cached block in cache pool, if the cached block exist then return the pointer,
 *		if the block does not cached already, find a non-used cache. if all of cached are 
 *		used out, return NULL.
 * \param[in] dev uffs device
 * \param[in] block block number to be found
 * \return found block cache buffer
 * \retval NULL caches used out
 * \retval non-NULL buffer pointer of given block
 */
uffs_blockInfo * uffs_GetBlockInfo(uffs_Device *dev, int block)
{
	uffs_blockInfo *work;
	int i;

	//search cached block
	if((work = uffs_FindBlockInfoInCache(dev, block)) != NULL) {
		_MoveBcToTail(dev, work);
		return work;
	}

	//can't find block from cache, need to find a free(unlocked) cache
	for(work = dev->bc.head; work != NULL; work = work->next) {
		if(work->refCount == 0) break;
	}
	if(work == NULL) {
		//caches used out !
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "insufficient block info cache\n");
		return NULL;
	}

	work->blockNum = block;
	work->expiredCount = dev->attr->pages_per_block;
	for(i = 0; i < dev->attr->pages_per_block; i++) {
		work->spares[i].expired = 1;
	}

	work->refCount = 1;

	_MoveBcToTail(dev, work);

	return work;
}

/** 
 * \brief put block info buffer back to pool, should be called with #uffs_GetBlockInfo in pairs.
 * \param[in] dev uffs device
 * \param[in] p pointer of block info buffer
 */
void uffs_PutBlockInfo(uffs_Device *dev, uffs_blockInfo *p)
{
	dev = dev;
	if(p->refCount == 0) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "Put an unused block info cache back ?\n");
	}
	else {
		p->refCount--;
	}
}


/** 
 * \brief make the given pages expired in given block info buffer
 * \param[in] dev uffs device
 * \param[in] p pointer of block info buffer
 * \param[in] page given page number. if #UFFS_ALL_PAGES presented, all pages in the block should be made expired.
 */
void uffs_ExpireBlockInfo(uffs_Device *dev, uffs_blockInfo *p, int page)
{
	int i;
	uffs_pageSpare *spare;

	if(page == UFFS_ALL_PAGES) {
		for(i = 0; i < dev->attr->pages_per_block; i++) {
			spare = &(p->spares[i]);
			if(spare->expired == 0) {
				spare->expired = 1;
				p->expiredCount++;
			}
		}
	}
	else {
		if(page >= 0 && page < dev->attr->pages_per_block) {
			spare = &(p->spares[page]);
			if(spare->expired == 0) {
				spare->expired = 1;
				p->expiredCount++;
			}
		}
	}
}

/** 
 * Is all blcok info cache free (not referenced) ?
 */
UBOOL uffs_IsAllBlockInfoFree(uffs_Device *dev)
{
	uffs_blockInfo *work;

	work = dev->bc.head;
	while(work) {
		if(work->refCount != 0) return U_FALSE;
		work = work->next;
	}

	return U_TRUE;
}

void uffs_ExpireAllBlockInfo(uffs_Device *dev)
{
	uffs_blockInfo *bc;

	bc = dev->bc.head;
	while(bc) {
		uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);
		bc = bc->next;
	}
	return;
}
