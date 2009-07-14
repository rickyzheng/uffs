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
 * \file uffs_blockinfo.h
 * \brief data structure for operating block information
 * \author Ricky Zheng
 */

#ifndef _UFFS_BLOCKINFO_H_
#define _UFFS_BLOCKINFO_H_

#include "uffs/uffs_types.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_core.h"

#ifdef __cplusplus
extern "C"{
#endif
/** 
 * \struct uffs_PageSpareSt
 * \brief this structure is for storing uffs tag and more. 
 */
struct uffs_PageSpareSt {
	u8 expired:1;			//!< 0 not expired. 1 expired. 
	u8 check_ok:1;			//!< 0 check sum is OK. 1 check sum is error. 
	u8 block_status:1;		//!< for block status, 1 bad, 0 valid 
	uffs_Tags tag;			//!< page tag 
};

/** 
 * \struct uffs_BlockInfoSt
 * \brief block information data. Block info is frequently accessed,
          UFFS use a cache system to speed up block info access.
 */
struct uffs_BlockInfoSt {
	struct uffs_BlockInfoSt *next;
	struct uffs_BlockInfoSt *prev;
	u16 block;							//!< block number
	struct uffs_PageSpareSt *spares;	//!< page spare info array
	int expired_count;					//!< how many pages expired in this block ? 
	int ref_count;						//!< reference counter, it's safe to reuse this block memory when the counter is 0.
};

/** initialize block info caches */
URET uffs_InitBlockInfoCache(uffs_Device *dev, int maxCachedBlocks);

/** release block info caches */
URET uffs_ReleaseBlockInfoCache(uffs_Device *dev);

/** check page spare checksum, and set spare->check_ok */
void uffs_CheckPageSpare(uffs_Device *dev, uffs_PageSpare *spare);

/** load page spare to block info cache */
URET uffs_LoadBlockInfo(uffs_Device *dev, uffs_BlockInfo *work, int page);

/** find block info cache */
uffs_BlockInfo * uffs_FindBlockInfoInCache(uffs_Device *dev, int block);

/** get block info cache, load it on demand */
uffs_BlockInfo * uffs_GetBlockInfo(uffs_Device *dev, int block);

/** put info cache back to pool, should be called with #uffs_GetBlockInfo in pairs. */
void uffs_PutBlockInfo(uffs_Device *dev, uffs_BlockInfo *p);

/** explicitly expire a block info cache */
void uffs_ExpireBlockInfo(uffs_Device *dev, uffs_BlockInfo *p, int page);

/** no one hold any block info cache ? safe to release block info caches */
UBOOL uffs_IsAllBlockInfoFree(uffs_Device *dev);

/** explicitly expire all block info caches */
void uffs_ExpireAllBlockInfo(uffs_Device *dev);

#ifdef __cplusplus
}
#endif


#endif
