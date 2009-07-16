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
 * \brief flash interface for UFFS
 * \author Ricky Zheng
 */

#ifndef _UFFS_FLASH_H_
#define _UFFS_FLASH_H_

#include "uffs/uffs_types.h"
#include "uffs/uffs_config.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs_device.h"

#ifdef __cplusplus
extern "C"{
#endif


/** ECC options (uffs_StorageAttrSt.ecc_opt) */
#define UFFS_ECC_NONE		0	//!< do not use ECC
#define UFFS_ECC_SOFT		1	//!< UFFS calculate the ECC
#define UFFS_ECC_HW			2	//!< Flash driver(or by hardware) calculate the ECC
#define UFFS_ECC_HW_AUTO	3	//!< Hardware calculate the ECC and automatically write to spare.

/** 
 * \struct uffs_StorageAttrSt
 * \brief uffs device storage attribute, provide by nand specific file
 */
struct uffs_StorageAttrSt {
	u32 total_blocks;		//!< total blocks in this chip
	u32 block_data_size;	//!< block data size (= page_data_size * pages_per_block)
	u16 page_data_size;		//!< page data size (physical page data size, e.g. 512)
	u16 spare_size;			//!< page spare size (physical page spare size, e.g. 16)
	u16 pages_per_block;	//!< pages per block
	u16 block_status_offs;	//!< block status byte in spare
	int ecc_opt;			//!< ecc option ( #UFFS_ECC_[NONE|SOFT|HW|HW_AUTO] )
	const u8 *ecc_layout;	//!< ECC layout: [ofs1, size1, ofs2, size2, ..., 0xFF, 0]
	const u8 *data_layout;	//!< spare data layout: [ofs1, size1, ofs2, size2, ..., 0xFF, 0]
	void *private;			//!< private data for storage attribute
};


/**
 * \struct uffs_DeviceOpsSt 
 * \brief lower level flash operations, should be implement in flash driver
 */
struct uffs_DeviceOpsSt {
	/**
	 * Read page data and calculate ECC. flash driver MUST provide this function.
	 * if ecc_opt is UFFS_ECC_HW, flash driver must use their own ECC algrithm,
	 * or use hardware calculate the ECC. 
	 */
	URET (*ReadPageData)(uffs_Device *dev, u32 block, u32 page, u8 *page, int len, u8 *ecc);

	/**
	 * Read page spare. flash driver MUST provide this function.
	 */
	URET (*ReadPageSpare)(uffs_Device *dev, u32 block, u32 page, u8 *spare, int len);

	/**
	 * Write page data and calculate ECC. flash driver MUST provide this function.
	 * if ecc_opt is UFFS_ECC_HW, flash driver must use their own ECC algrithm,
	 * or use hardware calculate the ECC. 
	 */
	URET (*WritePageData)(uffs_Device *dev, u32 block, u32 page, const u8 *page, int len, u8 *ecc);

	/**
	 * Write page spare. flash driver MUST provide this function.
	 * if ecc_opt is UFFS_ECC_HW_AUTO, flash driver should not overwite the ECC part.
	 */
	URET (*WritePageSpare)(uffs_Device *dev, u32 block, u32 page, const u8 *spare, int len);

	/**
	 * check if this is a bad block. this function is optional.
	 * if not provided, UFFS check will the status byte in spare.
	 */
	UBOOL (*IsBadBlock)(uffs_Device *dev, u32 block);

	/**
	 * Mark a new bad block. flash driver MUST provide this function.
	 */
	URET (*MarkBadBlock)(uffs_Device *dev, u32 block);

	/**
	 * Erase a block. flash driver MUST provide this function.
	 */
	URET (*EraseBlock)(uffs_Device *dev, u32 block);
};


/** read page spare, fill tag and ECC */
URET uffs_ReadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag, u8 *ecc);

/** read page data to page buf and calculate ecc */
URET uffs_ReadPage(uffs_Device *dev, int block, int page, uffs_Buf *buf, u8 *ecc);

/** write page data, tag and ecc */
URET uffs_WritePageCombine(uffs_Device *dev, int block, int page, uffs_Buf *buf, uffs_Tags *tag, u8 *ecc);

/** Mark this block as bad block */
URET uffs_MarkBadBlock(uffs_Device *dev, int block);

/** Erase flash block */
URET uffs_EraseBlock(uffs_Device *dev, int block);


#ifdef __cplusplus
}
#endif
#endif
