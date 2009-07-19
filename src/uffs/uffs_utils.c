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
 * \file uffs_utils.c
 * \brief utilities of uffs
 * \author Ricky Zheng, created 12th May, 2005
 */
#include "uffs/uffs_device.h"
#include "uffs/uffs_utils.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_version.h"

#include <stdio.h>
#include <string.h>

#define PFX "util: "

#ifdef ENABLE_BAD_BLOCK_VERIFY
static void _ForceFormatAndCheckBlock(uffs_Device *dev, int block)
{
	u8 *pageBuf;
	int pageSize;
	int i, j;

	pageSize = dev->attr->page_data_size + dev->attr->spare_size;
	pageBuf = dev->mem.one_page_buffer;

	if (pageBuf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Alloc page buffer fail ! Format stoped.\n");
		return;
	}

	//step 1: Erase, fully fill with 0x0, and check
	dev->ops->EraseBlock(dev, block);
	memset(pageBuf, 0, pageSize);
	for (i = 0; i < dev->attr->pages_per_block; i++) {
		dev->ops->WritePage(dev, block, i, pageBuf, (u8 *)pageBuf + dev->attr->page_data_size);
	}
	for (i = 0; i < dev->attr->pages_per_block; i++) {
		dev->ops->ReadPage(dev, block, i, pageBuf, (u8 *)pageBuf + dev->attr->page_data_size);
		for (j = 0; j < pageSize; j++) {
			if(pageBuf[j] != 0)
				goto bad_out;
		}
	}

	//step 2: Erase, and check
	dev->ops->EraseBlock(dev, block);
	for (i = 0; i < dev->attr->pages_per_block; i++) {
		dev->ops->ReadPage(dev, block, i, pageBuf, (u8 *)pageBuf + dev->attr->page_data_size);
		for (j = 0; j < pageSize; j++) {
			if(pageBuf[j] != 0xff) 
				goto bad_out;
		}
	}

	return;

bad_out:
	dev->ops->EraseBlock(dev, block);
	dev->flash->MakeBadBlockMark(dev, block);
	return;
}
#endif



URET uffs_FormatDevice(uffs_Device *dev)
{
	u16 i, slot;
	
	if (dev == NULL)
		return U_FAIL;

	if (dev->ops == NULL || dev->flash == NULL) 
		return U_FAIL;


	if (uffs_BufIsAllFree(dev) == U_FALSE) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"some page still in used!\n");
		return U_FAIL;
	}

	for (slot = 0; slot < MAX_DIRTY_BUF_GROUPS; slot++) {
		if (dev->buf.dirtyGroup[slot].count > 0) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"there still have dirty pages!\n");
			return U_FAIL;
		}
	}

	uffs_BufSetAllEmpty(dev);


	if (uffs_IsAllBlockInfoFree(dev) == U_FALSE) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"there still have block info cache ? fail to format\n");
		return U_FAIL;
	}

	uffs_ExpireAllBlockInfo(dev);

	for (i = dev->par.start; i <= dev->par.end; i++) {
		if (dev->ops->IsBlockBad(dev, i) == U_FALSE) {
			dev->ops->EraseBlock(dev, i);
		}
		else {
#ifdef ENABLE_BAD_BLOCK_VERIFY
			_ForceFormatAndCheckBlock(dev, i);
#endif
		}
	}

	if (uffs_ReleaseTreeBuf(dev) == U_FAIL) {
		return U_FAIL;
	}

	if (uffs_InitTreeBuf(dev) == U_FAIL) {
		return U_FAIL;
	}

	if (uffs_BuildTree(dev) == U_FAIL) {
		return U_FAIL;
	}
	
	return U_SUCC;
}

