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
static URET femu_WritePage(uffs_Device *dev, u32 block, u32 pageNum, const u8 *data, const u8 *spare);
static URET femu_WritePageData(uffs_Device *dev, u32 block, u32 pageNum, const u8 *page, int ofs, int len);
static URET femu_WritePageSpare(uffs_Device *dev, u32 block, u32 pageNum, const u8 *spare, int ofs, int len);
static URET femu_ReadPage(uffs_Device *dev, u32 block, u32 pageNum, u8 *data, u8 *spare);
static URET femu_ReadPageData(uffs_Device *dev, u32 block, u32 pageNum, u8 *page, int ofs, int len);
static URET femu_ReadPageSpare(uffs_Device *dev, u32 block, u32 pageNum, u8 *spare, int ofs, int len);
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
	
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;
	
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
	memset(emu->em_monitor_page, 0, blks * blkPgs);
	memset(emu->em_monitor_spare, 0, blks * blkPgs);

	emu->fp = fopen(emu->emu_filename, "rb");
	if (emu->fp == NULL) {
		emu->fp = fopen(emu->emu_filename, "ab+");
		if (emu->fp == NULL) {
			printf(PFX"Failed to create uffs emulation file.");
			return U_FAIL;
		}

		fseek(emu->fp, 0, SEEK_END);
		fSize = ftell(emu->fp);
		
		if (fSize < blkSize * blks)	{
			printf("Creating uffs emulation file\n");
			fseek(emu->fp, 0, SEEK_SET);
			memset(p, 0xff, pgdSize + spSize);
			for (i = 0; i < blkPgs * blks; i++)	{
				written = fwrite(p, 1, pgdSize + spSize, emu->fp);
				if (written != pgdSize + spSize)	{
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

static URET femu_WritePage(uffs_Device *dev, u32 block, u32 pageNum, const u8 *data, const u8 *spare)
{
	int ret;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;

	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;

	if (data) {
		ret = femu_WritePageData(dev, block, pageNum, data, 0, pgdSize);
		if(ret == U_FAIL)
			return ret;
	}
	
	if (spare) {
		ret = femu_WritePageSpare(dev, block, pageNum, spare, 0, spSize);
		if(ret == U_FAIL)
			return ret;
	}

	return U_SUCC;	

}

static URET femu_WritePageData(uffs_Device *dev, u32 block, u32 pageNum, const u8 *page, int ofs, int len)
{
	int written;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	uffs_FileEmu *emu;
	emu = (uffs_FileEmu *)(dev->attr->private);

	if (!emu || !(emu->fp))
		return U_FAIL;

	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;

	if (ofs + len > pgdSize) {
		printf("femu: write page data out of range!\n");
		return U_FAIL;
	}

	emu->em_monitor_page[block * blkPgs + pageNum]++;
	if (emu->em_monitor_page[block * blkPgs + pageNum] > MAXWRITETIME_PAGE) {
		printf("Warrning: block %d page %d exceed it's maximum write time!\r\n", block, pageNum);
		return U_FAIL;
	}
	
	if (page) {
		fseek(emu->fp, 
			(block * blkPgs + pageNum) * 
			(pgdSize + spSize) 
			+ ofs, SEEK_SET);
		written = fwrite(page, 1, len, emu->fp);
		
		if (written != len) {
			printf("femu: write page I/O error ?\n");
			return U_FAIL;
		}
	}

	dev->st.pageWriteCount++;
	fflush(emu->fp);
	
	return U_SUCC;
}

static URET femu_WritePageSpare(uffs_Device *dev, u32 block, u32 pageNum, const u8 *spare, int ofs, int len)
{
	int written;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp)) 
		return U_FAIL;

	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;

//	printf("WS: %d/%d, size %d\n", block, pageNum, len);
	
	if (ofs + len > spSize) {
		printf("femu: write page data out of range!\n");
		return U_FAIL;
	}

	emu->em_monitor_spare[block*blkPgs + pageNum]++;
	if (emu->em_monitor_spare[block*blkPgs + pageNum] > MAXWRITETIME_SPARE) {
		printf("Warrning: block %d page %d (spare) exceed it's maximum write time!\r\n", block, pageNum);
		return U_FAIL;
	}
	
	if (spare) {
		fseek(emu->fp, (block*blkPgs + pageNum) * (pgdSize + spSize) + dev->attr->page_data_size + ofs, SEEK_SET);
		written = fwrite(spare, 1, len, emu->fp);
		if (written != len) {
			printf("femu: write spare I/O error ?\n");
			return U_FAIL;
		}
	}

	fflush(emu->fp);
	dev->st.spareWriteCount++;

	return U_SUCC;
}


static URET femu_ReadPage(uffs_Device *dev, u32 block, u32 pageNum, u8 *data, u8 *spare)
{
	int ret;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;

	if (data) {
		ret = femu_ReadPageData(dev, block, pageNum, data, 0, pgdSize);
		if(ret == U_FAIL) 
			return U_FAIL;
	}
	
	if (spare) {
		ret = femu_ReadPageSpare(dev, block, pageNum, spare, 0, spSize);
		if(ret == U_FAIL)
			return U_FAIL;
	}

	return U_SUCC;	
}

static URET femu_ReadPageData(uffs_Device *dev, u32 block, u32 pageNum, u8 *page, int ofs, int len)
{
	int nread;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp))
		return U_FAIL;

	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;

	if(ofs + len > pgdSize) {
		printf("femu: read page data out of range!\n");
		return U_FAIL;
	}
	
	if(page) {
		fseek(emu->fp, (block*blkPgs + pageNum) * (pgdSize + spSize) + ofs, SEEK_SET);
		nread = fread(page, 1, len, emu->fp);

		// for ECC testing.
		if (0 && block == 100 && pageNum == 3) {
			printf("--- ECC error inject to block 100 page 3 ---\n");
			page[13] = (page[13] & ~0x40) | (~(page[13] & 0x40) & 0x40) ;
		}
		
		if (nread != len) {
			printf("femu: read page I/O error ?\n");
			return U_FAIL;
		}
	}

	dev->st.pageReadCount++;

	return U_SUCC;
}

static URET femu_ReadPageSpare(uffs_Device *dev, u32 block, u32 pageNum, u8 *spare, int ofs, int len)
{
	int nread;
	int pos;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp))
		return U_FAIL;

	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;
	
//	printf("RS: %d/%d, size %d\n", block, pageNum, len);

	if (ofs + len > spSize) {
		printf("femu: read page spare out of range!\n");
		return U_FAIL;
	}

	if (spare) {
		pos = (block*blkPgs + pageNum) * (pgdSize + spSize) + dev->attr->page_data_size + ofs;
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

	dev->st.spareReadCount++;
		
	return U_SUCC;
}



static URET femu_EraseBlock(uffs_Device *dev, u32 blockNumber)
{

	int i;
	u8 * pg = em_page_buf;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	uffs_FileEmu *emu;

	emu = (uffs_FileEmu *)(dev->attr->private);
	if (!emu || !(emu->fp))
		return U_FAIL;

	pgSize = dev->attr->page_data_size + dev->attr->spare_size;
	pgdSize = dev->attr->page_data_size;
	spSize = dev->attr->spare_size;
	blkPgs = dev->attr->pages_per_block;
	blks = dev->attr->total_blocks;
	blkSize = dev->attr->block_data_size;
	
	printf("femu: erase block %d\n", blockNumber);

	if ((int)blockNumber >= blks) {
		printf("Attempt to erase non-existant block %d\n",blockNumber);
		return U_FAIL;
	}
	else {
		//clear this block monitors
		memset(emu->em_monitor_page + (blockNumber * blkPgs), 
			0, 
			blkPgs * sizeof(u8));
		memset(emu->em_monitor_spare + (blockNumber * blkPgs),
			0,
			blkPgs * sizeof(u8));
		
		memset(pg, 0xff, (pgdSize + spSize));
		
		fseek(emu->fp, blockNumber * blkPgs * (pgdSize + spSize), SEEK_SET);
		
		for (i = 0; i < blkPgs; i++)	{
			fwrite(pg, 1, (pgdSize + spSize), emu->fp);
		}

		fflush(emu->fp);
		dev->st.blockEraseCount++;

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
	uffs_stat *s;
	s = &(dev->st);
	
	printf("-----------statistics-----------\n");
	printf("Block Erased: %d\n", s->blockEraseCount);
	printf("Write Page:   %d\n", s->pageWriteCount);
	printf("Write Spare:  %d\n", s->spareWriteCount);
	printf("Read Page:    %d\n", s->pageReadCount);
	printf("Read Spare:   %d\n", s->spareReadCount);
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
