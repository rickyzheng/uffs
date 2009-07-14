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
 * \file uffs_badblock.c
 * \brief bad block checking and recovering
 * \author Ricky Zheng, created in 13th Jun, 2005
 */

#include "uffs/uffs_fs.h"
#include "uffs/uffs_config.h"
#include "uffs/uffs_ecc.h"
#include "uffs/uffs_badblock.h"
#include <string.h>

#define PFX "bbl:  "

void uffs_InitBadBlock(uffs_Device *dev)
{
	dev->bad.block = UFFS_INVALID_BLOCK;
}

/** 
 * \brief check ECC in buf, if the data is corrupt, try ECC correction.
 * \param[in] dev uffs device
 * \param[in] buf data in buf
 * \param[in] block num of this buf
 * \return return U_SUCC if data is valid or collected by ECC successful,
 *         return U_FAIL if ECC fail.
 * \note if a bad block is found, it will then be add to dev->bad, so that we can
 *       deal with it later by calling #uffs_RecoverBadBlock.
 */
URET uffs_CheckBadBlock(uffs_Device *dev, uffs_Buf *buf, int block)
{
	u8 ecc[MAX_ECC_LENGTH];
	int ret;

	dev->flash->MakeEcc(dev, buf->data, ecc);
	ret = dev->flash->EccCollect(dev, buf->data, buf->ecc, ecc);
	if (ret > 0) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"bad block(%d) found but corrected by ecc!\n", block);

		if (dev->bad.block == block) {
			uffs_Perror(UFFS_ERR_NORMAL, PFX"The bad block %d has been reported before.\n", block);
			return U_SUCC;
		}

		if (dev->bad.block != UFFS_INVALID_BLOCK) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"uffs can't handle more than one bad block!\n");
			return U_FAIL;
		}
		/**
		 * mark this block as a new discovered bad block,
		 * so that we can deal with this bad block later.(by calling #uffs_RecoverBadBlock)
		 */
		dev->bad.block = block;
		return U_SUCC;
	}
	else if (ret < 0) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"bad block(%d) found and can't be corrected by ecc!\n", block);
		if (dev->bad.block == block) {
			uffs_Perror(UFFS_ERR_NORMAL, PFX"The bad block %d has been reported before.\n", block);
			return U_FAIL;
		}

		if (dev->bad.block != UFFS_INVALID_BLOCK) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"uffs can't handle more than one bad block!\n");
			return U_FAIL;
		}
		/**
		 * Don't know how can we do, but set treat it as a new discovered bad block anyway.
		 */
		dev->bad.block = block;
		return U_FAIL;
	}
	
	//no error found
	return U_SUCC;
}

/** 
 * \brief process bad block: erase bad block, mark it as 'bad' and put the node to bad block list.
 * \param[in] dev uffs device
 * \param[in] node bad block tree node (before the block turn 'bad', it must belong to something ...)
 */
void uffs_ProcessBadBlock(uffs_Device *dev, TreeNode *node)
{
	if (HAVE_BADBLOCK(dev)) {
		// erase the bad block
		dev->ops->EraseBlock(dev, dev->bad.block);
		dev->flash->MakeBadBlockMark(dev, dev->bad.block);

		// and put it into bad block list
		uffs_InsertToBadBlockList(dev, node);

		//clear bad block mark.
		dev->bad.block = UFFS_INVALID_BLOCK;
	}
}

/** 
 * \brief recover bad block
 * \param[in] dev uffs device
 */
void uffs_RecoverBadBlock(uffs_Device *dev)
{
	TreeNode *good, *bad;
	uffs_Buf *buf;
	u16 i;
	u16 page;
	uffs_BlockInfo *bc = NULL;
	uffs_Tags *tag;
	uffs_Tags newTag;
	UBOOL succRecov;
	UBOOL goodBlockIsDirty = U_FALSE;
	URET ret;
	int region;
	u8 type;
	
	if (dev->bad.block == UFFS_INVALID_BLOCK)
		return;

	// pick up a erased good block
	good = uffs_GetErased(dev);
	if (good == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"no free block to replace bad block!\n");
		return;
	}

	//recover block
	bc = uffs_GetBlockInfo(dev, dev->bad.block);
	
	if (bc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get bad block info\n");
		return;
	}

	succRecov = U_TRUE;
	for (i = 0; i < dev->attr->pages_per_block; i++) {
		page = uffs_FindPageInBlockWithPageId(dev, bc, i);
		if(page == UFFS_INVALID_PAGE) {
			break;  //end of last valid page, normal break
		}
		page = uffs_FindBestPageInBlock(dev, bc, page);
		tag = &(bc->spares[page].tag);
		buf = uffs_BufClone(dev, NULL);
		if (buf == NULL) {	
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't clone a new buf!\n");
			succRecov = U_FALSE;
			break;
		}
		//NOTE: since this is a bad block, we can't guarantee the data is ECC ok, so just load data even ECC is not OK.
		ret = uffs_LoadPhyDataToBufEccUnCare(dev, buf, bc->block, page);
		if (ret == U_FAIL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"I/O error ?\n");
			uffs_BufFreeClone(dev, buf);
			succRecov = U_FALSE;
			break;
		}
		buf->data_len = tag->data_len;
		if (buf->data_len > dev->com.pg_data_size) {
			uffs_Perror(UFFS_ERR_NOISY, PFX"data length over flow!!!\n");
			buf->data_len = dev->com.pg_data_size;
		}

		buf->father = tag->father;
		buf->serial = tag->serial;
		buf->type = tag->type;
		buf->page_id = tag->page_id;
		
		newTag = *tag;
		newTag.block_ts = uffs_GetNextBlockTimeStamp(tag->block_ts);
		ret = uffs_WriteDataToNewPage(dev, good->u.list.block, i, &newTag, buf);
		goodBlockIsDirty = U_TRUE;
		uffs_BufFreeClone(dev, buf);
		if (ret != U_SUCC) {
			uffs_Perror(UFFS_ERR_NORMAL, PFX"I/O error ?\n");
			succRecov = U_FALSE;
			break;
		}
	}


	if (succRecov == U_TRUE) {
		//successful recover bad block, so need to mark bad block, and replace with good one

		region = SEARCH_REGION_DIR|SEARCH_REGION_FILE|SEARCH_REGION_DATA;
		bad = uffs_FindNodeByBlock(dev, dev->bad.block, &region);
		if (bad != NULL) {
			switch (region) {
			case SEARCH_REGION_DIR:
				bad->u.dir.block = good->u.list.block;
				type = UFFS_TYPE_DIR;
				break;
			case SEARCH_REGION_FILE:
				bad->u.file.block = good->u.list.block;
				type = UFFS_TYPE_FILE;
				break;
			case SEARCH_REGION_DATA:
				bad->u.data.block = good->u.list.block;
				type = UFFS_TYPE_DATA;
			}
			
			//from now, the 'bad' is actually good block :)))
			uffs_Perror(UFFS_ERR_NOISY, PFX"new bad block %d found, and replaced by %d!\n", dev->bad.block, good->u.list.block);
			uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);
			//we reuse the 'good' node as bad block node, and process the bad block.
			good->u.list.block = dev->bad.block;
			uffs_ProcessBadBlock(dev, good);
		}
		else {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't find the reported bad block(%d) in the tree???\n", dev->bad.block);
			if (goodBlockIsDirty == U_TRUE)
				dev->ops->EraseBlock(dev, good->u.list.block);
			uffs_InsertToErasedListTail(dev, good);
		}
	}
	else {
		if (goodBlockIsDirty == U_TRUE)
			dev->ops->EraseBlock(dev, good->u.list.block);
		uffs_InsertToErasedListTail(dev, good); //put back to erased list
	}

	uffs_PutBlockInfo(dev, bc);

}
