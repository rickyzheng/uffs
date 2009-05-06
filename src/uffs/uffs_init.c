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
 * \file uffs_init.c
 * \brief initialize uffs file system device
 * \author Ricky Zheng, created 12th May, 2005
 */

#include "uffs/uffs_types.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_config.h"
#include "uffs/uffs_tree.h"
#include "uffs/uffs_fs.h"
#include "uffs/uffs_badblock.h"
#include <string.h>

#define PFX "init: "

URET uffs_InitDevice(uffs_Device *dev)
{
	URET ret;

	if (dev->mem.init) {
		if (dev->mem.init(dev) != U_SUCC) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"Init memory allocator fail.\n");
			return U_FAIL;
		}
	}

	memset(&(dev->st), 0, sizeof(uffs_stat));

	uffs_DeviceInitLock(dev);
	uffs_InitBadBlock(dev);

	if(uffs_InitFlashClass(dev) != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't initialize flash class!\n");
		goto fail;
	}

	uffs_Perror(UFFS_ERR_NOISY, PFX"init page buf\n");
	ret = uffs_BufInit(dev, MAX_PAGE_BUFFERS, MAX_DIRTY_PAGES_IN_A_BLOCK);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"Initialize page buffers fail\n");
		goto fail;
	}
	uffs_Perror(UFFS_ERR_NOISY, PFX"init block info cache\n");
	ret = uffs_InitBlockInfoCache(dev, MAX_CACHED_BLOCK_INFO);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"Initialize block info fail\n");
		goto fail;
	}

	ret = uffs_InitTreeBuf(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to init tree buffers\n");
		goto fail;
	}

	ret = uffs_BuildTree(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to build tree\n");
		goto fail;
	}

	return U_SUCC;

fail:
	uffs_DeviceReleaseLock(dev);

	return U_FAIL;
}

URET uffs_ReleaseDevice(uffs_Device *dev)
{
	URET ret;

	ret = uffs_ReleaseBlockInfoCache(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "fail to release block info.\n");
		return U_FAIL;
	}

	ret = uffs_BufReleaseAll(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "fail to release page buffers\n");
		return U_FAIL;
	}

	ret = uffs_ReleaseTreeBuf(dev);
	if(ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to release tree buffers!\n");
		return U_FAIL;
	}

	if (dev->mem.release) ret = dev->mem.release(dev);
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to release memory allocator!\n");
	}

	uffs_DeviceReleaseLock(dev);

	return ret;

}

URET uffs_initMountTable(void)
{
	struct uffs_mountTableSt *tbl = uffs_GetMountTable();
	struct uffs_mountTableSt *work;

	for(work = tbl; work; work = work->next) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"init device for mount point %s ...\n", work->mountPoint);
		if(work->dev->Init(work->dev) == U_FAIL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"init device for mount point %s fail\n", work->mountPoint);
			return U_FAIL;
		}

		work->dev->par.start = work->startBlock;
		if(work->endBlock < 0) {
			work->dev->par.end = work->dev->attr->total_blocks + work->endBlock;
		}
		else {
			work->dev->par.end = work->endBlock;
		}
		uffs_Perror(UFFS_ERR_NOISY, PFX"mount partiton: %d,%d\n",
			work->dev->par.start, work->dev->par.end);

		if (uffs_InitDevice(work->dev) != U_SUCC) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"init device fail !\n");
			return U_FAIL;
		}
	}

	return U_SUCC;
}

URET uffs_releaseMountTable(void)
{
	struct uffs_mountTableSt *tbl = uffs_GetMountTable();
	struct uffs_mountTableSt *work;
	for(work = tbl; work; work = work->next) {
		uffs_ReleaseDevice(work->dev);
		work->dev->Release(work->dev);
	}
	return U_SUCC;
}


