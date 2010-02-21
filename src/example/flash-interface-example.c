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
 * \file flash-interface-example.c
 * \brief example for using flash driver and multiple partitions, with static memory allocator.
 * \author Ricky Zheng, created at 27 Nov, 2007
 */
  

#include "uffs/uffs_device.h"
#include "uffs/uffs_flash.h"

#define PFX "nand-drv:"


static int nand_write_page_data(uffs_Device *dev, u32 block, u32 pageNum, const u8 *page, int len, u8 *ecc, UBOOL commit);
static int nand_write_page_spare(uffs_Device *dev, u32 block, u32 pageNum, const u8 *spare, int ofs, int len, UBOOL commit);

static int nand_read_page_data(uffs_Device *dev, u32 block, u32 pageNum, u8 *page, int len, u8 *ecc);
static int nand_read_page_spare(uffs_Device *dev, u32 block, u32 pageNum, u8 *spare, int ofs, int len);

static int nand_erase_block(uffs_Device *dev, u32 blockNumber);

static URET nand_init_device(uffs_Device *dev);



static int nand_write_page_data(uffs_Device *dev, u32 block, u32 pageNum, const u8 *page, int len, u8 *ecc, UBOOL commit)
{
  // insert your nand driver codes here ...
	
	dev->st.pageWriteCount++;
	return UFFS_FLASH_NO_ERR;
}


static int nand_write_page_spare(uffs_Device *dev, u32 block, u32 pageNum, const u8 *spare, int ofs, int len, UBOOL commit)
{
  // insert your nand driver codes here ...
  
  dev->st.spareWriteCount++;  
	return UFFS_FLASH_NO_ERR;
}

static int nand_read_page_data(uffs_Device *dev, u32 block, u32 pageNum, u8 *page, int len, u8 *ecc)
{
  // insert your nand driver codes here ...

	dev->st.pageReadCount++;
	return UFFS_FLASH_NO_ERR;
}

static int nand_read_page_spare(uffs_Device *dev, u32 block, u32 pageNum, u8 *spare, int ofs, int len)
{
  // insert your nand driver codes here ...

	dev->st.spareReadCount++;		
	return UFFS_FLASH_NO_ERR;
}


static int nand_erase_block(uffs_Device *dev, u32 blockNumber)
{
  // insert your nand driver codes here ...
  
	dev->st.blockEraseCount++;
	return UFFS_FLASH_NO_ERR;
}


/////////////////////////////////////////////////////////////////////////////////

static uffs_FlashOpsSt my_nand_driver_ops = {
	nand_read_page_data,    //ReadPageData
	nand_read_page_spare,   //ReadPageSpare
    NULL,                   //ReadPageSpareWithLayout
    nand_write_page_data,   //WritePageData
    nand_write_page_spare,  //WritePageSpare
    NULL,                   //WritePageSpareWithLayout
    NULL,                   //IsBadBlock
    NULL,                   //MarkBadBlock
    nand_erase_block,       //EraseBlock
};

// change these parameters to fit your nand flash specification
#define MAN_ID          MAN_ID_SAMSUNG  // simulate Samsung's NAND flash

#define TOTAL_BLOCKS    1024
#define PAGE_DATA_SIZE  512
#define PAGE_SPARE_SIZE 16
#define PAGES_PER_BLOCK 32
#define PAGE_SIZE		(PAGE_DATA_SIZE + PAGE_SPARE_SIZE)
#define BLOCK_DATA_SIZE (PAGE_DATA_SIZE * PAGES_PER_BLOCK)

#define NR_PARTITION	2								/* total partitions */
#define PAR_1_BLOCKS	100								/* partition 1 */
#define PAR_2_BLOCKS	(TOTAL_BLOCKS - PAR_1_BLOCKS)	/* partition 2 */

static struct uffs_storageAttrSt flash_storage = {0};

/* static alloc the memory for each partition */

static int static_buffer_par1[UFFS_STATIC_BUFF_SIZE(PAGES_PER_BLOCK, PAGE_SIZE, PAR_1_BLOCKS) / sizeof(int)];
static int static_buffer_par2[UFFS_STATIC_BUFF_SIZE(PAGES_PER_BLOCK, PAGE_SIZE, PAR_2_BLOCKS) / sizeof(int)];;

/* init memory allocator, setup buffer sizes */
static URET static_mem_alloc_init_par_1(struct uffs_DeviceSt *dev)
{
  struct uffs_memAllocatorSt *mem = &dev->mem;
  mem->buf_start = static_buffer_par1;
  mem->buf_size = sizeof(static_buffer_par1);
  mem->pos = 0;

  uffs_Perror(UFFS_ERR_NOISY, PFX"Total static memory: %d bytes\n", mem->buf_size);

  return U_SUCC;
}

/* init memory allocator, setup buffer sizes */
static URET static_mem_alloc_init_par_2(struct uffs_DeviceSt *dev)
{
  struct uffs_memAllocatorSt *mem = &dev->mem;
  mem->buf_start = static_buffer_par2;
  mem->buf_size = sizeof(static_buffer_par2);
  mem->pos = 0;

  uffs_Perror(UFFS_ERR_NOISY, PFX"Total static memory: %d bytes\n", mem->buf_size);

  return U_SUCC;
}
  
/* allocate memory (for dynamic memory allocation) */
static void * static_mem_alloc_malloc(struct uffs_DeviceSt *dev, unsigned int size)
{
  struct uffs_memAllocatorSt *mem = &dev->mem;
  void *p = NULL;

  if (mem->buf_size - mem->pos < size) {
    uffs_Perror(UFFS_ERR_SERIOUS, PFX"Memory alloc failed! (alloc %d, free %d)\n", size, mem->buf_size - mem->pos);
  }
  else {
    p = mem->buf_start + size;
    mem->pos += size;
    uffs_Perror(UFFS_ERR_NOISY, PFX"Allocated %d, free %d\n", size, mem->buf_size - mem->pos);
  }

  return p;
}


static void setup_flash_storage(struct uffs_storageAttrSt *attr)
{
	attr->total_blocks = TOTAL_BLOCKS;			/* total blocks */
	attr->page_data_size = PAGE_DATA_SIZE;		/* page data size */
	attr->pages_per_block = PAGES_PER_BLOCK;	/* pages per block */
	attr->spare_size = PAGE_SPARE_SIZE;		  	/* page spare size */
	attr->block_status_offs = 4;				/* block status offset is 5th byte in spare */
    attr->ecc_opt = UFFS_ECC_SOFT;              /* ecc option */
    attr->layout_opt = UFFS_LAYOUT_UFFS;        /* let UFFS do the spare layout */    
}


static URET my_initDevice(uffs_Device *dev)
{
	dev->ops = &my_nand_driver_ops;
    
	return U_SUCC;
}

static URET my_releaseDevice(uffs_Device *dev)
{
	return U_SUCC;
}

/* define mount table */
static uffs_Device demo_device_1 = {0};
static uffs_Device demo_device_2 = {0};
static struct uffs_mountTableSt demo_mount_table[] = {
	{ &demo_device_1,  0, PAR_1_BLOCKS - 1, "/data/" },
	{ &demo_device_2,  PAR_1_BLOCKS, PAR_2_BLOCKS - 1, "/" },
	{ NULL, NULL, 0, 0 }
};

static int my_init_filesystem(void)
{
	struct uffs_mountTableSt *mtbl = &(demo_mount_table[0]);
	struct uffs_memAllocatorSt *mem;

	/* setup nand storage attributes */
	setup_flash_storage(&flash_storage);

	/* setup memory allocator */
	mem = &demo_device_1.mem;
	memset(mem, 0, sizeof(struct uffs_memAllocatorSt));
	mem->init = static_mem_alloc_init_par_1;
	mem->malloc = static_mem_alloc_malloc;

	mem = &demo_device_2.mem;
	memset(mem, 0, sizeof(struct uffs_memAllocatorSt));
	mem->init = static_mem_alloc_init_par_2;
	mem->malloc = static_mem_alloc_malloc;

	/* register mount table */
	while(mtbl->dev) {
		mtbl->dev->Init = my_initDevice;
		mtbl->dev->Release = my_releaseDevice;
		mtbl->dev->attr = &flash_storage;
		uffs_RegisterMountTable(mtbl);
		mtbl++;
	}
	
	return uffs_initMountTable() == U_SUCC ? 0 : -1;
}

/* application entry */
int my_application_main_entry()
{
  my_init_filesystem();
  uffs_InitObjectBuf();
  
  // ... my application codes ....
  // read/write/create/delete files ...
  
  uffs_ReleaseObjectBuf();
  uffs_releaseMountTable();
  
  return 0;
}


/////////////////////////////////////////////////////////////////////////////////
