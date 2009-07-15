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
 * \file uffs_public.h
 * \brief public data structures for uffs
 * \author Ricky Zheng
 */

#ifndef UFFS_PUBLIC_H
#define UFFS_PUBLIC_H

#include "uffs/uffs_types.h"
#include "uffs/uffs_config.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs.h"

#ifdef __cplusplus
extern "C"{
#endif


/** 
 * \struct uffs_TagsSt
 * \note size: 12 bytes, small enough to be embedded into the 'spare' of NAND flash page.
 *			Although this struct is designed as close as the NAND flash spare, it is *NOT*
 *          the physical map of NAND flash page's 'spare'!! It's "Logical Spare".
 *	\see Logic Spare <--> Physical Spare : uffs_FlashOps->LoadPageSpare,->WritePageSpare
 */
struct uffs_TagsSt {
	u8 dirty:1;				//!< 0: dirty, 1: clear
	u8 valid:1;				//!< 0: valid, 1: invalid
	u8 type:4;				//!< block type: #UFFS_TYPE_DIR, #UFFS_TYPE_FILE, #UFFS_TYPE_DATA or #UFFS_TYPE_RESV
	u8 block_ts:2;	//!< time stamp of block;

	u8 page_id;				//!< page id
	u16 father;				//!< father's serial number
	u16 serial;				//!< serial number

	u16 data_len;			//!< length of page data length
	u16 dataSum;			//!< sum of file name or directory name
#if defined(ENABLE_TAG_CHECKSUM) && ENABLE_TAG_CHECKSUM == 1
	u8 checksum;			//!< checksum of above, or ecc of tag...
#endif
	u8 block_status;			//!< block status, this byte is loaded from flash, but not write to flash directly

};

/**
 * \struct uffs_TagsSt_8
 * \brief special version for 8 bytes spare (not fit for 12 bytes uffs_Tags)
 */
struct uffs_TagsSt_8 {
	u8 dirty:1;				//!< 0: dirty, 1: clear
	u8 valid:1;				//!< 0: valid, 1: invalid
	u8 type:4;				//!< block type: #UFFS_TYPE_DIR, #UFFS_TYPE_FILE, #UFFS_TYPE_DATA or #UFFS_TYPE_RESV
	u8 block_ts:2;			//!< time stamp of block;

	u8 page_id;				//!< page id
	u8 father;				//!< father's serial number, warning: using 8-bit, blocks should not > 254
	u8 serial;				//!< serial number, warning: using 8-bit, blocks should not > 254

	u16 dataSum;			//!< sum of file name or directory name
	u8 data_len;			//!< length of page data length, warning: using 8-bit
	u8 block_status;			//!< block status, this byte is loaded from flash, but not write to flash directly
};

/*
New tag: ecc embedded
struct uffs_TagsSt {
//0:
	u8 dirty:1;				//!< 0: dirty, 1: clear
	u8 valid:1;				//!< 0: valid, 1: invalid
	u8 type:4;				//!< block type: #UFFS_TYPE_DIR, #UFFS_TYPE_FILE, #UFFS_TYPE_DATA or #UFFS_TYPE_RESV
	u8 block_ts:2;	//!< time stamp of block;
//1:
	u8 dataSum;				//!< sum of file name or directory name, or ... ?
//2:
	u16 father;				//!< father's serial number
	u16 serial;				//!< serial number
//6:	
	u16 data_len;			//!< length of page data: maximum 64K bytes
//8:
#if NAND_PAGE_DATASIZE == 512
	u8 ecc[6];				//!< ecc for 512 page size
#elseif NAND_PAGE_DATASIZE == 1024
	u8 ecc[12];				//!< ecc for 1K page size
#elseif NAND_PAGE_DATASIZE == 2048
	u8 ecc[24];				//!< ecc for 2K page size
#endif
	u8 page_id;				//!< maximum 256 pages in a block

	u8 block_status;			//!< block status, this byte is loaded from flash, but not write to flash directly !

}

*/



/** uffs_TagsSt.dirty */
#define TAG_VALID		0
#define TAG_INVALID		1

/** uffs_TagsSt.valid */
#define TAG_DIRTY		0
#define TAG_CLEAR		1


int uffs_GetFirstBlockTimeStamp(void);
int uffs_GetNextBlockTimeStamp(int prev);
UBOOL uffs_IsSrcNewerThanObj(int src, int obj);

void uffs_TransferToTag8(uffs_Tags *tag, uffs_Tags_8 *tag_8);
void uffs_TransferFromTag8(uffs_Tags *tag, uffs_Tags_8 *tag_8);


#include "uffs_device.h"

/********************* uffs disk and partition ***************************/

#define UFFS_DISK_MARK				0x53464655	//'UFFS'
#define UFFS_DISK_INFO_SIZE			64
#define UFFS_PARTITION_INFO_SIZE	32

//PHICICAL storage
//uffs disk information residents on first block of NAND flash
//Assume that NAND flash's first block is always good.
struct uffs_DiskInfoSt {
	u32 uffs_mark;		//!< will be 'UFFS', value: 0x53464655
	u32 version;		//!< uffs version
	u32 reserved[16];	//!< reserved
	u32 status;			//!< uffs status
	u32 partitions;		//!< partition nums
	u32 alternate;		//!< diskinfo alternate block
};

struct uffs_PartitionInfoSt {
	u32 status;				//partition status
	u32 create_time;			//create time
	u32 lastModified;		//last modified time
	u32 block_begin;		//partition begin block
	u32 block_end;			//partition end block
	u32 access;				//access mask
};


struct uffs_StorageSt {
	int mainBlock;
	struct uffs_DiskInfoSt main;
	int partsNum;
	struct uffs_PartitionInfoSt *parts;
};



/********************************** debug & error *************************************/
#define UFFS_ERR_NOISY		-1
#define UFFS_ERR_NORMAL		0
#define UFFS_ERR_SERIOUS	1
#define UFFS_ERR_DEAD		2

//#define UFFS_DBG_LEVEL	UFFS_ERR_NORMAL	
#define UFFS_DBG_LEVEL	UFFS_ERR_NOISY	

void uffs_Perror( int level, const char *errFmt, ... );

/********************************** NAND **********************************************/
//NAND flash specific file must implement these interface
URET uffs_LoadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag);
URET uffs_WritePageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag);
URET uffs_MakePageValid(uffs_Device *dev, int block, int page, uffs_Tags *tag);
UBOOL uffs_IsBlockBad(uffs_Device *dev, uffs_BlockInfo *bc);

/********************************** Public defines *****************************/
/**
 * \def UFFS_ALL_PAGES 
 * \brief UFFS_ALL_PAGES if this value presented, that means the objects are all pages in the block
 */
#define UFFS_ALL_PAGES (0xffff)

/** 
 * \def UFFS_INVALID_PAGE
 * \brief macro for invalid page number
 */
#define UFFS_INVALID_PAGE	(0xfffe)
#define UFFS_INVALID_BLOCK	(0xfffe)


URET uffs_NewBlock(uffs_Device *dev, u16 block, uffs_Tags *tag, uffs_Buf *buf);
URET uffs_BlockRecover(uffs_Device *dev, uffs_BlockInfo *old, u16 newBlock);
URET uffs_PageRecover(uffs_Device *dev, 
					  uffs_BlockInfo *bc, 
					  u16 oldPage, 
					  u16 newPage, 
					  uffs_Buf *buf);
int uffs_FindFreePageInBlock(uffs_Device *dev, uffs_BlockInfo *bc);
u8 uffs_CalTagCheckSum(uffs_Tags *tag);
u16 uffs_FindBestPageInBlock(uffs_Device *dev, uffs_BlockInfo *bc, u16 page);
u16 uffs_FindFirstValidPage(uffs_Device *dev, uffs_BlockInfo *bc);
u16 uffs_FindFirstFreePage(uffs_Device *dev, uffs_BlockInfo *bc, u16 pageFrom);
u16 uffs_FindPageInBlockWithPageId(uffs_Device *dev, uffs_BlockInfo *bc, u16 page_id);

u8 uffs_MakeSum8(const void *p, int len);
u16 uffs_MakeSum16(const void *p, int len);
URET uffs_CreateNewFile(uffs_Device *dev, u16 father, u16 serial, uffs_BlockInfo *bc, uffs_FileInfo *fi);

int uffs_GetBlockFileDataLength(uffs_Device *dev, uffs_BlockInfo *bc, u8 type);
UBOOL uffs_IsPageErased(uffs_Device *dev, uffs_BlockInfo *bc, u16 page);
int uffs_GetFreePagesCount(uffs_Device *dev, uffs_BlockInfo *bc);
UBOOL uffs_IsDataBlockReguFull(uffs_Device *dev, uffs_BlockInfo *bc);

int uffs_GetBlockTimeStamp(uffs_Device *dev, uffs_BlockInfo *bc);

URET uffs_WriteDataToNewPage(uffs_Device *dev, 
							 u16 block, 
							 u16 page,
							 uffs_Tags *tag,
							 uffs_Buf *buf);


int uffs_GetDeviceUsed(uffs_Device *dev);
int uffs_GetDeviceFree(uffs_Device *dev);
int uffs_GetDeviceTotal(uffs_Device *dev);




/************************************************************************/
/*  init functions                                                                     */
/************************************************************************/
URET uffs_InitDevice(uffs_Device *dev);
URET uffs_ReleaseDevice(uffs_Device *dev);


URET uffs_InitFlashClass(uffs_Device *dev);



#ifdef __cplusplus
}
#endif
#endif
