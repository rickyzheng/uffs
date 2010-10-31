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
static URET femu_ReadPage_wrap(uffs_Device *dev, u32 block, u32 page, u8 *data, int data_len, u8 *ecc,
							u8 *spare, int spare_len);
static int femu_WritePage_wrap(uffs_Device *dev, u32 block, u32 page,
							const u8 *data, int data_len, const u8 *spare, int spare_len);
static URET femu_EraseBlock_wrap(uffs_Device *dev, u32 blockNumber);

static URET CheckInit(uffs_Device *dev)
{
	int i;
	int fSize;
	int written;
	u8 * p = em_page_buf;
	uffs_FileEmu *emu;
#ifdef FILEEMU_STOCK_BAD_BLOCKS
	int bad_blocks[] = FILEEMU_STOCK_BAD_BLOCKS;
	int j;
	u8 x = 0;
#endif
	
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
#ifdef FILEEMU_STOCK_BAD_BLOCKS
			for (j = 0; j < ARRAY_SIZE(bad_blocks); j++) {
				if (bad_blocks[j] < blks) {
					printf(" --- manufacture bad block %d ---\n", bad_blocks[j]);
					fseek(emu->fp, bad_blocks[j] * blk_size + pgd_size + dev->attr->block_status_offs, SEEK_SET);
					fwrite(&x, 1, 1, emu->fp);
				}
			}
#endif
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
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->_private);

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

	memcpy(&emu->ops_orig, dev->ops, sizeof(struct uffs_FlashOpsSt));

	emu->ops_orig.EraseBlock = femu_EraseBlock;
	dev->ops->EraseBlock = femu_EraseBlock_wrap;

	if (dev->ops->ReadPage)
		dev->ops->ReadPage = femu_ReadPage_wrap;
	if (dev->ops->WritePage)
		dev->ops->WritePage = femu_WritePage_wrap;

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

static URET femu_ReadPage_wrap(uffs_Device *dev, u32 block, u32 page, u8 *data, int data_len, u8 *ecc,
							u8 *spare, int spare_len)
{
	uffs_FileEmu *emu = (uffs_FileEmu *)(dev->attr->_private);

	//printf("femu: Read block %d page %d data %d spare %d\n", block, page, data_len, spare_len);	

	return emu->ops_orig.ReadPage(dev, block, page, data, data_len, ecc, spare, spare_len);
}

static int femu_WritePage_wrap(uffs_Device *dev, u32 block, u32 page,
							const u8 *data, int data_len, const u8 *spare, int spare_len)
{
	uffs_FileEmu *emu = (uffs_FileEmu *)(dev->attr->_private);

#ifdef FILEEMU_WRITE_BIT_FLIP
	struct uffs_FileEmuBitFlip flips[] = FILEEMU_WRITE_BIT_FLIP;
	struct uffs_FileEmuBitFlip *x;
	u8 buf[UFFS_MAX_PAGE_SIZE + UFFS_MAX_SPARE_SIZE];
	int i;
	u8 *p;

	if (data) {
		memcpy(buf, data, data_len);
		data = buf;
	}
	if (spare) {
		memcpy(buf + UFFS_MAX_PAGE_SIZE, spare, spare_len);
		spare = buf + UFFS_MAX_PAGE_SIZE;
	}

	for (i = 0; i < ARRAY_SIZE(flips); i++) {
		x = &flips[i];
		if (x->block == block && x->page == page) {
			p = NULL;
			if (x->offset > 0 && data && x->offset < data_len) {
				printf(" --- Inject data bit flip at block%d, page%d, offset%d, mask%d --- \n", block, page, x->offset, x->mask);
				p = (u8 *)(data + x->offset);
			}
			else if (x->offset < 0 && spare && -(x->offset ) < spare_len) {
				printf(" --- Inject spare bit flip at block%d, page%d, offset%d, mask%d --- \n", block, page, -x->offset, x->mask);
				p = (u8 *)(spare - x->offset);
			}
			if (p) {
				*p = (*p & ~x->mask) | (~(*p & x->mask) & x->mask);
			}
		}
	}
#endif

	//printf("femu: Write block %d page %d data %d spare %d\n", block, page, data_len, spare_len);	
	
	return emu->ops_orig.WritePage(dev, block, page, data, data_len, spare, spare_len);
}

static URET femu_EraseBlock_wrap(uffs_Device *dev, u32 blockNumber)
{
	uffs_FileEmu *emu = (uffs_FileEmu *)(dev->attr->_private);

#ifdef FILEEMU_ERASE_BAD_BLOCKS
	int blocks[] = FILEEMU_ERASE_BAD_BLOCKS;
	int i;
	URET ret;
	ret = emu->ops_orig.EraseBlock(dev, blockNumber);

	for (i = 0; i < ARRAY_SIZE(blocks); i++) {
		if (blockNumber == blocks[i]) {
			printf(" --- Inject bad block%d when erasing --- \n", blockNumber);
			ret = UFFS_FLASH_BAD_BLK;
		}
	}

	return ret;		

#else

	return emu->ops_orig.EraseBlock(dev, blockNumber);

#endif
}


void uffs_fileem_setup_device(uffs_Device *dev)
{
	dev->Init = femu_initDevice;
	dev->Release = femu_releaseDevice;
}


/////////////////////////////////////////////////////////////////////////////////
