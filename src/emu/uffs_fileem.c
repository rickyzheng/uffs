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


#define MAXWRITETIME_PAGE 2
#define MAXWRITETIME_SPARE 2

static u8 em_page_buf[4096];


static URET CheckInit(uffs_Device *dev);
static URET femu_WritePage(uffs_Device *dev, u32 block, u32 page_num, const u8 *data, const u8 *spare);
static URET femu_WritePageData(uffs_Device *dev, u32 block, u32 page_num, const u8 *page, int ofs, int len);
static URET femu_WritePageSpare(uffs_Device *dev, u32 block, u32 page_num, const u8 *spare, int ofs, int len);
static URET femu_ReadPage(uffs_Device *dev, u32 block, u32 page_num, u8 *data, u8 *spare);
static URET femu_ReadPageData(uffs_Device *dev, u32 block, u32 page_num, u8 *page, int ofs, int len);
static URET femu_ReadPageSpare(uffs_Device *dev, u32 block, u32 page_num, u8 *spare, int ofs, int len);
static URET femu_EraseBlock(uffs_Device *dev, u32 blockNumber);
static URET femu_ResetFlash(uffs_Device *dev);
static UBOOL femu_IsBlockBad(uffs_Device *dev, u32 block);
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
	blk_size = dev->attr->block_data_size;
	
	emu = (uffs_FileEmu *)(dev->attr->private);

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

static URET femu_WritePage(uffs_Device *dev, u32 block, u32 page_num, const u8 *data, const u8 *spare)
{
	int ret;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;

	if (data) {
		ret = femu_WritePageData(dev, block, page_num, data, 0, pgd_size);
		if(ret == U_FAIL)
			return ret;
	}
	
	if (spare) {
		ret = femu_WritePageSpare(dev, block, page_num, spare, 0, sp_size);
		if(ret == U_FAIL)
			return ret;
	}

	return U_SUCC;	

}

static URET femu_WritePageData(uffs_Device *dev, u32 block, u32 page_num, const u8 *page, int ofs, int len)
{
	int written;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	uffs_FileEmu *emu;
	emu = (uffs_FileEmu *)(dev->attr->private);

	if (!emu || !(emu->fp))
		return U_FAIL;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;

	if (ofs + len > pgd_size) {
		printf("femu: write page data out of range!\n");
		return U_FAIL;
	}

	emu->em_monitor_page[block * blk_pgs + page_num]++;
	if (emu->em_monitor_page[block * blk_pgs + page_num] > MAXWRITETIME_PAGE) {
		printf("Warrning: block %d page %d exceed it's maximum write time!\r\n", block, page_num);
		return U_FAIL;
	}
	
	if (page) {
		fseek(emu->fp, 
			(block * blk_pgs + page_num) * 
			(pgd_size + sp_size) 
			+ ofs, SEEK_SET);
		written = fwrite(page, 1, len, emu->fp);
		
		if (written != len) {
			printf("femu: write page I/O error ?\n");
			return U_FAIL;
		}
	}

	dev->st.page_write_count++;
	fflush(emu->fp);
	
	return U_SUCC;
}

static URET femu_WritePageSpare(uffs_Device *dev, u32 block, u32 page_num, const u8 *spare, int ofs, int len)
{
	int written;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp)) 
		return U_FAIL;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;

//	printf("WS: %d/%d, size %d\n", block, page_num, len);
	
	if (ofs + len > sp_size) {
		printf("femu: write page data out of range!\n");
		return U_FAIL;
	}

	emu->em_monitor_spare[block*blk_pgs + page_num]++;
	if (emu->em_monitor_spare[block*blk_pgs + page_num] > MAXWRITETIME_SPARE) {
		printf("Warrning: block %d page %d (spare) exceed it's maximum write time!\r\n", block, page_num);
		return U_FAIL;
	}
	
	if (spare) {
		fseek(emu->fp, (block*blk_pgs + page_num) * (pgd_size + sp_size) + dev->attr->page_data_size + ofs, SEEK_SET);
		written = fwrite(spare, 1, len, emu->fp);
		if (written != len) {
			printf("femu: write spare I/O error ?\n");
			return U_FAIL;
		}
	}

	fflush(emu->fp);
	dev->st.spare_write_count++;

	return U_SUCC;
}


static URET femu_ReadPage(uffs_Device *dev, u32 block, u32 page_num, u8 *data, u8 *spare)
{
	int ret;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;

	if (data) {
		ret = femu_ReadPageData(dev, block, page_num, data, 0, pgd_size);
		if(ret == U_FAIL) 
			return U_FAIL;
	}
	
	if (spare) {
		ret = femu_ReadPageSpare(dev, block, page_num, spare, 0, sp_size);
		if(ret == U_FAIL)
			return U_FAIL;
	}

	return U_SUCC;	
}

static URET femu_ReadPageData(uffs_Device *dev, u32 block, u32 page_num, u8 *page, int ofs, int len)
{
	int nread;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp))
		return U_FAIL;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;

	if(ofs + len > pgd_size) {
		printf("femu: read page data out of range!\n");
		return U_FAIL;
	}
	
	if(page) {
		fseek(emu->fp, (block*blk_pgs + page_num) * (pgd_size + sp_size) + ofs, SEEK_SET);
		nread = fread(page, 1, len, emu->fp);

		// for ECC testing.
		if (0 && block == 100 && page_num == 3) {
			printf("--- ECC error inject to block 100 page 3 ---\n");
			page[13] = (page[13] & ~0x40) | (~(page[13] & 0x40) & 0x40) ;
		}
		
		if (nread != len) {
			printf("femu: read page I/O error ?\n");
			return U_FAIL;
		}
	}

	dev->st.page_read_count++;

	return U_SUCC;
}

static URET femu_ReadPageSpare(uffs_Device *dev, u32 block, u32 page_num, u8 *spare, int ofs, int len)
{
	int nread;
	int pos;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp))
		return U_FAIL;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;
	
//	printf("RS: %d/%d, size %d\n", block, page_num, len);

	if (ofs + len > sp_size) {
		printf("femu: read page spare out of range!\n");
		return U_FAIL;
	}

	if (spare) {
		pos = (block*blk_pgs + page_num) * (pgd_size + sp_size) + dev->attr->page_data_size + ofs;
		if (fseek(emu->fp, pos, SEEK_SET) != 0) {
			printf("femu: seek to %d fail!\n", pos);
			return U_FAIL;
		}
		nread= fread(spare, 1, len, emu->fp);
		
		if (nread != len) {
			printf("femu: read spare I/O error ?\n");
			return U_FAIL;
		}
	}	

	dev->st.spare_read_count++;
		
	return U_SUCC;
}



static URET femu_EraseBlock(uffs_Device *dev, u32 blockNumber)
{

	int i;
	u8 * pg = em_page_buf;
	int pg_size, pgd_size, sp_size, blks, blk_pgs, blk_size;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp))
		return U_FAIL;

	pg_size = dev->attr->page_data_size + dev->attr->spare_size;
	pgd_size = dev->attr->page_data_size;
	sp_size = dev->attr->spare_size;
	blk_pgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blk_size = dev->attr->block_data_size;
	
	printf("femu: erase block %d\n", blockNumber);

	if ((int)blockNumber >= blks) {
		printf("Attempt to erase non-existant block %d\n",blockNumber);
		return U_FAIL;
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

		return U_SUCC;
	}
	
}

static URET femu_ResetFlash(uffs_Device *dev)
{
	return U_SUCC;
}

static UBOOL femu_IsBlockBad(uffs_Device *dev, u32 block)
{
	//this function is called when formating flash device

	unsigned char mark;
	mark = 0;

	femu_ReadPageSpare(dev, block, 0, &mark, 5, 1);
	if (mark == 0xff) {
		femu_ReadPageSpare(dev, block, 1, &mark, 5, 1);
		if (mark == 0xff) {
			return U_FALSE;
		}
	}

	return U_TRUE;
}

/////////////////////////////////////////////////////////////////////////////////

static uffs_DevOps emu_DevOps = {
	femu_ResetFlash,
	femu_IsBlockBad,
	NULL, /* MarkBadBlock */
	femu_EraseBlock,
	femu_WritePage,
	femu_WritePageData,
	femu_WritePageSpare,
	femu_ReadPage,
	femu_ReadPageData,
	femu_ReadPageSpare,
};

static URET femu_initDevice(uffs_Device *dev)
{
	uffs_Perror(UFFS_ERR_NORMAL, PFX "femu device init.\n");

	dev->ops = &emu_DevOps;							/* EMU device operations */

	CheckInit(dev);

	return U_SUCC;
}

static void femu_printStatistic(uffs_Device *dev)
{
	uffs_FlashStat *s;
	s = &(dev->st);
	
	printf("-----------statistics-----------\n");
	printf("Block Erased: %d\n", s->block_erase_count);
	printf("Write Page:   %d\n", s->page_write_count);
	printf("Write Spare:  %d\n", s->spare_write_count);
	printf("Read Page:    %d\n", s->page_read_count);
	printf("Read Spare:   %d\n", s->spare_read_count);
	printf("Disk total:   %d\n", uffs_GetDeviceTotal(dev));
	printf("Disk Used:    %d\n", uffs_GetDeviceUsed(dev));
	printf("Disk Free:    %d\n", uffs_GetDeviceFree(dev));
}

static URET femu_releaseDevice(uffs_Device *dev)
{
	uffs_FileEmu *emu;

	uffs_Perror(UFFS_ERR_NORMAL, PFX "femu device release.\n");

	emu = (uffs_FileEmu *)(dev->attr->private);

	emu->initCount--;
	if (emu->initCount == 0) {
		if (emu->fp) {
			fclose(emu->fp);
			emu->fp = NULL;
		}

		femu_printStatistic(dev);
		memset(emu, 0, sizeof(uffs_FileEmu));

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
