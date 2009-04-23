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
 * \file uffs_nand_simram.c
 * \brief Use RAM simulating NAND flash. To use RAM more efficent, we get rid of ECC.
 * \author Ricky Zheng, created 19 Dec, 2007
 */

#include "uffs/uffs_public.h"
#include "uffs/uffs_ecc.h"
#include <string.h>

#define PFX "SIMRAM: "

#define SIMRAM_SPARE_SIZE_MAX		16	// should be big enough to fit uffs_Tags

/**
 * \struct uffs_SimramSpareSt
 * \brief simulate RAM phicical format of page spare space
 */
struct uffs_SimramSpareSt {
	u8 x[SIMRAM_SPARE_SIZE_MAX];	
};

typedef struct uffs_SimramSpareSt		uffs_SimramSpare;		//NAND flash page spare



/** \brief load NAND spare data to "uffs_pageSpare"
 *  \param[in] dev uffs device
 *  \param[in] block block number to be loaded
 *  \param[in] page page number to be loaded
 *  \param[out] tag output for NAND page spare
 *  \note these function must be implemented in NAND specific file.
 */
static URET Simram_LoadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag)
{
	uffs_SimramSpare phi_spare;
	u8 * p = (u8 *)(tag);
	u8 * p_spare = (u8 *) phi_spare.x;
	u32 ofs_end;
	int status_ofs = dev->attr->block_status_offs;

	dev->ops->ReadPage(dev, block, page, NULL, p_spare);

	ofs_end = (u32)(&(((uffs_Tags *)NULL)->blockStatus));
	memcpy(p, p_spare, ofs_end);
	tag->blockStatus = 0xFF;

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
static URET Simram_WritePageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag)
{
	uffs_SimramSpare phi_spare;
	u8 * p = (u8 *)tag;
	u8 * p_spare = (u8 *) phi_spare.x;
	u32 ofs_end;

	memset(p_spare, 0xFF, dev->attr->spare_size);
	ofs_end = (u32)(&(((uffs_Tags *)NULL)->blockStatus));
	memcpy(p_spare, p, ofs_end);  //the rest bytes

	return dev->ops->WritePage(dev, block, page, NULL, (u8 *)(&phi_spare));
}

/**
 * \brief make the given block.page valid 
 * \param[in] dev uffs device
 * \param[in] block block number
 * \param[in] page page number
 * \param[in] tag source tag of this page.
 */
static URET Simram_MakePageValid(uffs_Device *dev, int block, int page, uffs_Tags *tag)
{
	//tag->valid = TAG_VALID; //0: valid, 1: invalid
	return Simram_WritePageSpare(dev, block, page, tag);
}


static UBOOL Simram_IsBlockBad(uffs_Device *dev, uffs_blockInfo *bc)
{
	bc = bc;
	dev = dev;

	// always good :)
	return U_FALSE;
}

static URET Simram_MakeBadBlockMark(uffs_Device *dev, int block)
{
	dev = dev;
	block = block;

	uffs_Perror(UFFS_ERR_NOISY, PFX"It's no sense to generate a bad block for sim RAM.\n");
	return U_SUCC;
}

static int Simram_GetEccSize(uffs_Device *dev)
{
	dev = dev;
	// no ECC needed.
	return 0;
}

static void Simram_MakeEcc(uffs_Device *dev, void *data, void *ecc)
{
	dev = dev;
	data = data;
	ecc = ecc;
	// do nothing
	return;
}

static int Simram_EccCorrect(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc)
{
	dev = dev; data = data;	read_ecc = read_ecc; test_ecc = test_ecc;
	// always good :)
	return 0;
}


static const struct uffs_FlashOpsSt Simram_Flash = {
	Simram_LoadPageSpare,
	Simram_WritePageSpare,
	Simram_MakePageValid,
	Simram_GetEccSize,
	Simram_MakeEcc,
	Simram_EccCorrect,
	Simram_IsBlockBad,
	Simram_MakeBadBlockMark
};


static URET Simram_InitClass(uffs_Device *dev, int id)
{
	id = id;

	if (dev->attr->spare_size > SIMRAM_SPARE_SIZE_MAX) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Spare size too large: %d, should <= %d\r\n", dev->attr->spare_size, SIMRAM_SPARE_SIZE_MAX);
		return U_FAIL;
	}

	dev->flash = &Simram_Flash;

	return U_SUCC;
}


struct uffs_FlashClassSt Simram_FlashClass = {
	"Sim RAM",
	MAN_ID_SIMRAM,		/* Simulate RAM ID: 0xFF */
	NULL,				/* don't care the device id */
	NULL,				/* no static ops */
	Simram_InitClass,	
};


