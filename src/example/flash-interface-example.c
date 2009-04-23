/*
    Copyright (C) 2005-2008  Ricky Zheng <ricky_gz_zheng@yahoo.co.nz>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/
/**
 * \file flash-interface-example.c
 * \brief example for using flash driver and multiple partitions, with static memory allocator.
 * \author Ricky Zheng, created 27 Nov, 2007
 */
  

#include "uffs/uffs_device.h"

#define PFX "nand-drv:"


static URET nand_write_page(uffs_Device *dev, u32 block, u32 pageNum, const u8 *data, const u8 *spare);
static URET nand_write_page_data(uffs_Device *dev, u32 block, u32 pageNum, const u8 *page, int ofs, int len);
static URET nand_write_page_spare(uffs_Device *dev, u32 block, u32 pageNum, const u8 *spare, int ofs, int len);
static URET nand_read_page(uffs_Device *dev, u32 block, u32 pageNum, u8 *data, u8 *spare);
static URET nand_read_page_data(uffs_Device *dev, u32 block, u32 pageNum, u8 *page, int ofs, int len);
static URET nand_read_page_spare(uffs_Device *dev, u32 block, u32 pageNum, u8 *spare, int ofs, int len);
static URET nand_erase_block(uffs_Device *dev, u32 blockNumber);
static URET nand_reset_flash(uffs_Device *dev);
static UBOOL nand_block_is_bad(uffs_Device *dev, u32 block);
static URET nand_mark_bad_block(uffs_Device *dev, u32 block);
static URET nand_init_device(uffs_Device *dev);


static URET nand_write_page(uffs_Device *dev, u32 block, u32 pageNum, const u8 *data, const u8 *spare)
{
	int ret;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;

	pgSize = dev->attr.page_data_size + dev->attr.spare_size;
	pgdSize = dev->attr.page_data_size;
	spSize = dev->attr.spare_size;
	blkPgs = dev->attr.pages_per_block;
	blks = dev->attr.total_blocks;
	blkSize = dev->attr.block_data_size;

	if(data) {
		ret = nand_write_page_data(dev, block, pageNum, data, 0, pgdSize);
		if(ret == U_FAIL) return ret;
	}
	
	if(spare) {
		ret = nand_write_page_spare(dev, block, pageNum, spare, 0, spSize);
		if(ret == U_FAIL) return ret;
	}

	return U_SUCC;	

}

static URET nand_write_page_data(uffs_Device *dev, u32 block, u32 pageNum, const u8 *page, int ofs, int len)
{
  // insert your nand driver codes here ...
	
	dev->st.pageWriteCount++;
	return U_SUCC;
}


static URET nand_write_page_spare(uffs_Device *dev, u32 block, u32 pageNum, const u8 *spare, int ofs, int len)
{
  // insert your nand driver codes here ...
  
  dev->st.spareWriteCount++;  
	return U_SUCC;
}


static URET nand_read_page(uffs_Device *dev, u32 block, u32 pageNum, u8 *data, u8 *spare)
{
	int ret;
	int pgSize, pgdSize, spSize, blks, blkPgs, blkSize;
	pgSize = dev->attr.page_data_size + dev->attr.spare_size;
	pgdSize = dev->attr.page_data_size;
	spSize = dev->attr.spare_size;
	blkPgs = dev->attr.pages_per_block;
	blks = dev->attr.total_blocks;
	blkSize = dev->attr.block_data_size;

	if(data)
	{
		ret = nand_read_page_data(dev, block, pageNum, data, 0, pgdSize);
		if(ret == U_FAIL) return U_FAIL;
	}
	
	if(spare)
	{
		ret = nand_read_page_spare(dev, block, pageNum, spare, 0, spSize);
		if(ret == U_FAIL) return U_FAIL;
	}

	return U_SUCC;	
}

static URET nand_read_page_data(uffs_Device *dev, u32 block, u32 pageNum, u8 *page, int ofs, int len)
{
  // insert your nand driver codes here ...

	dev->st.pageReadCount++;
	return U_SUCC;
}

static URET nand_read_page_spare(uffs_Device *dev, u32 block, u32 pageNum, u8 *spare, int ofs, int len)
{
  // insert your nand driver codes here ...

	dev->st.spareReadCount++;		
	return U_SUCC;
}



static URET nand_erase_block(uffs_Device *dev, u32 blockNumber)
{
  // insert your nand driver codes here ...
  
	dev->st.blockEraseCount++;
	return U_SUCC;
}

static URET nand_reset_flash(uffs_Device *dev)
{
  // leave it empty ...
  
	return U_SUCC;
}

static UBOOL nand_block_is_bad(uffs_Device *dev, u32 block)
{
	// Here is the example implementation for checking bad block.
	// If nand flash page size is not 512, the block status byte is 
	// not the 5th byte of page spare, you should change it according
	// your nand flash data sheet.

	unsigned char mark;
	mark = 0;
	nand_read_page_spare(dev, block, 0, &mark, 5, 1);
	if (mark == 0xff) {
		nand_read_page_spare(dev, block, 1, &mark, 5, 1);
		if (mark == 0xff) {
			return U_FALSE;
		}
	}

	return U_TRUE;
}

static URET nand_mark_bad_block(uffs_Device *dev, u32 block)
{
  // insert your nand driver codes here ...
  // mark a bad block, for example, by writing 0x00 to 5th byte of first page spare ...
  
	return U_SUCC;
}

/////////////////////////////////////////////////////////////////////////////////

static uffs_DevOps my_nand_device_ops = {
	nand_reset_flash,
	nand_block_is_bad,
	nand_mark_bad_block,
	nand_erase_block,
	nand_write_page,
	nand_write_page_data,
	nand_write_page_spare,
	nand_read_page,
	nand_read_page_data,
	nand_read_page_spare,
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

static int static_buffer_par1[(UFFS_BLOCK_INFO_BUFFER_SIZE(PAGES_PER_BLOCK) + UFFS_PAGE_BUFFER_SIZE(PAGE_SIZE) + UFFS_TREE_BUFFER_SIZE(PAR_1_BLOCKS) + PAGE_SIZE) / sizeof(int)];
static int static_buffer_par2[(UFFS_BLOCK_INFO_BUFFER_SIZE(PAGES_PER_BLOCK) + UFFS_PAGE_BUFFER_SIZE(PAGE_SIZE) + UFFS_TREE_BUFFER_SIZE(PAR_2_BLOCKS) + PAGE_SIZE) / sizeof(int)];

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
	attr->dev_type =	UFFS_DEV_EMU;			/* dev_type */
	attr->maker = MAN_ID;			        	/* simulate manufacture ID */
	attr->id = 0xe3;							/* chip id, can be ignored. */
	attr->total_blocks = TOTAL_BLOCKS;			/* total blocks */
	attr->block_data_size = BLOCK_DATA_SIZE;	/* block data size */
	attr->page_data_size = PAGE_DATA_SIZE;		/* page data size */
	attr->spare_size = PAGE_SPARE_SIZE;		  	/* page spare size */
	attr->pages_per_block = PAGES_PER_BLOCK;	/* pages per block */
	attr->block_status_offs = 5;				/* block status offset is 5th byte in spare */
}


static URET my_initDevice(uffs_Device *dev)
{
	dev->ops = &my_nand_device_ops;		
	return U_SUCC;
}

static URET my_releaseDevice(uffs_Device *dev)
{
	return U_SUCC;
}

/* define devices and mount table */
static uffs_Device demo_device_1 = {0};
static uffs_Device demo_device_2 = {0};
static struct uffs_mountTableSt demo_mount_table[] = {
	{ &demo_device_1,  0, PAR_1_BLOCKS - 1, "/data/" },
	{ &demo_device_2,  0, PAR_1_BLOCKS - 1, "/" },
	{ NULL, NULL, 0, 0 }
};


static int my_init_filesystem(void)
{
	struct uffs_mountTableSt *mtbl = &(demo_mount_table[0]);
	struct uffs_memAllocatorSt *mem;

	/* setup emu storage */
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
