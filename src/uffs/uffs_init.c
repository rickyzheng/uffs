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

	memset(&(dev->st), 0, sizeof(uffs_FlashStat));

	uffs_DeviceInitLock(dev);
	uffs_InitBadBlock(dev);

	if (uffs_InitFlashClass(dev) != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't initialize flash class!\n");
		goto fail;
	}

	uffs_Perror(UFFS_ERR_NOISY, PFX"init page buf\n");
	ret = uffs_BufInit(dev, MAX_PAGE_BUFFERS, MAX_DIRTY_PAGES_IN_A_BLOCK);
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"Initialize page buffers fail\n");
		goto fail;
	}
	uffs_Perror(UFFS_ERR_NOISY, PFX"init block info cache\n");
	ret = uffs_InitBlockInfoCache(dev, MAX_CACHED_BLOCK_INFO);
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_DEAD, PFX"Initialize block info fail\n");
		goto fail;
	}

	ret = uffs_InitTreeBuf(dev);
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to init tree buffers\n");
		goto fail;
	}

	ret = uffs_BuildTree(dev);
	if (ret != U_SUCC) {
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
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "fail to release block info.\n");
		return U_FAIL;
	}

	ret = uffs_BufReleaseAll(dev);
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX "fail to release page buffers\n");
		return U_FAIL;
	}

	ret = uffs_ReleaseTreeBuf(dev);
	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to release tree buffers!\n");
		return U_FAIL;
	}

	if (dev->mem.release)
		ret = dev->mem.release(dev);

	if (ret != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to release memory allocator!\n");
	}

	uffs_DeviceReleaseLock(dev);

	return ret;

}

