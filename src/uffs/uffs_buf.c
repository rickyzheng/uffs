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
 * \file uffs_buf.c
 * \brief uffs page buffers manipulations
 * \author Ricky Zheng
 * \note Created in 11th May, 2005
 */

#include "uffs/uffs_types.h"
#include "uffs/uffs_buf.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include "uffs/ubuffer.h"
#include "uffs/uffs_ecc.h"
#include "uffs/uffs_badblock.h"
#include <string.h>

#define PFX "pbuf: "

/**
 * \brief inspect (print) uffs page buffers.
 * \param[in] dev uffs device to be inspected.
 */
void uffs_BufInspect(uffs_Device *dev)
{
	struct uffs_pageBufsSt *pb = &dev->buf;
	uffs_Buf *buf;

	uffs_Perror(UFFS_ERR_NORMAL, "------------- page buffer inspect ---------\n");
	uffs_Perror(UFFS_ERR_NORMAL, "all buffers: \n");
	for (buf = pb->bufHead; buf; buf = buf->next) {
		if (buf->mark != 0) {
			uffs_Perror(UFFS_ERR_NORMAL, "\tF:%04x S:%04x P:%02d R:%02d D:%03d M:%d\n", 
				buf->father, buf->serial, buf->pageID, buf->refCount, buf->dataLen, buf->mark);
		}
	}
	uffs_Perror(UFFS_ERR_NORMAL, "--------------------------------------------\n");
}

/**
 * \brief initialize page buffers for device
 * in UFFS, each device has one buffer pool
 * \param[in] dev uffs device
 * \param[in] maxBuf maximum buffer number, normally use #MAX_PAGE_BUFFERS
 * \param[in] maxDirtyBuf maximum dirty buffer allowed, if the dirty buffer over this number,
 *            than need to be flush to flash
 */
URET uffs_BufInit(uffs_Device *dev, int maxBuf, int maxDirtyBuf)
{
	void *pool;
	u8 *data;
	uffs_Buf *buf;
	int size;
	int i, slot;

	if(!dev) return U_FAIL;

	//init device common parameters, which are needed by page buffers
	dev->com.pgSize = dev->attr->page_data_size;
	dev->com.eccSize = dev->flash->GetEccSize(dev);
	dev->com.pgDataSize = dev->com.pgSize - dev->com.eccSize;

	if(dev->buf.pool != NULL) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"buf.pool is not NULL, buf already inited ?\n");
		return U_FAIL;
	}
	
	size = (sizeof(uffs_Buf) + dev->com.pgSize) * maxBuf;
	if (dev->mem.page_buffer_size == 0) {
		if (dev->mem.malloc) {
			dev->mem.page_buffer = dev->mem.malloc(dev, size);
			if (dev->mem.page_buffer) dev->mem.page_buffer_size = size;
		}
	}
	if (size > dev->mem.page_buffer_size) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"page buffers require %d but only %d available.\n", size, dev->mem.page_buffer_size);
		return U_FAIL;
	}
	pool = dev->mem.page_buffer;

	uffs_Perror(UFFS_ERR_NOISY, PFX"alloc %d bytes.\n", size);
	dev->buf.pool = pool;

	data = (u8 *)pool + (sizeof(uffs_Buf) * maxBuf);

	for(i = 0; i < maxBuf; i++) {
		buf = (uffs_Buf *)((u8 *)pool + (sizeof(uffs_Buf) * i));
		memset(buf, 0, sizeof(uffs_Buf));
		data = (u8 *)pool + (sizeof(uffs_Buf) * maxBuf) + (dev->com.pgSize * i);

		buf->data = data;
		buf->ecc = data + dev->com.pgDataSize;
		buf->mark = UFFS_BUF_EMPTY;
		if(i == 0) {
			buf->prev = NULL;
			dev->buf.bufHead = buf;
		}
		else {
			buf->prev = (uffs_Buf *)((u8 *)buf - sizeof(uffs_Buf));
		}

		if(i == maxBuf - 1) {
			buf->next = NULL;
			dev->buf.bufTail = buf;
		}
		else {
			buf->next = (uffs_Buf *)((u8 *)buf + sizeof(uffs_Buf));
		}
	}

	dev->buf.maxBuf = maxBuf;
	dev->buf.maxDirtyBuf = (maxDirtyBuf > dev->attr->pages_per_block ? dev->attr->pages_per_block : maxDirtyBuf);
	for (slot = 0; slot < MAX_DIRTY_BUF_GROUPS; slot++) {
		dev->buf.dirtyGroup[slot].dirty = NULL;
		dev->buf.dirtyGroup[slot].dirtyCount = 0;
	}
	return U_SUCC;
}

/**
 * \brief flush all buffers
 */
URET uffs_BufFlushAll(struct uffs_DeviceSt *dev)
{
	int slot;
	for (slot = 0; slot < MAX_DIRTY_BUF_GROUPS; slot++) {
		if(_BufFlush(dev, FALSE, slot) != U_SUCC) {
			uffs_Perror(UFFS_ERR_NORMAL, PFX"fail to flush buffer(slot %d)\n", slot);
			return U_FAIL;
		}
	}
	return U_SUCC;
}

/** 
 * \brief release all page buffer, this function should be called 
			when unmounting a uffs device
 * \param[in] dev uffs device
 * \note if there are page buffers in used, it may cause fail to release
 */
URET uffs_BufReleaseAll(uffs_Device *dev)
{
	uffs_Buf *p;

	if(!dev) return U_FAIL;

	//now release all buffer
	p = dev->buf.bufHead;
	while(p) {
		if(p->refCount != 0) {
			uffs_Perror(UFFS_ERR_NORMAL, 
				PFX "can't release buffers, \
					father:%d, serial:%d, pageID:%d still in used.\n", p->father, p->serial, p->pageID);
			return U_FAIL;
		}
		p = p->next;
	}

	if(uffs_BufFlushAll(dev) != U_SUCC) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"can't release buf, fail to flush buffer\n");
		return U_FAIL;
	}

	if (dev->mem.free) dev->mem.free(dev, dev->buf.pool);
	dev->buf.pool = NULL;
	dev->buf.bufHead = dev->buf.bufTail = NULL;

	return U_SUCC;
}


static void _BreakFromBufList(uffs_Device *dev, uffs_Buf *buf)
{
	if(buf->next) {
		buf->next->prev = buf->prev;
	}
	if(buf->prev) {
		buf->prev->next = buf->next;
	}
	if(dev->buf.bufHead == buf) {
		dev->buf.bufHead = buf->next;
	}
	if(dev->buf.bufTail == buf) {
		dev->buf.bufTail = buf->prev;
	}
}

static void _LinkToBufListHead(uffs_Device *dev, uffs_Buf *buf)
{
	if(buf == dev->buf.bufHead) return;
	buf->prev = NULL;
	buf->next = dev->buf.bufHead;
	if(dev->buf.bufHead) dev->buf.bufHead->prev = buf;
	if(dev->buf.bufTail == NULL) dev->buf.bufTail = buf;
	dev->buf.bufHead = buf;
}

static void _LinkToBufListTail(uffs_Device *dev, uffs_Buf *buf)
{
	if(dev->buf.bufTail == buf) return;
	buf->prev = dev->buf.bufTail;
	buf->next = NULL;
	if(dev->buf.bufTail) dev->buf.bufTail->next = buf;
	if(dev->buf.bufHead == NULL) dev->buf.bufHead = buf;
	dev->buf.bufTail = buf;
}

//move a node which linked in the list to the head of list
static void _MoveNodeToHead(uffs_Device *dev, uffs_Buf *p)
{
	if(p == dev->buf.bufHead) return;

	//break from list
	_BreakFromBufList(dev, p);

	//link to head
	_LinkToBufListHead(dev, p);
}

static void _LinkToDirtyList(uffs_Device *dev, int slot, uffs_Buf *buf)
{
	if (buf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Try to insert a NULL node into dirty list ?\n");
		return;
	}

	buf->mark = UFFS_BUF_DIRTY;
	buf->prevDirty = NULL;
	buf->nextDirty = dev->buf.dirtyGroup[slot].dirty;
	if(dev->buf.dirtyGroup[slot].dirty) dev->buf.dirtyGroup[slot].dirty->prevDirty = buf;
	dev->buf.dirtyGroup[slot].dirty = buf;
	dev->buf.dirtyGroup[slot].dirtyCount++;
}

static uffs_Buf * _FindFreeBuf(uffs_Device *dev)
{
	uffs_Buf *buf;
#if 1
	buf = dev->buf.bufHead;
	while(buf) {
		if(buf->refCount == 0 && 
			buf->mark != UFFS_BUF_DIRTY) return buf;
		buf = buf->next;
	}
#else
	buf = dev->buf.bufTail;
	while(buf) {
		if(buf->refCount == 0 &&
			buf->mark != UFFS_BUF_DIRTY) return buf;
		buf = buf->prev;
	}
#endif

	return buf;
}

/** 
 * load psychical page data into buf and do ecc check 
 * \param[in] dev uffs device
 * \param[in] buf buf to be load in
 * \param[in] block psychical block number
 * \param[in] page psychical page number
 * \return return U_SUCC if no error, return U_FAIL if I/O error or ecc check fail
 */
URET uffs_LoadPhiDataToBuf(uffs_Device *dev, uffs_Buf *buf, u32 block, u32 page)
{
	URET ret;

	ret = dev->ops->ReadPageData(dev, block, page, buf->data, 0, dev->com.pgSize);
	if(ret == U_SUCC) {
		if (uffs_CheckBadBlock(dev, buf, block) == U_SUCC) {
			buf->mark = UFFS_BUF_VALID; // the data is valid now
		}
		else {
			buf->mark = UFFS_BUF_EMPTY;
			ret = U_FAIL;
		}
	}
	else {
		buf->mark = UFFS_BUF_EMPTY; // the data is not valid
	}
	
	return ret;
}

/** 
 * load psychical page data into buf and try ecc check 
 * \param[in] dev uffs device
 * \param[in] buf buf to be load in
 * \param[in] block psychical block number
 * \param[in] page psychical page number
 * \return return U_SUCC if no error, return U_FAIL if I/O error
 * \note this function should be only used when doing bad block recover.
 */
URET uffs_LoadPhiDataToBufEccUnCare(uffs_Device *dev, uffs_Buf *buf, u32 block, u32 page)
{
	URET ret;

	ret = dev->ops->ReadPageData(dev, block, page, buf->data, 0, dev->com.pgSize);
	if(ret == U_SUCC) {
		if (uffs_CheckBadBlock(dev, buf, block) == U_SUCC) {
			//ECC check fail, but we return 'successful' anyway !!
			buf->mark = UFFS_BUF_VALID;
		}
		return U_SUCC;
	}
	else {
		buf->mark = UFFS_BUF_EMPTY; // the data is not valid, I/O error ?
	}
	
	return ret;
}


/** 
 * find a buffer in the pool
 * \param[in] dev uffs device
 * \param[in] father father serial num
 * \param[in] serial serial num
 * \param[in] pageID pageID
 * \return return found buffer, return NULL if buffer not found
 */
uffs_Buf * uffs_BufFind(uffs_Device *dev, u16 father, u16 serial, u16 pageID)
{
	uffs_Buf *p = dev->buf.bufHead;

	while(p) {
		if(	p->father == father &&
			p->serial == serial &&
			p->pageID == pageID &&
			p->mark != UFFS_BUF_EMPTY) {
			//they have match one
			return p;
		}
		p = p->next;
	}

	return NULL; //buffer not found
}

static uffs_Buf * _FindBufInDirtyList(uffs_Buf *dirty, u16 pageID)
{
	while(dirty) {
		if(dirty->pageID == pageID) return dirty;
		dirty = dirty->nextDirty;
	}
	return NULL;
}

static URET _BreakFromDirty(uffs_Device *dev, uffs_Buf *dirtyBuf)
{
	int slot = -1;

	if(dirtyBuf->mark != UFFS_BUF_DIRTY) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"try to break a non-dirty buf from dirty list ?\n");
		return U_FAIL;
	}

	slot = uffs_BufFindGroupSlot(dev, dirtyBuf->serial);
	if(slot < 0) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"no dirty list exit ?\n");
		return U_FAIL;
	}

	// break from the link
	if(dirtyBuf->nextDirty) {
		dirtyBuf->nextDirty->prevDirty = dirtyBuf->prevDirty;
	}

	if(dirtyBuf->prevDirty) {
		dirtyBuf->prevDirty->nextDirty = dirtyBuf->nextDirty;
	}

	// check if it's the link head ...
	if(dev->buf.dirtyGroup[slot].dirty == dirtyBuf) {
		dev->buf.dirtyGroup[slot].dirty = dirtyBuf->nextDirty;
	}

	dirtyBuf->nextDirty = dirtyBuf->prevDirty = NULL; // clear dirty link

	dev->buf.dirtyGroup[slot].dirtyCount--;

	return U_SUCC;
}

static u16 _GetDataSum(uffs_Device *dev, uffs_Buf *buf)
{
	u16 dataSum = 0; //default: 0
	uffs_fileInfo *fi;
	
	dev = dev;
	//FIXME: We use the same schema for both dir and file.
	if(buf->type == UFFS_TYPE_FILE || buf->type == UFFS_TYPE_DIR) {
		if(buf->pageID == 0) {
			fi = (uffs_fileInfo *)(buf->data);
			dataSum = uffs_MakeSum16(fi->name, fi->name_len);
		}
	}

	return dataSum;
}


static URET _CheckDirtyList(uffs_Buf *dirty)
{
	u16 father;
	u16 serial;

	if(dirty == NULL) {
		return U_SUCC;
	}

	father = dirty->father;
	serial = dirty->serial;
	dirty = dirty->nextDirty;

	while(dirty) {
		if(father != dirty->father ||
			serial != dirty->serial) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"father or serial in dirty pages buffer are not the same ?\n");
			return U_FAIL;
		}
		if(dirty->mark != UFFS_BUF_DIRTY) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"non-dirty page buffer in dirty buffer list ?\n");
			return U_FAIL;
		}
		dirty = dirty->nextDirty;
	}
	return U_SUCC;
}

/** find a page in dirty list, which has minimum pageID */
uffs_Buf * _FindMinimunPageIdFromDirtyList(uffs_Buf *dirtyList)
{
	uffs_Buf * work = dirtyList;
	uffs_Buf * buf = dirtyList;

	work = work->nextDirty;
	while(work) {
		if(work->pageID < buf->pageID) buf = work;
		work = work->nextDirty;
	}
	return buf;
}

/** 
 * \brief flush buffer to a block with enough free pages 
 *  
 *  pages in dirty list must be sorted by pageID to write to flash
 */
static
URET
 _BufFlush_Exist_With_Enough_FreePage(
		uffs_Device *dev,
		int slot,			//!< dirty group slot
		uffs_blockInfo *bc, //!< block info (Source, also destination)
		u16 freePages		//!< free pages number on destination block
		)		
{
	u16 page;
	uffs_Buf *buf;
	uffs_Tags *tag;
	URET ret;

//	uffs_Perror(UFFS_ERR_NOISY, PFX"Flush buffers with Enough Free Page, in block %d\n",
//							bc->blockNum);

	for(page = dev->attr->pages_per_block - freePages;	//page: free page num
			dev->buf.dirtyGroup[slot].dirtyCount > 0;						//still has dirty pages?
			page++) {

		buf = _FindMinimunPageIdFromDirtyList(dev->buf.dirtyGroup[slot].dirty);
		if(buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"dirtyCount > 0, but no dirty pages in list ?\n");
			return U_FAIL;
		}

		//now a dirty page which has pageID(id) been found
		//write to page i
		uffs_LoadBlockInfo(dev, bc, page);
		tag = &(bc->spares[page].tag);
		tag->blockTimeStamp = uffs_GetBlockTimeStamp(dev, bc);
		tag->dataLength = buf->dataLen;
		tag->type = buf->type;
		tag->dataSum = _GetDataSum(dev, buf);
		tag->father = buf->father;
		tag->serial = buf->serial;
		tag->pageID = (u8)(buf->pageID);  //FIX ME!! if page more than 256 in a block
		ret = uffs_WriteDataToNewPage(dev, bc->blockNum, page, tag, buf);
		if(ret == U_SUCC) {
			if(_BreakFromDirty(dev, buf) == U_SUCC) {
				buf->mark = UFFS_BUF_VALID;
				_MoveNodeToHead(dev, buf);
			}
		}
		else{
			uffs_Perror(UFFS_ERR_NORMAL, PFX"I/O error <1>?\n");
			return U_FAIL;
		}
	} //end of for
	
	if(dev->buf.dirtyGroup[slot].dirty != NULL || dev->buf.dirtyGroup[slot].dirtyCount != 0) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"still has dirty buffer ?\n");
	}
	return U_SUCC;
}


/** 
 * \brief flush buffer to a new block which is not registered in tree
 *
 * Scenario:
 *		1. get a new block
 *		2. write pages in dirty list to new block, sorted by pageID
 *		3. insert new block to tree
 */
static URET _BufFlush_NewBlock(uffs_Device *dev, int slot)
{
	u8 type;
	u16 father, serial;
	uffs_Buf *dirty, *buf;
	TreeNode *node;
	u16 i;
	uffs_blockInfo *bc;
	uffs_Tags *tag;
	URET ret;

	dirty = dev->buf.dirtyGroup[slot].dirty;
	type = dirty->type;
	father = dirty->father;		//all pages in dirty list have the same type, father and serial
	serial = dirty->serial;

	node = uffs_GetErased(dev);
	if(node == NULL) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"no erased block!\n");
		return U_FAIL;
	}
	bc = uffs_GetBlockInfo(dev, node->u.list.block);
	if(bc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"get block info fail!\n");
		uffs_InsertToErasedListHead(dev, node); //put node back to erased list
		return U_FAIL;
	}

//	uffs_Perror(UFFS_ERR_NOISY, PFX"Flush buffers with NewBlock, block %d\n",
//					bc->blockNum);

	for(i = 0; i < dev->attr->pages_per_block; i++) {
		buf = _FindBufInDirtyList(dirty, i);
		if(buf == NULL) break;
		uffs_LoadBlockInfo(dev, bc, i);
		tag = &(bc->spares[i].tag);
		tag->blockTimeStamp = uffs_GetFirstBlockTimeStamp();
		tag->dataLength = buf->dataLen;
		tag->dataSum = _GetDataSum(dev, buf);
		tag->type = type;
		tag->father = buf->father;
		tag->serial = buf->serial;
		tag->pageID = (u8)(buf->pageID);	//FIX ME!! if page more than 256 in a block
		ret = uffs_WriteDataToNewPage(dev, node->u.list.block, i, tag, buf);
		if(ret == U_SUCC) {
			if(_BreakFromDirty(dev, buf) == U_SUCC) {
				buf->mark = UFFS_BUF_VALID;
				_MoveNodeToHead(dev, buf);
			}
		}
		else{
			uffs_Perror(UFFS_ERR_NORMAL, PFX"I/O error <2>?\n");
			uffs_PutBlockInfo(dev, bc);
			return U_FAIL;
		}
	}

	//now, all pages write successful, fix the tree...
	switch(type) {
	case UFFS_TYPE_DIR:
		node->u.dir.block = bc->blockNum;
		node->u.dir.father = father;
		node->u.dir.serial = serial;
		//node->u.dir.pagID = 0;		//dir stored in page 0,  ??
		//node->u.dir.ofs = 0;		//TODO!!, for dir, the ofs should be ... ?
		node->u.dir.checkSum = bc->spares[0].tag.dataSum; //for dir, the checkSum should be the same as file
								//FIXME: if we support more than one dir in one block
		break;
	case UFFS_TYPE_FILE:
		node->u.file.block = bc->blockNum;
		node->u.file.father = father;
		node->u.file.serial = serial;
		node->u.file.checkSum = bc->spares[0].tag.dataSum; //for file, the page0 is where fileinfo ...
		break;
	case UFFS_TYPE_DATA:
		node->u.data.block = bc->blockNum;
		node->u.data.father = father;
		node->u.data.serial = serial;
		break;
	default:
		uffs_Perror(UFFS_ERR_NOISY, PFX"Unknown type %d\n", type);
		break;
	}

	uffs_InsertNodeToTree(dev, type, node);
	uffs_PutBlockInfo(dev, bc);

	return U_SUCC;
}

/** 
 * \brief flush buffer with block recover
 *
 * Scenario: 
 *		1. get a free (erased) block --> newNode <br>
 *		2. copy from old block ---> oldNode, or copy from dirty list, <br>
 *			sorted by pageID, to new block. Skips the invalid pages when copy pages.<br>
 *		3. erased old block. set new info to oldNode, set newNode->block = old block,<br>
 *			and put newNode to erased list.<br>
 *	\note IT'S IMPORTANT TO KEEP OLD NODE IN THE LIST, so you don't need to update the obj->node :-)
 */
static URET _BufFlush_Exist_With_BlockCover(
			uffs_Device *dev,
			int slot,			//!< dirty group slot
			TreeNode *node,		//!< old data node on tree
			uffs_blockInfo *bc	//!< old data block info
			)
{
	u16 i;
	u8 type, timeStamp;
	u16 page, father, serial;
	uffs_Buf *buf;
	TreeNode *newNode;
	uffs_blockInfo *newBc;
	uffs_Tags *tag, *oldTag;
	URET ret = U_SUCC;
	u16 newBlock;
	UBOOL succRecover = U_TRUE; //TRUE: recover successful, erase old block,
								//FALSE: fail to recover, erase new block

	type = dev->buf.dirtyGroup[slot].dirty->type;
	father = dev->buf.dirtyGroup[slot].dirty->father;
	serial = dev->buf.dirtyGroup[slot].dirty->serial;

	newNode = uffs_GetErased(dev);
	if(newNode == NULL) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"no enough erased block!\n");
		return U_FAIL;
	}
	newBlock = newNode->u.list.block;
	newBc = uffs_GetBlockInfo(dev, newBlock);
	if(newBc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"get block info fail!\n");
		uffs_InsertToErasedListHead(dev, newNode);  //put node back to erased list
													//because it doesn't use, so put to head
		return U_FAIL;
	}

	uffs_LoadBlockInfo(dev, newBc, UFFS_ALL_PAGES);
	timeStamp = uffs_GetNextBlockTimeStamp(uffs_GetBlockTimeStamp(dev, bc));

//	uffs_Perror(UFFS_ERR_NOISY, PFX"Flush buffers with Block Recover, from %d to %d\n", 
//					bc->blockNum, newBc->blockNum);

	for(i = 0; i < dev->attr->pages_per_block; i++) {
		tag = &(newBc->spares[i].tag);
		tag->blockTimeStamp = timeStamp;
		tag->father = father;
		tag->serial = serial;
		tag->type = type;
		tag->pageID = (u8)i; //now, pageID = page, FIX ME!! if more than 256 pages in a block
		
		buf = _FindBufInDirtyList(dev->buf.dirtyGroup[slot].dirty, i);
		if(buf != NULL) {
			tag->dataLength = buf->dataLen;
			tag->dataSum = _GetDataSum(dev, buf);
			ret = uffs_WriteDataToNewPage(dev, newBlock, i, tag, buf);
			if(ret == U_SUCC) {
				if(_BreakFromDirty(dev, buf) == U_SUCC) {
					buf->mark = UFFS_BUF_VALID;
					_MoveNodeToHead(dev, buf);
				}
			}
			else{
				uffs_Perror(UFFS_ERR_NORMAL, PFX"I/O error <3>?\n");
				succRecover = U_FALSE;
				break;
			}
		}
		else {
			
			page = uffs_FindPageInBlockWithPageId(dev, bc, i);
			if(page == UFFS_INVALID_PAGE) {
				uffs_ExpireBlockInfo(dev, newBc, i);
				break;  //end of last page, normal break
			}
			page = uffs_FindBestPageInBlock(dev, bc, page);
			
			oldTag = &(bc->spares[page].tag);
			buf = uffs_BufClone(dev, NULL);
			if(buf == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't clone a new buf!\n");
				succRecover = U_FALSE;
				break;
			}
			ret = uffs_LoadPhiDataToBuf(dev, buf, bc->blockNum, page);
			if(ret == U_FAIL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"I/O error ?\n");
				uffs_BufFreeClone(dev, buf);
				break;
			}
			buf->dataLen = oldTag->dataLength;
			if(buf->dataLen > dev->com.pgDataSize) {
				uffs_Perror(UFFS_ERR_NOISY, PFX"data length over flow!!!\n");
				buf->dataLen = dev->com.pgDataSize;
			}

			buf->type = type;
			buf->father = father;
			buf->serial = serial;
			buf->dataLen = oldTag->dataLength;
			buf->pageID = oldTag->pageID; 

			tag->dataLength = buf->dataLen;
			tag->dataSum = _GetDataSum(dev, buf);

			ret = uffs_WriteDataToNewPage(dev, newBlock, i, tag, buf);
			uffs_BufFreeClone(dev, buf);
			if(ret != U_SUCC) {
				uffs_Perror(UFFS_ERR_NORMAL, PFX"I/O error <4>?\n");
				succRecover = U_FALSE;
				break;
			}
		}

	} //end of for

	if(succRecover == U_TRUE) {
		switch(type) {
		case UFFS_TYPE_DIR:
			node->u.dir.block = newBlock;
			node->u.dir.checkSum = newBc->spares[0].tag.dataSum;
			//node->u.dir.ofs = 0; //TODO!! fix me!
			//node->u.dir.pagID = 0; //TODO!! fix me!
			break;
		case UFFS_TYPE_FILE:
			node->u.file.block = newBlock;
			node->u.file.checkSum = newBc->spares[0].tag.dataSum;
			break;
		case UFFS_TYPE_DATA:
			node->u.data.block = newBlock;
			break;
		default:
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"UNKNOW TYPE\n");
			break;
		}
		newNode->u.list.block = bc->blockNum;
		uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);

		if (newNode->u.list.block == dev->bad.block) {
			// the recovered block is a BAD block, we need to deal with it immediately.
			uffs_ProcessBadBlock(dev, newNode);
		}
		else {
			// erase recovered block, put it back to erased block list.
			dev->ops->EraseBlock(dev, bc->blockNum);
			uffs_InsertToErasedListTail(dev, newNode);
		}
	}
	else {
		uffs_ExpireBlockInfo(dev, newBc, UFFS_ALL_PAGES);
		dev->ops->EraseBlock(dev, newBlock);
		newNode->u.list.block = newBlock;
		uffs_InsertToErasedListTail(dev, newNode);
	}

	if(dev->buf.dirtyGroup[slot].dirty != NULL || dev->buf.dirtyGroup[slot].dirtyCount != 0) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"still has dirty buffer ?\n");
	}

	uffs_PutBlockInfo(dev, newBc);

	return U_SUCC;
}

URET _BufFlush(struct uffs_DeviceSt *dev, UBOOL force_block_recover, int slot)
{
	uffs_Buf *dirty;
	TreeNode *node;
	uffs_blockInfo *bc;
	u16 n;
	URET ret;
	u8 type;
	u16 father;
	u16 serial;
	int block;
	
	if(dev->buf.dirtyGroup[slot].dirtyCount == 0) {
		return U_SUCC;
	}

	dirty = dev->buf.dirtyGroup[slot].dirty;

	if(_CheckDirtyList(dirty) == U_FAIL) return U_FAIL;

	type = dirty->type;
	father = dirty->father;
	serial = dirty->serial;

	switch(type) {
	case UFFS_TYPE_DIR:
		node = uffs_FindDirNodeFromTree(dev, serial);
		break;
	case UFFS_TYPE_FILE:
		node = uffs_FindFileNodeFromTree(dev, serial);
		break;
	case UFFS_TYPE_DATA:
		node = uffs_FindDataNode(dev, father, serial);
		break;
	default:
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"unknown type\n");
		return U_FAIL;
	}

	if(node == NULL) {
		//not found in the tree, need to generate a new block
		ret = _BufFlush_NewBlock(dev, slot);
	}
	else {
		switch(type) {
		case UFFS_TYPE_DIR:
			block = node->u.dir.block;
			break;
		case UFFS_TYPE_FILE:
			block = node->u.file.block;
			break;
		case UFFS_TYPE_DATA:
			block = node->u.data.block;
			break;
		default:
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"unknown type.\n");
			return U_FAIL;
		}
		bc = uffs_GetBlockInfo(dev, block);
		if(bc == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"get block info fail.\n");
			return U_FAIL;
		}
		uffs_LoadBlockInfo(dev, bc, UFFS_ALL_PAGES);
		n = uffs_GetFreePagesCount(dev, bc);

		if(n >= dev->buf.dirtyGroup[slot].dirtyCount && !force_block_recover) {
			//The free pages are enough for the dirty pages
			ret = _BufFlush_Exist_With_Enough_FreePage(dev, slot, bc, n);
		}
		else {
			ret = _BufFlush_Exist_With_BlockCover(dev, slot, node, bc);
		}
		uffs_PutBlockInfo(dev, bc);
	}

	return ret;
}

static int _FindMostDirtyGroup(struct uffs_DeviceSt *dev)
{
	int i, slot = -1;
	int max_count = 0;

	for (i = 0; i < MAX_DIRTY_BUF_GROUPS; i++) {
		if (dev->buf.dirtyGroup[i].dirty) {
			if (dev->buf.dirtyGroup[i].dirtyCount > max_count) {
				max_count = dev->buf.dirtyGroup[i].dirtyCount;
				slot = i;
			}
		}
	}

	return slot;
}

/** 
 * flush buffers to flash.
 * this will pick up a most dirty group, and flush it if there is no free dirty group slot.
 * \param[in] dev uffs device
 */
URET uffs_BufFlush(struct uffs_DeviceSt *dev)
{
	int slot;
	slot = uffs_BufFindFreeGroupSlot(dev);
	if (slot >= 0) {
		return U_SUCC;  //there is free slot, do nothing.
	}
	else {
		slot = _FindMostDirtyGroup(dev);
		return _BufFlush(dev, FALSE, slot);
	}
}

/** 
 * flush buffers to flash
 * this will pick up a most dirty group, and flush it if there is no free dirty group slot.
 * \param[in] dev uffs device
 * \param[in] force_block_recover #U_TRUE: force a block recover even there are enough free pages
 */
URET uffs_BufFlushEx(struct uffs_DeviceSt *dev, UBOOL force_block_recover)
{
	int slot;
	slot = uffs_BufFindFreeGroupSlot(dev);
	if (slot >= 0) {
		return U_SUCC;  //there is free slot, do nothing.
	}
	else {
		slot = _FindMostDirtyGroup(dev);
		return _BufFlush(dev, force_block_recover, slot);
	}
}

/**
 * flush buffer group with given serial num.
 * this will find the dirty group with given serial num, flush it to flash.
 *
 * \param[in] dev uffs device
 * \param[in] serial serial num of the group
 */
URET uffs_BufFlushGroup(struct uffs_DeviceSt *dev, int serial)
{
	/* TODO: buffer group */
	return U_SUCC;
}

/**
 * flush buffer group with given serial num and force_block_recover indicator.
 * this will find the dirty group with given serial num, flush it to flash.
 *
 * \param[in] dev uffs device
 * \param[in] serial serial num of group
 * \param[in] force_block_recover indicator
 */
URET uffs_BufFlushGroupEx(struct uffs_DeviceSt *dev, int serial, UBOOL force_block_recover)
{
	/* TODO: buffer group */

	return U_SUCC;
}

/**
 * find a free dirty group slot
 *
 * \param[in] dev uffs device
 * \return slot index (0 to MAX_DIRTY_BUF_GROUPS - 1) if found one, otherwise return -1.
 */
int uffs_BufFindFreeGroupSlot(struct uffs_DeviceSt *dev)
{
	int i, slot = -1;
	for (i = 0; i < MAX_DIRTY_BUF_GROUPS; i++) {
		if (dev->buf.dirtyGroup[i].dirty == NULL) {
			slot = i;
			break;
		}
	}
	return slot;
}

/**
 * find a dirty group slot with given serial num.
 *
 * \param[in] dev uffs device
 * \return slot index (0 to MAX_DIRTY_BUF_GROUPS - 1) if found one, otherwise return -1.
 */
int uffs_BufFindGroupSlot(struct uffs_DeviceSt *dev, int serial)
{
	uffs_Buf *buf;
	int i, slot = -1;
	for (i = 0; i < MAX_DIRTY_BUF_GROUPS; i++) {
		if (dev->buf.dirtyGroup[i].dirty) {
			buf = dev->buf.dirtyGroup[i].dirty;
			if (buf->serial == serial) {
				slot = i;
				break;
			}
		}
	}
	return slot;
}

/** 
 * \brief get a page buffer
 * \param[in] dev uffs device
 * \param[in] father father serial num
 * \param[in] serial serial num
 * \param[in] pageID pageID
 * \return return the buffer found in buffer list, if not found, return NULL.
 */
uffs_Buf * uffs_BufGet(struct uffs_DeviceSt *dev, u16 father, u16 serial, u16 pageID)
{
	uffs_Buf *p;

	//first, check whether the buffer exist in buf list ?
	p = uffs_BufFind(dev, father, serial, pageID);

	if(p) {
		p->refCount++;
		_MoveNodeToHead(dev, p);
	}

	return p;
}

/** 
 * New generate a buffer
 */
uffs_Buf *uffs_BufNew(struct uffs_DeviceSt *dev, u8 type, u16 father, u16 serial, u16 pageID)
{
	uffs_Buf *buf;

	buf = uffs_BufGet(dev, father, serial, pageID);
	if (buf) {
		if (buf->refCount > 1) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"When create new buf, an exist buffer has ref count %d, possibly bug!\n", buf->refCount);
		}
		else {
			buf->dataLen = 0;
		}
		_MoveNodeToHead(dev, buf);
		return buf;
	}

	buf = _FindFreeBuf(dev);
	if(buf == NULL) {
		uffs_BufFlush(dev);
		buf = _FindFreeBuf(dev);
		if (buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"no free page buf!\n");
			return NULL;
		}
	}

	buf->mark = UFFS_BUF_EMPTY;
	buf->type = type;
	buf->father = father;
	buf->serial = serial;
	buf->pageID = pageID;
	buf->dataLen = 0;
	buf->refCount++;
	memset(buf->data, 0xff, dev->com.pgSize);

	_MoveNodeToHead(dev, buf);
	
	return buf;	
}



/** 
 * get a page buffer
 * \param[in] dev uffs device
 * \param[in] type dir, file or data ?
 * \param[in] node node on the tree
 * \param[in] pageID pageID
 * \return return the buffer if found in buffer list, if not found in 
 *			buffer list, it will get a free buffer, and load data from flash.
 *			return NULL if not free buffer.
 */
uffs_Buf *uffs_BufGetEx(struct uffs_DeviceSt *dev, u8 type, TreeNode *node, u16 pageID)
{
	uffs_Buf *buf;
	u16 father, serial, block, page;
	uffs_blockInfo *bc;

	switch(type) {
	case UFFS_TYPE_DIR:
		father = node->u.dir.father;
		serial = node->u.dir.serial;
		block = node->u.dir.block;
		break;
	case UFFS_TYPE_FILE:
		father = node->u.file.father;
		serial = node->u.file.serial;
		block = node->u.file.block;
		break;
	case UFFS_TYPE_DATA:
		father = node->u.data.father;
		serial = node->u.data.serial;
		block = node->u.data.block;
		break;
	default:
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"unknown type");
		return NULL;
	}

	buf = uffs_BufFind(dev, father, serial, pageID);
	if(buf) {
		buf->refCount++;
		return buf;
	}

	buf = _FindFreeBuf(dev);
	if(buf == NULL) {
		uffs_BufFlush(dev);
		buf = _FindFreeBuf(dev);
		if (buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"no free page buf!\n");
			return NULL;
		}
	}

	bc = uffs_GetBlockInfo(dev, block);
	if (bc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't get block info!\n");
		return NULL;
	}
	
	page = uffs_FindPageInBlockWithPageId(dev, bc, pageID);
	if(page == UFFS_INVALID_PAGE) {
		uffs_PutBlockInfo(dev, bc);
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't find right page ?\n");
		return NULL;
	}
	page = uffs_FindBestPageInBlock(dev, bc, page);
	uffs_PutBlockInfo(dev, bc);

	buf->mark = UFFS_BUF_EMPTY;
	buf->type = type;
	buf->father = father;
	buf->serial = serial;
	buf->pageID = pageID;

	if (uffs_LoadPhiDataToBuf(dev, buf, block, page) == U_FAIL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't load page from flash !\n");
		return NULL;
	}

	buf->dataLen = bc->spares[page].tag.dataLength;
	buf->mark = UFFS_BUF_VALID;
	buf->refCount++;

	_MoveNodeToHead(dev, buf);
	
	return buf;

}

/** 
 * \brief Put back a page buffer, make reference count decrease by one
 * \param[in] dev uffs device
 * \param[in] buf buffer to be put back
 */
URET uffs_BufPut(uffs_Device *dev, uffs_Buf *buf)
{
	dev = dev;
	if(buf == NULL) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX "Can't put an NULL buffer!\n");
		return U_FAIL;
	}
	if(buf->refCount == 0) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX "Putting an unused page buffer ? \n");
		return U_FAIL;
	}

	buf->refCount--;
	
	return U_SUCC;
}


/** 
 * \brief clone from an exist buffer.
		allocate memory for new buffer, and copy data from original buffer if 
		original buffer is not NULL. 
 * \param[in] dev uffs device
 * \param[in] buf page buffer to be clone from. if NULL presented here, data copy will not be processed
 * \return return the cloned page buffer, all data copied from source
 * \note the cloned buffer is not linked in page buffer list in uffs device,
 *			so you should use #uffs_BufFreeClone instead of #uffs_BufPut when you put back or release buffer
 */
uffs_Buf * uffs_BufClone(uffs_Device *dev, uffs_Buf *buf)
{
	uffs_Buf *p;

	p = _FindFreeBuf(dev);
	if(p == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"no enough free pages for clone!\n");
		return NULL;
	}
	_BreakFromBufList(dev, p);

	if(buf) {
		p->father = buf->father;
		p->type = buf->type;
		p->serial = buf->serial;
		p->pageID = buf->pageID;
		
		p->dataLen = buf->dataLen;
		//athough the valid data length is .dataLen,
		//but we still need copy the whole buffer, include ecc
		memcpy(p->data, buf->data, dev->com.pgSize);
	}
	p->next = p->prev = NULL;			//because the cloned one is not linked to device buffer
	p->nextDirty = p->prevDirty = NULL;
	p->refCount = CLONE_BUF_MARK;		//CLONE_BUF_MARK indicates that this is an cloned buffer
	return p;
}

/** 
 * \brief release cloned buffer
 * \param[in] dev uffs device
 * \param[in] buf cloned buffer
 */
void uffs_BufFreeClone(uffs_Device *dev, uffs_Buf *buf)
{
	dev = dev; //make compiler happy
	if(!buf) return;

	if(buf->refCount != CLONE_BUF_MARK) {
		/* a cloned buffer must have a refCount of CLONE_BUF_MARK */
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "Try to release a non-cloned page buffer ?\n");
		return;
	}
	buf->refCount = 0;
	buf->mark = UFFS_BUF_EMPTY;
	_LinkToBufListTail(dev, buf);
}



UBOOL uffs_BufIsAllFree(struct uffs_DeviceSt *dev)
{
	uffs_Buf *buf = dev->buf.bufHead;
	while(buf) {
		if(buf->refCount != 0) return U_FALSE;
		buf = buf->next;
	}
	return U_TRUE;
}

UBOOL uffs_BufIsAllEmpty(struct uffs_DeviceSt *dev)
{
	uffs_Buf *buf = dev->buf.bufHead;
	while(buf) {
		if(buf->mark != UFFS_BUF_EMPTY) return U_FALSE;
		buf = buf->next;
	}
	return U_TRUE;
}


URET uffs_BufSetAllEmpty(struct uffs_DeviceSt *dev)
{
	uffs_Buf *buf = dev->buf.bufHead;
	while(buf) {
		buf->mark = UFFS_BUF_EMPTY;
		buf = buf->next;
	}
	return U_SUCC;
}


void uffs_BufIncRef(uffs_Buf *buf)
{
	buf->refCount++;
}

void uffs_BufDecRef(uffs_Buf *buf)
{
	if(buf->refCount > 0) buf->refCount--;
}

void uffs_BufSetMark(uffs_Buf *buf, int mark)
{
	buf->mark = mark;
}

#if 0
static UBOOL _IsBufInDirtyList(struct uffs_DeviceSt *dev, uffs_Buf *buf)
{
	uffs_Buf *p = dev->buf.dirtyGroup[slot].dirty;
	while(p) {
		if(p == buf) return U_TRUE;
		p = p->nextDirty;
	}
	return U_FALSE;
}
#endif

URET uffs_BufWrite(struct uffs_DeviceSt *dev, uffs_Buf *buf, void *data, u32 ofs, u32 len)
{
	uffs_Buf *dirty;
	UBOOL inDirtyList = U_FALSE;
	int slot;

	if(ofs + len > dev->com.pgDataSize) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"data length out of range! %d+%d\n", ofs, len);
		return U_FAIL;
	}

	slot = uffs_BufFindGroupSlot(dev, buf->serial);

	if (slot < 0) {
		// need to take a free slot
		slot = uffs_BufFindFreeGroupSlot(dev);
		if (slot < 0) {
			// no free slot ? flush buffer
			uffs_BufFlush(dev);
			slot = uffs_BufFindFreeGroupSlot(dev);
			if (slot < 0) {
				// still no free slot ??
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"no free slot ?\n");
				return U_FAIL;
			}
		}
	}

	memcpy(buf->data + ofs, data, len);
	if(ofs + len > buf->dataLen) buf->dataLen = ofs + len;
	
	if(inDirtyList == U_FALSE) {
		_LinkToDirtyList(dev, slot, buf);
	}

	if(dev->buf.dirtyGroup[slot].dirtyCount >= dev->buf.maxDirtyBuf) {
		if(uffs_BufFlushGroup(dev, buf->serial) != U_SUCC) {
			return U_FAIL;
		}
	}

	return U_SUCC;
}

URET uffs_BufRead(struct uffs_DeviceSt *dev, uffs_Buf *buf, void *data, u32 ofs, u32 len)
{
	u32 readSize;
	u32 pgDataSize = dev->com.pgDataSize;

	readSize = (ofs >= pgDataSize ? 0 :	(ofs + len >= pgDataSize ? pgDataSize - ofs : len));
	if(readSize > 0) memcpy(data, buf->data + ofs, readSize);

	return U_SUCC;
}







