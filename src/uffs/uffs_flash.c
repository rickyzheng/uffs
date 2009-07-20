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
 * \file uffs_flash.c
 * \brief UFFS flash interface
 * \author Ricky Zheng, created 17th July, 2009
 */

#include "uffs/uffs_public.h"
#include "uffs/uffs_ecc.h"
#include "uffs/uffs_flash.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_badblock.h"
#include <string.h>

#define PFX "Flash: "


#define ECC_SIZE(dev) (3 * (dev)->attr->page_data_size / 256)
#define TAG_STORE_SIZE	(sizeof(u16) + (int)(&((struct uffs_TagsSt *)0)->tag_ecc))

// P256: [D0, D1, D2, D3, ECC1, S, ECC2, ECC3]
static const u8 P256_sdata_layout[] = {0, 4, 0xFF, 0};
static const u8 P256_ecc_layout[] = {4, 1, 6, 2, 0xFF, 0};
static const u8 P256_s_ecc_layout[] = {0xFF, 0};

// P512: [D0, D1, D2, D3, D4, S, D5, SECC1, SECC2, ECC1, ECC2, ECC3, ECC4, ECC5, ECC6, X]
static const u8 P512_sdata_layout[] = {0, 5, 6, 1, 0xFF, 0};
static const u8 P512_ecc_layout[] = {9, 6, 0xFF, 0};
static const u8 P512_s_ecc_layout[] = {7, 2, 0xFF, 0};

// P1K: [D0, D1, D2, D3, D4, S, D5, SECC1, SECC2, ECC1, ..., ECC12, X, ...]
static const u8 P1K_sdata_layout[] = {0, 5, 6, 1, 0xFF, 0};
static const u8 P1K_ecc_layout[] = {9, 12, 0xFF, 0};
static const u8 P1K_s_ecc_layout[] = {7, 2, 0xFF, 0};

// P2K: [D0, D1, D2, D3, D4, S, D5, SECC1, SECC2, ECC1, ..., ECC24, X, ...]
static const u8 P2K_sdata_layout[] = {0, 5, 6, 1, 0xFF, 0};
static const u8 P2K_ecc_layout[] = {9, 24, 0xFF, 0};
static const u8 P2K_s_ecc_layout[] = {7, 2, 0xFF, 0};

static const u8 * layout_sel_tbl[3][3] = {
	{P256_sdata_layout, P256_ecc_layout, P256_s_ecc_layout},
	{P512_sdata_layout, P512_ecc_layout, P512_s_ecc_layout},
	{P1K_sdata_layout, P1K_ecc_layout, P1K_s_ecc_layout},
	{P2K_sdata_layout, P2K_ecc_layout, P2K_s_ecc_layout},
};



static int _calculate_spare_buf_size(uffs_Device *dev)
{
	const u8 *p;
	int ecc_last = 0, tag_last = 0;
	int ecc_size, tag_size;
	int n;

	ecc_size = ECC_SIZE(dev);
	
	p = dev->attr->ecc_layout;
	if (p) {
		while (*p != 0xFF && ecc_size > 0) {
			n = (p[1] > ecc_size ? ecc_size : p[1]);
			ecc_last = p[0] + n;
			ecc_size -= n;
			p += 2;
		}
	}

	tag_size = TAG_STORE_SIZE;
	p = dev->attr->data_layout;
	if (p) {
		while (*p != 0xFF && tag_size > 0) {
			n = (p[1] > tag_size ? tag_size : p[1]);
			tag_last = p[0] + n;
			tag_size -= n;
			p += 2;
		}
	}

	n = (ecc_last > tag_last ? ecc_last : tag_last);
	n = (n > dev->attr->block_status_offs + 1 ? n : dev->attr->block_status_offs + 1);

	return n;
}

/**
 * Initialize UFFS flash interface
 */
URET uffs_FlashInterfaceInit(uffs_Device *dev)
{
	struct uffs_StorageAttrSt *attr = dev->attr;
	int idx;
	const u8 idx_tbl[] = {0, 1, 2, 3, 3};

	dev->mem.spare_buffer_size = _calculate_spare_buf_size(dev);

	idx = idx_tbl[(attr->page_data_size / 256) >> 1];

	if (attr->ecc_layout == NULL)
		attr->ecc_layout = layout_sel_tbl[idx][0];
	if (attr->data_layout == NULL)
		attr->data_layout = layout_sel_tbl[idx][1];
	if (attr->s_ecc_layout == NULL)
		attr->s_ecc_layout = layout_sel_tbl[idx][2];

	return U_SUCC;
}

/**
 * unload spare to tag and ecc.
 */
static void _UnloadSpare(uffs_Device *dev, const u8 *spare, uffs_Tags *tag, u8 *ecc)
{
	u8 *p_tag = (u8 *)tag;
	int tag_size = TAG_STORE_SIZE;
	int ecc_size = ECC_SIZE(dev);
	int n;
	const u8 *p;

	// unload ecc
	p = dev->attr->ecc_layout;
	if (p && ecc) {
		while (*p != 0xFF && ecc_size > 0) {
			n = (p[1] > ecc_size ? ecc_size : p[1]);
			memcpy(ecc, spare + p[0], n);
			ecc_size -= n;
			ecc += n;
			p += 2;
		}
	}

	// unload tag
	if (tag) {
		p = dev->attr->data_layout;
		while (*p != 0xFF && tag_size > 0) {
			n = (p[1] > tag_size ? tag_size : p[1]);
			memcpy(p_tag, spare + p[1], n);
			tag_size -= n;
			p_tag += n;
			p += 2;
		}

		tag->block_status = spare[dev->attr->block_status_offs];
	}
}

/**
 * Read page spare, fill tag and ECC
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[out] tag
 * \param[out] ecc
 */
URET uffs_FlashReadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag, u8 *ecc)
{
	uffs_FlashOps *ops = dev->ops;
	struct uffs_StorageAttrSt *attr = dev->attr;
	u8 * spare_buf = dev->mem.spare_buffer;
	u8 * p_tag;
	u16 tag_ecc;
	int ret = 0;
	UBOOL is_bad = U_FALSE;

	if (attr->layout_opt == UFFS_LAYOUT_FLASH)
		ret = ops->ReadPageSpareLayout(dev, block, page, (u8 *)tag, TAG_STORE_SIZE, ecc);
	else
		ret = ops->ReadPageSpare(dev, block, page, spare_buf, dev->mem.spare_buffer_size);

	if (ret == -1) 
		goto ext;
	else if (ret == -2) {
		is_bad = U_TRUE;
		goto ext;
	}
	else if (ret > 0) {
		is_bad = U_TRUE;
	}

	if (attr->layout_opt != UFFS_LAYOUT_FLASH)
		_UnloadSpare(dev, spare_buf, tag, ecc);

	// check tag ecc
	if (dev->attr->ecc_opt != UFFS_ECC_NONE) {
		p_tag = (u8 *)tag;
		tag_ecc = uffs_MakeEcc8(p_tag, TAG_STORE_SIZE - sizeof(tag_ecc));
		ret = uffs_EccCorrect8(p_tag, tag->tag_ecc, tag_ecc, TAG_STORE_SIZE - sizeof(tag_ecc));
		if (ret < 0) {
			is_bad = U_TRUE;
			ret = -2;
			goto ext;
		}
		else if (ret > 0)
			is_bad = U_TRUE;
	}

ext:
	if (is_bad) {
		uffs_BadBlockAdd(dev, block);
		uffs_Perror(UFFS_ERR_NORMAL, PFX"A new bad block (%d) is detected.\n", block);
	}

	if (ret >= 0)
		return U_SUCC;

	if (ret == -1)
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"I/O error.\n");

	return U_FAIL;
}

/**
 * Read page data to page buf and calculate ecc.
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[out] buf
 */
URET uffs_FlashReadPage(uffs_Device *dev, int block, int page, uffs_Buf *buf)
{
	uffs_FlashOps *ops = dev->ops;
	int size = dev->attr->page_data_size;
	u8 ecc_buf[MAX_ECC_SIZE];
	u8 ecc_store[MAX_ECC_SIZE];
	UBOOL is_bad = U_FALSE;

	int ret;

	ret = ops->ReadPageData(dev, block, page, buf->start, size, ecc_buf);

	if (ret == -1)
		goto ext;
	else if (ret == -2) {
		is_bad = U_TRUE;
		goto ext;
	}
	else if (ret > 0)
		is_bad = U_TRUE;


	if (dev->attr->ecc_opt == UFFS_ECC_SOFT)
		uffs_MakeEcc(buf->start, size, ecc_buf);

	if (dev->attr->ecc_opt == UFFS_ECC_SOFT ||
		dev->attr->ecc_opt == UFFS_ECC_HW) {
		ret = uffs_EccCorrect(buf->start, size, ecc_store, ecc_buf);
	}

	if (dev->attr->ecc_opt != UFFS_ECC_NONE &&
		dev->attr->ecc_opt != UFFS_ECC_HW) {

		if (uffs_FlashReadPageSpare(dev, block, page, NULL, ecc_store) == U_FAIL) {
			ret = -1;
			goto ext;
		}

		ret = uffs_EccCorrect(buf->start, size, ecc_store, ecc_buf);

		if (ret < 0) {
			is_bad = U_TRUE;
			ret = -2;
		}
		else if (ret > 0) {
			uffs_BadBlockAdd(dev, block);
		}
	}

ext:
	if (is_bad)
		uffs_BadBlockAdd(dev, block);

	if (ret >= 0) {
		buf->data_len = buf->start[0] | (buf->start[1] << 8);
		buf->check_sum = buf->start[2] | (buf->start[3] << 8);
	}

	return (ret >= 0 ? U_SUCC : U_FAIL);
}

/**
 * make spare from tag and ecc
 */
static void _MakeSpare(uffs_Device *dev, uffs_Tags *tag, u8 *ecc, u8* spare)
{
	u8 *p_tag = (u8 *)tag;
	int tag_size = TAG_STORE_SIZE;
	int ecc_size = ECC_SIZE(dev);
	int n;
	const u8 *p;

	memset(spare, 0xFF, dev->mem.spare_buffer_size);	// initialize as 0xFF.

	// load ecc
	p = dev->attr->ecc_layout;
	if (p && ecc) {
		while (*p != 0xFF && ecc_size > 0) {
			n = (p[1] > ecc_size ? ecc_size : p[1]);
			memcpy(spare + p[0], ecc, n);
			ecc_size -= n;
			ecc += n;
			p += 2;
		}
	}

	// load tag
	if (dev->attr->ecc_opt == UFFS_ECC_NONE) {
		tag->tag_ecc = 0xFFFF;
	}
	else {
		tag->tag_ecc = uffs_MakeEcc8(p_tag, tag_size - sizeof(tag->tag_ecc));
	}
	p = dev->attr->data_layout;
	while (*p != 0xFF && tag_size > 0) {
		n = (p[1] > tag_size ? tag_size : p[1]);
		memcpy(p_tag, spare + p[1], n);
		tag_size -= n;
		p_tag += n;
		p += 2;
	}
}

/**
 * write the whole page, include data and spare
 *
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[in] buf contains data to be wrote
 * \param[in] tag tag to be wrote
 *
 * \note if ecc passed as NULL, we'll calculate it, or by flash driver.
 */
URET uffs_FlashWritePageCombine(uffs_Device *dev, int block, int page, uffs_Buf *buf, uffs_Tags *tag)
{
	uffs_FlashOps *ops = dev->ops;
	int size = dev->attr->page_data_size;
	u8 ecc_buf[MAX_ECC_SIZE];
	u8 *spare = dev->mem.spare_buffer;
	int ret;
	UBOOL is_bad = U_FALSE;

	if (dev->attr->ecc_opt == UFFS_ECC_SOFT)
		uffs_MakeEcc(buf->start, size, ecc_buf);

	ret = ops->WritePageData(dev, block, page, buf->start, size, ecc_buf);

	if (ret == -1)
		goto ext;
	else if (ret == -2) {
		is_bad = U_TRUE;
		goto ext;
	}
	else if (ret > 0)
		is_bad = U_TRUE;

	if (dev->attr->ecc_opt != UFFS_ECC_NONE)
		tag->tag_ecc = uffs_MakeEcc8(tag, TAG_STORE_SIZE);

	tag->dirty = 0;
	tag->valid = 0;

	if (dev->attr->ecc_opt == UFFS_ECC_SOFT ||
		dev->attr->ecc_opt == UFFS_ECC_HW)
		_MakeSpare(dev, tag, ecc_buf, spare);
	else
		_MakeSpare(dev, tag, NULL, spare);

	ret = ops->WritePageSpare(dev, block, page, spare, dev->mem.spare_buffer_size);

	if (ret == -1)
		goto ext;
	else if (ret == -2) {
		is_bad = U_TRUE;
		goto ext;
	}
	else if (ret > 0)
		is_bad = U_TRUE;

ext:
	if (is_bad)
		uffs_BadBlockAdd(dev, block);

	return (ret >= 0 ? U_SUCC : U_FAIL);
}

/** Mark this block as bad block */
URET uffs_FlashMarkBadBlock(uffs_Device *dev, int block)
{
	// for now, just call flash driver's function

	return dev->ops->MarkBadBlock(dev, block) == 0 ? U_SUCC : U_FAIL;
}

/** Is this block a bad block ? */
UBOOL uffs_FlashIsBadBlock(uffs_Device *dev, int block)
{
	uffs_Tags tag;

	if (dev->ops->IsBadBlock)
		return dev->ops->IsBadBlock(dev, block) == 1 ? U_TRUE : U_FALSE;

	if (dev->attr->layout_opt == UFFS_LAYOUT_FLASH)
		dev->ops->ReadPageSpareLayout(dev, block, 0, (u8 *)&tag, TAG_STORE_SIZE, NULL);
	else
		dev->ops->ReadPageSpare(dev, block, 0, (u8 *)&tag, TAG_STORE_SIZE);

	if (tag.block_status == 0xFF) {
		if (dev->attr->layout_opt == UFFS_LAYOUT_FLASH)
			dev->ops->ReadPageSpareLayout(dev, block, 0, (u8 *)&tag, TAG_STORE_SIZE, NULL);
		else
			dev->ops->ReadPageSpare(dev, block, 0, (u8 *)&tag, TAG_STORE_SIZE);

		if (tag.block_status == 0xFF)
			return U_FALSE;
	}

	return U_TRUE;
}


/** Erase flash block */
URET uffs_FlashEraseBlock(uffs_Device *dev, int block)
{
	int ret;

	ret = dev->ops->EraseBlock(dev, block);

	if (ret == -2 || ret > 0)
		uffs_BadBlockAdd(dev, block);

	return (ret >= 0 ? U_SUCC : U_FAIL);
}

