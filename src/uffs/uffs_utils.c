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

#define PFX "utils:"

static void _ForceFormatAndCheckBlock(uffs_Device *dev, int block)
{
	u8 *pageBuf;
	int pageSize;
	int i, j;

	pageSize = dev->attr->page_data_size + dev->attr->spare_size;
	if (dev->mem.one_page_buffer_size == 0) {
		if (dev->mem.malloc) {
			dev->mem.one_page_buffer = dev->mem.malloc(dev, pageSize);
			if (dev->mem.one_page_buffer) dev->mem.one_page_buffer_size = pageSize;
		}
	}
	if (pageSize > dev->mem.one_page_buffer_size) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Require one page buf %d but only %d available. Format stoped.\n", pageSize, dev->mem.one_page_buffer_size);
		return;
	}
	pageBuf = dev->mem.one_page_buffer;

	if (pageBuf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Alloc page buffer fail ! Format stoped.\n");
		return;
	}

	//step 1: Erase, fully fill with 0x0, and check
	dev->ops->EraseBlock(dev, block);
	memset(pageBuf, 0, pageSize);
	for(i = 0; i < dev->attr->pages_per_block; i++) {
		dev->ops->WritePage(dev, block, i, pageBuf, (u8 *)pageBuf + dev->attr->page_data_size);
	}
	for(i = 0; i < dev->attr->pages_per_block; i++) {
		dev->ops->ReadPage(dev, block, i, pageBuf, (u8 *)pageBuf + dev->attr->page_data_size);
		for(j = 0; j < pageSize; j++) {
			if(pageBuf[j] != 0) goto bad_out;
		}
	}

	//step 2: Erase, and check
	dev->ops->EraseBlock(dev, block);
	for(i = 0; i < dev->attr->pages_per_block; i++) {
		dev->ops->ReadPage(dev, block, i, pageBuf, (u8 *)pageBuf + dev->attr->page_data_size);
		for(j = 0; j < pageSize; j++) {
			if(pageBuf[j] != 0xff) goto bad_out;
		}
	}

	return;

bad_out:
	dev->ops->EraseBlock(dev, block);
	dev->flash->MakeBadBlockMark(dev, block);
	if (dev->mem.free) {
		dev->mem.free(dev, dev->mem.one_page_buffer);
		dev->mem.one_page_buffer_size = 0;
	}

	return;
}


URET uffs_FormatDevice(uffs_Device *dev)
{
	u16 i;
	
	if(dev == NULL) return U_FAIL;
	if(dev->ops == NULL || dev->flash == NULL) return U_FAIL;


	if(uffs_BufIsAllFree(dev) == U_FALSE) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"some page still in used!\n");
		return U_FAIL;
	}

	if(dev->buf.dirtyCount > 0) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"there still have dirty pages!\n");
		return U_FAIL;
	}

	uffs_BufSetAllEmpty(dev);


	if(uffs_IsAllBlockInfoFree(dev) == U_FALSE) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"there still have block info cache ? fail to format\n");
		return U_FAIL;
	}

	uffs_ExpireAllBlockInfo(dev);

	for(i = dev->par.start; i <= dev->par.end; i++) {
		if(dev->ops->IsBlockBad(dev, i) == U_FALSE) {
			dev->ops->EraseBlock(dev, i);
		}
		else {
			_ForceFormatAndCheckBlock(dev, i);
		}
	}

	if(uffs_ReleaseTreeBuf(dev) == U_FAIL) {
		return U_FAIL;
	}

	if(uffs_InitTreeBuf(dev) == U_FAIL) {
		return U_FAIL;
	}

	if(uffs_BuildTree(dev) == U_FAIL) {
		return U_FAIL;
	}
	
	return U_SUCC;
}

