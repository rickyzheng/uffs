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
 * \file uffs_nand_samsung.c
 * \brief samsung NAND flash specific file
 * \author Ricky Zheng, created 12th May, 2005
 */

#include "uffs/uffs_public.h"
#include "uffs/uffs_ecc.h"
#include "uffs/uffs_device.h"
#include <string.h>

#define PFX "samsung NAND: "


#define SAMSUNG_PAGE_SIZE_MAX	2048

#define SAMSUNG_SPARE_LENGTH_MAX	(16 * (SAMSUNG_PAGE_SIZE_MAX / 512))

/**
 * \struct uffs_SamsungSpareSt
 * \brief samsung NAND phicical format of page spare space
 */
struct uffs_SamsungSpareSt {
	u8 x[SAMSUNG_SPARE_LENGTH_MAX];
};

typedef struct uffs_SamsungSpareSt		uffs_SamsungSpare;		//NAND flash page spare


/** \brief load NAND spare data to "uffs_pageSpare"
 *  \param[in] dev uffs device
 *  \param[in] block block number to be loaded
 *  \param[in] page page number to be loaded
 *  \param[out] tag output for NAND page spare
 *  \note these function must be implemented in NAND specific file.
 */
static URET Samsung_LoadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag)
{
	uffs_SamsungSpare phi_spare;
	u8 * p;
	u8 * p_spare = (u8 *) &phi_spare;
	u32 ofs_end;
	int status_ofs = dev->attr->block_status_offs;
	uffs_Tags_8 tag_8;

	dev->ops->ReadPage(dev, block, page, NULL, p_spare);

	if (dev->attr->spare_size < 16)
		p = (u8 *)(&tag_8); // using small Tags
	else
		p = (u8 *)(tag);
	
	/** \note We must to work around the STATUS byte */
	memcpy(p, p_spare, status_ofs);
	p += status_ofs;
	p_spare += status_ofs;
	p_spare++;		//skip the status byte!

	if (dev->attr->spare_size < 16) {
		ofs_end = (u32)(&(((uffs_Tags_8 *)NULL)->blockStatus));
		memcpy(p, p_spare, ofs_end - status_ofs);
		uffs_TransferFromTag8(tag, &tag_8);
	}
	else {
		ofs_end = (u32)(&(((uffs_Tags *)NULL)->blockStatus));
		memcpy(p, p_spare, ofs_end - status_ofs);
	}

	tag->blockStatus = phi_spare.x[status_ofs]; 

	return U_SUCC;
}

/**
 * \brief write page spare space with given tag, all other spare content
 *         will be read from page spare before write to it.
 * \param[in] dev uffs device
 * \param[in] block block number to be wrote to
 * \param[in] page page number to be wrote to
 * \param[in] tag input tag
 * \note you must make sure that the block are erased, and page spare should not be wrote more then twice
 *		after block been erased once.
 */
static URET Samsung_WritePageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag)
{
	uffs_SamsungSpare phi_spare;
	u8 * p;
	u8 * p_spare = (u8 *) &phi_spare;
	u32 ofs_end;
	int status_ofs = dev->attr->block_status_offs;
	uffs_Tags_8 tag_8;
	
	dev->ops->ReadPage(dev, block, page, NULL, (u8 *)(&phi_spare));

	if(page == 0 || page == 1) {
		if(phi_spare.x[status_ofs] != 0xff) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"try to write to a bad block(%d) ? \n", block);
			return U_FAIL;
		}
	}

	if (dev->attr->spare_size < 16) {
		uffs_TransferToTag8(tag, &tag_8);
		p = (u8 *) (&tag_8);
	}
	else
		p = (u8 *) tag;

	memcpy(p_spare, p, status_ofs);	//copy bytes first
	p += status_ofs;
	p_spare += status_ofs;			
	p_spare++;										//skip the status byte

	if (dev->attr->spare_size < 16) {
		ofs_end = (u32)(&(((uffs_Tags_8 *)NULL)->blockStatus));
		memcpy(p_spare, p, ofs_end - status_ofs);  //the rest bytes
	}
	else {
		ofs_end = (u32)(&(((uffs_Tags *)NULL)->blockStatus));
		memcpy(p_spare, p, ofs_end - status_ofs);  //the rest bytes
	}

	return dev->ops->WritePage(dev, block, page, NULL, (u8 *)(&phi_spare));
}

/**
 * \brief make the given block.page valid 
 * \param[in] dev uffs device
 * \param[in] block block number
 * \param[in] page page number
 * \param[in] tag source tag of this page.
 */
static URET Samsung_MakePageValid(uffs_Device *dev, int block, int page, uffs_Tags *tag)
{
	//tag->valid = TAG_VALID; //0: valid, 1: invalid
	return Samsung_WritePageSpare(dev, block, page, tag);
}



static UBOOL Samsung_IsBlockBad(uffs_Device *dev, uffs_blockInfo *bc)
{
	uffs_LoadBlockInfo(dev, bc, 0);
#ifdef SAFE_CHECK_BAD_BLOCK_DOUBLE
	uffs_LoadBlockInfo(dev, bc, 1);
#endif
	if(bc->spares[0].tag.blockStatus != 0xff
#ifdef SAFE_CHECK_BAD_BLOCK_DOUBLE
		 || bc->spares[1].tag.blockStatus != 0xff
#endif
		 ) {
		return U_TRUE;
	}

	return U_FALSE;
}

static URET Samsung_MakeBadBlockMark(uffs_Device *dev, int block)
{
	u8 mark = 0;
	int status_ofs = dev->attr->block_status_offs;

	if (dev->ops->MarkBadBlock) {
		return dev->ops->MarkBadBlock(dev, block);
	}
	else {
		dev->ops->ReadPageSpare(dev, block, 0, &mark, status_ofs, 1);
		if(mark == 0xff){
			uffs_Perror(UFFS_ERR_NOISY, PFX"New bad block generated! %d\n", block);
			dev->ops->WritePageSpare(dev, block, 0, (u8 *)"1", status_ofs, 1);
		}
		return U_SUCC;
	}
}

static const struct uffs_FlashOpsSt Samsung_Flash_256 = {
	Samsung_LoadPageSpare,
	Samsung_WritePageSpare,
	Samsung_MakePageValid,
	uffs_GetEccSize256,
	uffs_MakeEcc256,
	uffs_EccCorrect256,
	Samsung_IsBlockBad,
	Samsung_MakeBadBlockMark
};


static const struct uffs_FlashOpsSt Samsung_Flash_512 = {
	Samsung_LoadPageSpare,
	Samsung_WritePageSpare,
	Samsung_MakePageValid,
	uffs_GetEccSize512,
	uffs_MakeEcc512,
	uffs_EccCorrect512,
	Samsung_IsBlockBad,
	Samsung_MakeBadBlockMark
};

static const struct uffs_FlashOpsSt Samsung_Flash_1K = {
	Samsung_LoadPageSpare,
	Samsung_WritePageSpare,
	Samsung_MakePageValid,
	uffs_GetEccSize1K,
	uffs_MakeEcc1K,
	uffs_EccCorrect1K,
	Samsung_IsBlockBad,
	Samsung_MakeBadBlockMark
};

static const struct uffs_FlashOpsSt Samsung_Flash_2K = {
	Samsung_LoadPageSpare,
	Samsung_WritePageSpare,
	Samsung_MakePageValid,
	uffs_GetEccSize2K,
	uffs_MakeEcc2K,
	uffs_EccCorrect2K,
	Samsung_IsBlockBad,
	Samsung_MakeBadBlockMark
};
 
static URET Samsung_InitClass(uffs_Device *dev, int id)
{
	id = id;
	if (dev->attr->page_data_size == 256) {
		dev->flash = &Samsung_Flash_256;
	}
	else if (dev->attr->page_data_size == 512) {
		dev->flash = &Samsung_Flash_512;
	}
	else if(dev->attr->page_data_size == 1024) {
		dev->flash = &Samsung_Flash_1K;
	}
	else if(dev->attr->page_data_size == 2048) {
		dev->flash = &Samsung_Flash_2K;
	}
	else {
		dev->flash = NULL;
		return U_FAIL;
	}

	return U_SUCC;
}

/** \note the valid id list must be terminated by '-1' */
//static int valid_id_list[] = {0xe3, 0xe5, 0xe6, 0x73, 0x75, 0x76, 0x79, 0x35, 0x36, -1};


struct uffs_FlashClassSt Samsung_FlashClass = {
	"Samsung NAND",
	MAN_ID_SAMSUNG,		/* Samsung Manufacture ID: 0xEC */
	NULL,				/* don't care the device id */
	NULL,				/* no static ops */
	Samsung_InitClass,	
};


