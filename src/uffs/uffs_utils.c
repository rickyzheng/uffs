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
#include "uffs/uffs_config.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_utils.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_version.h"
#include "uffs/uffs_badblock.h"
#include "uffs/uffs_fd.h"

#include <stdio.h>
#include <string.h>

#define PFX "util: "

#define SPOOL(dev) &((dev)->mem.spare_pool)

#ifdef CONFIG_USE_GLOBAL_FS_LOCK
static int _global_lock = 0;

/* global file system lock */
void uffs_InitGlobalFsLock(void)
{
	_global_lock = uffs_SemCreate(1);
}

void uffs_ReleaseGlobalFsLock(void)
{
	uffs_SemDelete(_global_lock);
}

void uffs_GlobalFsLockLock(void)
{
	uffs_SemWait(_global_lock);
}

void uffs_GlobalFsLockUnlock(void)
{
	uffs_SemSignal(_global_lock);
}

#endif


#ifdef CONFIG_ENABLE_BAD_BLOCK_VERIFY
static void _ForceFormatAndCheckBlock(uffs_Device *dev, int block)
{
	int i, j;
	uffs_Buf *buf = NULL;
	UBOOL bad = U_TRUE;
	URET ret;
	struct uffs_FlashOpsSt *ops = dev->ops;
	struct uffs_TagStoreSt ts;
	u8 *spare = NULL;

	buf = uffs_BufClone(dev, NULL);
	if (buf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS,
					"Alloc page buffer fail ! Format stoped.");
		goto ext;
	}

	spare = (u8 *)uffs_PoolGet(SPOOL(dev));
	if (spare == NULL)
		goto ext;

	//step 1: Erase, fully fill with 0x0, and check
	ret = uffs_FlashEraseBlock(dev, block);
	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		goto bad_out;

	memset(buf->header, 0, dev->com.pg_size);
	memset(&ts, 0, sizeof(ts));
	memset(spare, 0, dev->attr->spare_size);

	for (i = 0; i < dev->attr->pages_per_block; i++) {
		if (ops->WritePageWithLayout)
			ret = ops->WritePageWithLayout(dev, block, i, buf->header, dev->com.pg_size, NULL, &ts);
		else
			ret = ops->WritePage(dev, block, i, buf->header, dev->com.pg_size, spare, dev->attr->spare_size);

		if (UFFS_FLASH_IS_BAD_BLOCK(ret))
			goto bad_out;
	}
	for (i = 0; i < dev->attr->pages_per_block; i++) {
		memset(buf->header, 0xFF, dev->com.pg_size);
		memset(&ts, 0xFF, sizeof(ts));
		memset(spare, 0xFF, dev->attr->spare_size);

		if (ops->ReadPageWithLayout) {
			ret = ops->ReadPageWithLayout(dev, block, i, buf->header, dev->com.pg_size, NULL, &ts, NULL);
			if (UFFS_FLASH_IS_BAD_BLOCK(ret))
				goto bad_out;
			for (j = 0; j < dev->com.pg_size; j++)
				if (buf->header[j] != 0)
					goto bad_out;
			for (j = 0; j < sizeof(ts); j++)
				if (((u8 *)&ts)[j] != 0)
					goto bad_out;
		}
		else {
			ret = ops->ReadPage(dev, block, i, buf->header, dev->com.pg_size, NULL, spare, dev->attr->spare_size);
			if (UFFS_FLASH_IS_BAD_BLOCK(ret))
				goto bad_out;
			for (j = 0; j < dev->com.pg_size; j++)
				if (buf->header[j] != 0)
					goto bad_out;
			for (j = 0; j < dev->attr->spare_size; j++)
				if (spare[j] != 0)
					goto bad_out;
		}
	}

	//step 2: Erase, and check
	ret = uffs_FlashEraseBlock(dev, block);
	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		goto bad_out;

	for (i = 0; i < dev->attr->pages_per_block; i++) {
		memset(buf->header, 0, dev->com.pg_size);
		memset(&ts, 0, sizeof(ts));
		memset(spare, 0, dev->attr->spare_size);

		if (ops->ReadPageWithLayout) {
			ret = ops->ReadPageWithLayout(dev, block, i, buf->header, dev->com.pg_size, NULL, &ts, NULL);
			if (UFFS_FLASH_IS_BAD_BLOCK(ret))
				goto bad_out;
			for (j = 0; j < dev->com.pg_size; j++)
				if (buf->header[j] != 0xFF)
					goto bad_out;
			for (j = 0; j < sizeof(ts); j++)
				if (((u8 *)&ts)[j] != 0xFF)
					goto bad_out;
		}
		else {
			ret = ops->ReadPage(dev, block, i, buf->header, dev->com.pg_size, NULL, spare, dev->attr->spare_size);
			if (UFFS_FLASH_IS_BAD_BLOCK(ret))
				goto bad_out;
			for (j = 0; j < dev->com.pg_size; j++)
				if (buf->header[j] != 0xFF)
					goto bad_out;
			for (j = 0; j < dev->attr->spare_size; j++)
				if (spare[j] != 0xFF)
					goto bad_out;
		}
	}

	// format succ
	bad = U_FALSE;

bad_out:
	if (bad == U_TRUE)
		uffs_FlashMarkBadBlock(dev, block);
ext:
	if (buf) 
		uffs_BufFreeClone(dev, buf);

	if (spare)
		uffs_PoolPut(SPOOL(dev), spare);

	return;
}
#endif



URET uffs_FormatDevice(uffs_Device *dev, UBOOL force)
{
	u16 i, slot;
	URET ret = U_SUCC;
	
	if (dev == NULL)
		return U_FAIL;

	if (dev->ops == NULL) 
		return U_FAIL;

	uffs_GlobalFsLockLock();

	ret = uffs_BufFlushAll(dev);

	if (dev->ref_count > 1 && force != U_TRUE) {
		uffs_Perror(UFFS_ERR_NORMAL,
					"can't format when dev->ref_count = %d",
					dev->ref_count);
		ret = U_FAIL;
	}

	if (ret == U_SUCC && force == U_TRUE) {
		uffs_DirEntryBufPutAll(dev);
		uffs_PutAllObjectBuf(dev);
		uffs_FdSignatureIncrease();
	}

	if (ret == U_SUCC &&
		uffs_BufIsAllFree(dev) == U_FALSE &&
		force == U_TRUE)
	{
		uffs_Perror(UFFS_ERR_NORMAL, "some page still in used!");
		ret = U_FAIL;
	}

	for (slot = 0; ret == U_SUCC && slot < MAX_DIRTY_BUF_GROUPS; slot++) {
		if (dev->buf.dirtyGroup[slot].count > 0) {
			uffs_Perror(UFFS_ERR_SERIOUS, "there still have dirty pages!");
			ret = U_FAIL;
		}
	}

	if (ret == U_SUCC)
		uffs_BufSetAllEmpty(dev);


	if (ret == U_SUCC && uffs_BlockInfoIsAllFree(dev) == U_FALSE) {
		uffs_Perror(UFFS_ERR_NORMAL,
					"there still have block info cache ? fail to format");
		ret = U_FAIL;
	}

	if (ret == U_SUCC)
		uffs_BlockInfoExpireAll(dev);

	for (i = dev->par.start; ret == U_SUCC && i <= dev->par.end; i++) {
		if (uffs_FlashIsBadBlock(dev, i) == U_FALSE) {
			uffs_FlashEraseBlock(dev, i);
			if (HAVE_BADBLOCK(dev))
				uffs_BadBlockProcess(dev, NULL);
		}
		else {
#ifdef CONFIG_ENABLE_BAD_BLOCK_VERIFY
			_ForceFormatAndCheckBlock(dev, i);
#endif
		}
	}

	if (ret == U_SUCC && uffs_TreeRelease(dev) == U_FAIL) {
		ret = U_FAIL;
	}

	if (ret == U_SUCC && uffs_TreeInit(dev) == U_FAIL) {
		ret = U_FAIL;
	}

	if (ret == U_SUCC && uffs_BuildTree(dev) == U_FAIL) {
		ret = U_FAIL;
	}

	uffs_GlobalFsLockUnlock();

	return ret;
}

