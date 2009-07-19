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
#include <string.h>

#define PFX "Flash: "


#define ECC_SIZE(dev) (3 * (dev)->attr->page_data_size / 256)
#define TAG_STORE_SIZE	(sizeof(u16) + (int)(&((uffs_Tag *)0)->tag_ecc))


static int _calculate_spare_buf_size(uffs_Device *dev)
{
	u8 *p;
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
			n = (p[1] > tag_size ? : tag_size : p[1]);
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
	dev->mem.spare_buffer_size = _calculate_spare_buf_size(dev);

	return U_SUCC;
}

/**
 * unload spare to tag and ecc.
 */
static void _UnloadSpare(uffs_Device *dev, u8 *spare, uffs_Tags *tag, u8 *ecc)
{
	u8 *p_tag = (u8 *)tag;
	int tag_size = TAG_STORE_SIZE;
	int ecc_size = ECC_SIZE(dev);
	int n;
	u8 *p;

	// unload ecc
	p = dev->attr->ecc_layout;
	if (p && ecc) {
		while (*p != 0xFF && ecc_size > 0) {
			n = (p[1] > ecc_size ? ecc_size : p[1]);
			memcpy(ecc, spare_buf + p[0], n);
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
			memcpy(p_tag, spare_buf + p[1], n);
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
	uffs_StorageAttrSt *attr = dev->attr;
	u8 * ecc_buf = dev->mem.ecc_buffer;
	u8 * spare_buf = dev->mem.spare_buffer;
	u8 * p_tag;
	u16 tag_ecc;
	int ret = 0;
	UBOOL is_bad = U_FALSE;

	if (attr->layout_opt == UFFS_LAYOUT_FLASH)
		ret = ops->ReadPageSpareLayout(dev, block, page, tag, ecc);
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
		_UnloadSpare(dev, tag, ecc, spare_buf);

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
 * \param[out] ecc_stored
 */
URET uffs_FlashReadPage(uffs_Device *dev, int block, int page, uffs_Buf *buf, const u8 *ecc_stored)
{
	uffs_FlashOps *ops = dev->ops;
	int size = dev->attr->page_data_size;
	u8 *ecc_buf = &dev->mem.ecc_buffer;
	int ret;

	if (ops->ReadPageData(dev, block, page, buf->start, size, ecc_buf) != U_SUCC) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"I/O error\n");
		return U_FAIL;
	}

	if (dev->attr->ecc_opt != UFFS_ECC_NONE) {
		ret = uffs_EccCorrect(buf->start, size, ecc_read, ecc_buf);
		if (ret < 0) {
			uffs_Perror(UFFS_ERR_NORMAL, PFX"ecc correct fail, a new bad block(%d) is found when reading page (%d)\n", block, page);
			uffs_BadBlockAdd(dev, block);
			return U_FAIL;
		}
		else if (ret > 0) {
			uffs_BadBlockAdd(dev, block);
		}
	}

	buf->data_len = buf->start[0] | (buf->start[1] << 8);
	buf->check_sum = buf->start[2] | (buf->start[3] << 8);

	return U_SUCC;
}

/**
 * make spare from tag and ecc
 */
static void _MakeSpare(uffs_Device *dev, uffs_Tag *tag, u8 *ecc, u8* spare)
{
	u8 *p_tag = (u8 *)tag;
	int tag_size = TAG_STORE_SIZE;
	int ecc_size = ECC_SIZE(dev);
	int n;
	u8 *p;

	memset(spare, 0xFF, dev->mem.spare_buffer_size);	// initialize as 0xFF.

	// load ecc
	p = dev->attr->ecc_layout;
	if (p && ecc) {
		while (*p != 0xFF && ecc_size > 0) {
			n = (p[1] > ecc_size ? ecc_size : p[1]);
			memcpy(spare_buf + p[0], ecc, n);
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
		memcpy(p_tag, spare_buf + p[1], n);
		tag_size -= n;
		p_tag += n;
		p += 2;
	}
}

/**
 * write the whole page, data and spare
 *
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[in] buf contains data to be wrote
 * \param[in] tag tag to be wrote
 * \param[in] ecc ecc to be wrote
 *
 * \note if ecc passed as NULL, we'll calculate it, or by flash driver.
 */
URET uffs_FlashWritePageCombine(uffs_Device *dev, int block, int page, uffs_Buf *buf, uffs_Tags *tag, u8 *ecc)
{
	uffs_FlashOps *ops = dev->ops;
	int size = dev->attr->page_data_size;
	u8 *ecc_buf;
	u8 *spare = &dev->mem.spare_buffer;
	int ret;

	if (dev->attr->ecc_opt == UFFS_ECC_NONE) {
		ecc = NULL;
		ecc_buf = NULL;
	}
	else {
		ecc_buf = &dev->mem.ecc_buffer;
	}

	if (ops->WritePageData(dev, block, page, buf->start, size, ecc ? NULL : ecc_buf) == U_FAIL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"I/O error\n");
		return U_FAIL;
	}

	if (ecc == NULL && dev->attr->ecc_opt == UFFS_ECC_SOFT)
		uffs_MakeEcc(buf->start, size, ecc_buf);

	_MakeSpare(dev, tag, ecc ? ecc : ecc_buf, spare);

	if (ops->WritePageSpare(dev, block, page, spare, dev->mem.spare_buffer_size) == U_FAIL)
		return U_FAIL;
	
	return U_SUCC;
}

/** Mark this block as bad block */
URET uffs_FlashMarkBadBlock(uffs_Device *dev, int block)
{
	//TODO: ...
	return U_FAIL;
}

/** Erase flash block */
URET uffs_FlashEraseBlock(uffs_Device *dev, int block)
{
	//TODO: ...
	return U_FAIL;
}

