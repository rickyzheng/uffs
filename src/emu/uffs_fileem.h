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
 * \file uffs_fileem.h
 * \brief Emulate NAND flash with host file.
 * \author Ricky Zheng
 */

#ifndef _UFFS_FILEEM_H_
#define _UFFS_FILEEM_H_

#include "uffs/uffs_device.h"

struct uffs_FileEmuBitFlip {
	int block;
	int page;
	int offset;
	u8 mask;
};

/* simulate bad blocks */
#define FILEEMU_STOCK_BAD_BLOCKS	{5, 18}		// bad block come from manufacture
#define FILEEMU_ERASE_BAD_BLOCKS	{10, 15}	// new bad block discovered when erasing

/* simulating bit flip */
#define FILEEMU_WRITE_BIT_FLIP \
	{ \
		{2, 2, 10, 1 << 4}, /* block 2, page 2, offset 10, bit 4 */	\
		{2, 4, -3, 1 << 2}, /* block 2, page 4, spare offset 3, bit 2*/ \
		{6, 1, 5, 1 << 3},	/* block 6, page 1, offset 5, bit 3 */ \
		{6, 1, 15, 1 << 7},	/* block 6, page 1, offset 300, bit 7 */ \
		{8, 2, 2, 1 << 1},	/* block 8, page 2, offset 2, bit 1 */ \
		{8, 2, 100, 1 << 5},/* block 8, page 2, offset 100, bit 5 */ \
	}

extern struct uffs_FlashOpsSt g_femu_ops_ecc_soft;		// for software ECC or no ECC.
extern struct uffs_FlashOpsSt g_femu_ops_ecc_hw;		// for hardware ECC
extern struct uffs_FlashOpsSt g_femu_ops_ecc_hw_auto;	// for auto hardware ECC

#define PAGE_DATA_WRITE_COUNT_LIMIT		1
#define PAGE_SPARE_WRITE_COUNT_LIMIT	1

typedef struct uffs_FileEmuSt {
	int initCount;
	FILE *fp;
	FILE *dump_fp;
	u8 *em_monitor_page;
	u8 * em_monitor_spare;
	const char *emu_filename;
	struct uffs_FlashOpsSt ops_orig;
} uffs_FileEmu;

void uffs_fileem_setup_device(uffs_Device *dev);

#endif

