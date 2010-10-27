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
 * \file uffs_fileem.c
 * \brief emulate uffs file system
 * \author Ricky Zheng, created 9th May, 2005
 */
  

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "uffs/uffs_device.h"
#include "uffs_fileem.h"

#define PFX "femu: "

static u8 em_page_buf[UFFS_MAX_PAGE_SIZE + UFFS_MAX_SPARE_SIZE];

static URET emu_initDevice(uffs_Device *dev);


static URET CheckInit(uffs_Device *dev)
{
	int i;
	int fSize;
	int written;
	u8 * p = em_page_buf;
	uffs_FileEmu *emu;
	
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->page_data_size * dev->attr->pages_per_block;
	
	emu = (uffs_FileEmu *)(dev->attr->_private);

	if (emu->initCount > 0) {
		emu->initCount++;
		return U_SUCC;
	}

	emu->em_monitor_page = (u8 *) malloc(dev->attr->total_blocks * dev->attr->pages_per_block);
	if (!emu->em_monitor_page)
		return U_FAIL;
	emu->em_monitor_spare = (u8 *) malloc(dev->attr->total_blocks * dev->attr->pages_per_block);
	if (!emu->em_monitor_spare)
		return U_FAIL;


	//clear monitor
	memset(emu->em_monitor_page, 0, blks * blk_pgs);
	memset(emu->em_monitor_spare, 0, blks * blk_pgs);

	emu->fp = fopen(emu->emu_filename, "rb");
	if (emu->fp == NULL) {
		emu->fp = fopen(emu->emu_filename, "ab+");
		if (emu->fp == NULL) {
			printf(PFX"Failed to create uffs emulation file.");
			return U_FAIL;
		}

		fseek(emu->fp, 0, SEEK_END);
		fSize = ftell(emu->fp);
		
		if (fSize < blk_size * blks)	{
			printf("Creating uffs emulation file\n");
			fseek(emu->fp, 0, SEEK_SET);
			memset(p, 0xff, pgd_size + sp_size);
			for (i = 0; i < blk_pgs * blks; i++)	{
				written = fwrite(p, 1, pgd_size + sp_size, emu->fp);
				if (written != pgd_size + sp_size)	{
					printf("Write failed\n");
					fclose(emu->fp);
					emu->fp = NULL;
					return U_FAIL;
				}
			}		
		}
	}
	fflush(emu->fp);	
	fclose(emu->fp);

	emu->fp = fopen(emu->emu_filename, "rb+");
	if (emu->fp == NULL) {
		printf(PFX"Can't open emulation file.\n");
		return U_FAIL;
	}

	emu->initCount++;

	return U_SUCC;
}



static URET femu_EraseBlock(uffs_Device *dev, u32 blockNumber)
{

	int i;
	u8 * pg = em_page_buf;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->_private);
	if (!emu || !(emu->fp))
		goto err;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->page_data_size * dev->attr->pages_per_block;
	
	printf("femu: erase block %d\n", blockNumber);

	if ((int)blockNumber >= blks) {
		printf("Attempt to erase non-existant block %d\n",blockNumber);
		goto err;
	}
	else {

		//clear this block monitors
		memset(emu->em_monitor_page + (blockNumber * blk_pgs), 
			0, 
			blk_pgs * sizeof(u8));
		memset(emu->em_monitor_spare + (blockNumber * blk_pgs),
			0,
			blk_pgs * sizeof(u8));
		
		if (1 && (blockNumber == 5)) {  // simulate bad block 
			return UFFS_FLASH_BAD_BLK;
		}

		memset(pg, 0xff, (pgd_size + sp_size));
		
		fseek(emu->fp, blockNumber * blk_pgs * (pgd_size + sp_size), SEEK_SET);
		
		for (i = 0; i < blk_pgs; i++)	{
			fwrite(pg, 1, (pgd_size + sp_size), emu->fp);
		}

		fflush(emu->fp);
		dev->st.block_erase_count++;
	}

	return UFFS_FLASH_NO_ERR;
err:
	return UFFS_FLASH_IO_ERR;
	
}


/////////////////////////////////////////////////////////////////////////////////

static URET femu_initDevice(uffs_Device *dev)
{
	uffs_Perror(UFFS_ERR_NORMAL,  "femu device init.");

	switch(dev->attr->ecc_opt) {
		case UFFS_ECC_NONE:
		case UFFS_ECC_SOFT:
			dev->ops = &g_femu_ops_ecc_soft;
			break;
		case UFFS_ECC_HW:
			dev->ops = &g_femu_ops_ecc_hw;
			break;
		case UFFS_ECC_HW_AUTO:
			dev->ops = &g_femu_ops_ecc_hw_auto;
			break;
		default:
			break;
	}

	dev->ops->EraseBlock = femu_EraseBlock;

	CheckInit(dev);

	return U_SUCC;
}

static URET femu_releaseDevice(uffs_Device *dev)
{
	uffs_FileEmu *emu;

	uffs_Perror(UFFS_ERR_NORMAL,  "femu device release.");

	emu = (uffs_FileEmu *)(dev->attr->_private);

	emu->initCount--;
	if (emu->initCount == 0) {
		if (emu->fp) {
			fclose(emu->fp);
			emu->fp = NULL;
		}

		if (emu->em_monitor_page)
			free(emu->em_monitor_page);
		if (emu->em_monitor_spare) 
			free(emu->em_monitor_spare);
		emu->em_monitor_page = NULL;
		emu->em_monitor_spare = NULL;
	}

	return U_SUCC;
}


void uffs_fileem_setup_device(uffs_Device *dev)
{
	dev->Init = femu_initDevice;
	dev->Release = femu_releaseDevice;
}

/////////////////////////////////////////////////////////////////////////////////
