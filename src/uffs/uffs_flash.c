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
#define TAG_STORE_SIZE	(sizeof(struct uffs_TagStoreSt))

// P256: [D0, D1, D2, D3, ECC1, S, ECC2, ECC3]
static const u8 P256_sdata_layout[] = {0, 4, 0xFF, 0};
static const u8 P256_ecc_layout[] = {4, 1, 6, 2, 0xFF, 0};

// P512: [D0, D1, D2, D3, D4, S, D5, D6, D7, ECC1, ECC2, ECC3, ECC4, ECC5, ECC6, X]
static const u8 P512_sdata_layout[] = {0, 5, 6, 3, 0xFF, 0};
static const u8 P512_ecc_layout[] = {9, 6, 0xFF, 0};

// P1K: [D0, D1, D2, D3, D4, S, D5, D6, D7, ECC1, ..., ECC12, X, ...]
static const u8 P1K_sdata_layout[] = {0, 5, 6, 3, 0xFF, 0};
static const u8 P1K_ecc_layout[] = {9, 12, 0xFF, 0};

// P2K: [D0, D1, D2, D3, D4, S, D5, D6, D7, ECC1, ..., ECC24, X, ...]
static const u8 P2K_sdata_layout[] = {0, 5, 6, 3, 0xFF, 0};
static const u8 P2K_ecc_layout[] = {9, 24, 0xFF, 0};

static const u8 * layout_sel_tbl[4][2] = {
	{P256_sdata_layout, P256_ecc_layout},
	{P512_sdata_layout, P512_ecc_layout},
	{P1K_sdata_layout, P1K_ecc_layout},
	{P2K_sdata_layout, P2K_ecc_layout},
};

static void uffs_TagMakeEcc(struct uffs_TagStoreSt *ts)
{
	ts->tag_ecc = 0xFFF;
	ts->tag_ecc = uffs_MakeEcc8(ts, sizeof(struct uffs_TagStoreSt));
}

static int uffs_TagEccCorrect(struct uffs_TagStoreSt *ts)
{
	u16 ecc_store, ecc_read;
	int ret;

	ecc_store = ts->tag_ecc;
	ts->tag_ecc = 0xFFF;
	ecc_read = uffs_MakeEcc8(ts, sizeof(struct uffs_TagStoreSt));
	ret = uffs_EccCorrect8(ts, ecc_read, ecc_store, sizeof(struct uffs_TagStoreSt));
	ts->tag_ecc = ecc_store;	// restore tag ecc

	return ret;

}

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

	idx = idx_tbl[(attr->page_data_size / 256) >> 1];

	if (attr->data_layout == NULL)
		attr->data_layout = layout_sel_tbl[idx][0];
	if (attr->ecc_layout == NULL)
		attr->ecc_layout = layout_sel_tbl[idx][1];

	dev->mem.spare_buffer_size = _calculate_spare_buf_size(dev);

	return U_SUCC;
}

/**
 * unload spare to tag and ecc.
 */
static void _UnloadSpare(uffs_Device *dev, const u8 *spare, uffs_Tags *tag, u8 *ecc)
{
	u8 *p_tag = (u8 *)&tag->s;
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
			memcpy(p_tag, spare + p[0], n);
			tag_size -= n;
			p_tag += n;
			p += 2;
		}

		tag->block_status = spare[dev->attr->block_status_offs];
	}
}

/**
 * Read tag and ecc from page spare
 *
 * \param[in] dev uffs device
 * \param[in] block flash block num
 * \param[in] page flash page num
 * \param[out] tag tag to be filled
 * \param[out] ecc ecc to be filled
 *
 * \return	#UFFS_FLASH_NO_ERR: success and/or has no flip bits.
 *			#UFFS_FLASH_IO_ERR: I/O error, expect retry ?
 *			#UFFS_FLASH_ECC_FAIL: spare data has flip bits and ecc correct failed.
 *			#UFFS_FLASH_ECC_OK: spare data has flip bits and corrected by ecc.
*/
int uffs_FlashReadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag, u8 *ecc)
{
	uffs_FlashOps *ops = dev->ops;
	struct uffs_StorageAttrSt *attr = dev->attr;
	u8 * spare_buf = dev->mem.spare_buffer;
	int ret = 0;
	UBOOL is_bad = U_FALSE;


	if (attr->layout_opt == UFFS_LAYOUT_FLASH)
		ret = ops->ReadPageSpareWithLayout(dev, block, page, (u8 *)&tag->s, tag ? TAG_STORE_SIZE : 0, ecc);
	else
		ret = ops->ReadPageSpare(dev, block, page, spare_buf, 0, dev->mem.spare_buffer_size);


	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		is_bad = U_TRUE;

	if (attr->layout_opt != UFFS_LAYOUT_FLASH)
		_UnloadSpare(dev, spare_buf, tag, ecc);

	// copy some raw data
	if (tag) {
		tag->_dirty = tag->s.dirty;
		tag->_valid = tag->s.valid;
	}

	if (UFFS_FLASH_HAVE_ERR(ret))
		goto ext;

	if (tag) {
		if (tag->_valid == 1) //it's not a valid page ? don't need go further
			goto ext;

		// do tag ecc correction
		if (dev->attr->ecc_opt != UFFS_ECC_NONE) {
			ret = uffs_TagEccCorrect(&tag->s);
			ret = (ret < 0 ? UFFS_FLASH_ECC_FAIL :
					(ret > 0 ? UFFS_FLASH_ECC_OK : UFFS_FLASH_NO_ERR));

			if (UFFS_FLASH_IS_BAD_BLOCK(ret))
				is_bad = U_TRUE;

			if (UFFS_FLASH_HAVE_ERR(ret))
				goto ext;
		}
	}

ext:
	if (is_bad) {
		uffs_BadBlockAdd(dev, block);
		uffs_Perror(UFFS_ERR_NORMAL, PFX"A new bad block (%d) is detected.\n", block);
	}

	return ret;
}

/**
 * Read page data to page buf and calculate ecc.
 * \param[in] dev uffs device
 * \param[in] block flash block num
 * \param[in] page flash page num of the block
 * \param[out] buf holding the read out data
 *
 * \return	#UFFS_FLASH_NO_ERR: success and/or has no flip bits.
 *			#UFFS_FLASH_IO_ERR: I/O error, expect retry ?
 *			#UFFS_FLASH_ECC_FAIL: spare data has flip bits and ecc correct failed.
 *			#UFFS_FLASH_ECC_OK: spare data has flip bits and corrected by ecc.
 */
int uffs_FlashReadPage(uffs_Device *dev, int block, int page, u8 *buf)
{
	uffs_FlashOps *ops = dev->ops;
	int size = dev->attr->page_data_size;
	u8 ecc_buf[MAX_ECC_SIZE];
	u8 ecc_store[MAX_ECC_SIZE];
	UBOOL is_bad = U_FALSE;

	int ret;

	// if ecc_opt is HW or HW_AUTO, flash driver should do ecc correction.
	ret = ops->ReadPageData(dev, block, page, buf, size, ecc_buf);
	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		is_bad = U_TRUE;

	if (UFFS_FLASH_HAVE_ERR(ret))
		goto ext;

	if (dev->attr->ecc_opt == UFFS_ECC_SOFT) {
		uffs_MakeEcc(buf, size, ecc_buf);
		ret = uffs_FlashReadPageSpare(dev, block, page, NULL, ecc_store);
		if (UFFS_FLASH_IS_BAD_BLOCK(ret))
			is_bad = U_TRUE;

		if (UFFS_FLASH_HAVE_ERR(ret))
			goto ext;

		ret = uffs_EccCorrect(buf, size, ecc_store, ecc_buf);
		ret = (ret < 0 ? UFFS_FLASH_ECC_FAIL :
				(ret > 0 ? UFFS_FLASH_ECC_OK : UFFS_FLASH_NO_ERR));

		if (UFFS_FLASH_IS_BAD_BLOCK(ret))
			is_bad = U_TRUE;

		if (UFFS_FLASH_HAVE_ERR(ret))
			goto ext;
	}

ext:
	if (is_bad) {
		uffs_BadBlockAdd(dev, block);
	}

	return ret;
}

/**
 * make spare from tag and ecc
 */
static void _MakeSpare(uffs_Device *dev, uffs_TagStore *ts, u8 *ecc, u8* spare)
{
	u8 *p_ts = (u8 *)ts;
	int ts_size = TAG_STORE_SIZE;
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

	p = dev->attr->data_layout;
	while (*p != 0xFF && ts_size > 0) {
		n = (p[1] > ts_size ? ts_size : p[1]);
		memcpy(spare + p[0], p_ts, n);
		ts_size -= n;
		p_ts += n;
		p += 2;
	}
}

/**
 * write the whole page, include data and tag
 *
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[in] buf contains data to be wrote
 * \param[in] tag tag to be wrote
 *
 */
int uffs_FlashWritePageCombine(uffs_Device *dev, int block, int page, u8 *buf, uffs_Tags *tag)
{
	uffs_FlashOps *ops = dev->ops;
	int size = dev->attr->page_data_size;
	u8 ecc_buf[MAX_ECC_SIZE];
	u8 *spare = dev->mem.spare_buffer;
	int ret;
	UBOOL is_bad = U_FALSE;
	uffs_TagStore local_ts;

	// setp 1: write only the dirty bit to the spare
	memset(&local_ts, 0xFF, sizeof(local_ts));
	local_ts.dirty = TAG_DIRTY;	//!< set dirty mark

	if (dev->attr->layout_opt == UFFS_LAYOUT_UFFS) {
		_MakeSpare(dev, &local_ts, NULL, spare);
		ret = ops->WritePageSpare(dev, block, page, spare, 0, dev->mem.spare_buffer_size);
	}
	else {
		ret = ops->WritePageSpareWithLayout(dev, block, page, (u8 *)&local_ts, 1, NULL);
	}

	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		is_bad = U_TRUE;

	if (UFFS_FLASH_HAVE_ERR(ret))
		goto ext;

	// setp 2: write page data
	if (dev->attr->ecc_opt == UFFS_ECC_SOFT)
		uffs_MakeEcc(buf, size, ecc_buf);

	ret = ops->WritePageData(dev, block, page, buf, size, ecc_buf);
	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		is_bad = U_TRUE;

	if (UFFS_FLASH_HAVE_ERR(ret))
		goto ext;

	// setep 3: write full tag to spare, with ECC
	tag->s.dirty = TAG_DIRTY;		//!< set dirty bit
	tag->s.valid = TAG_VALID;		//!< set valid bit
	if (dev->attr->ecc_opt != UFFS_ECC_NONE)
		uffs_TagMakeEcc(&tag->s);
	else
		tag->s.tag_ecc = 0xFFFF;

	if (dev->attr->layout_opt == UFFS_LAYOUT_UFFS) {
		if (dev->attr->ecc_opt == UFFS_ECC_SOFT ||
			dev->attr->ecc_opt == UFFS_ECC_HW) {
			_MakeSpare(dev, &tag->s, ecc_buf, spare);
		}
		else
			_MakeSpare(dev, &tag->s, NULL, spare);

		ret = ops->WritePageSpare(dev, block, page, spare, 0, dev->mem.spare_buffer_size);
	}
	else {
		ret = ops->WritePageSpareWithLayout(dev, block, page, (u8 *)(&tag->s), TAG_STORE_SIZE, ecc_buf);
	}

	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		is_bad = U_TRUE;

ext:
	if (is_bad)
		uffs_BadBlockAdd(dev, block);

	return ret;
}

/** Mark this block as bad block */
URET uffs_FlashMarkBadBlock(uffs_Device *dev, int block)
{
	u8 status = 0;
	int ret;

	if (dev->ops->MarkBadBlock)
		return dev->ops->MarkBadBlock(dev, block) == 0 ? U_SUCC : U_FAIL;

	if (dev->ops->WritePageSpare == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"flash driver must provide 'WritePageSpare' function!\n");
		return U_FAIL;
	}

	ret = dev->ops->EraseBlock(dev, block);
	if (ret == UFFS_FLASH_NO_ERR)
		ret = dev->ops->WritePageSpare(dev, block, 0, &status, dev->attr->block_status_offs, 1);

	return ret == UFFS_FLASH_NO_ERR ? U_SUCC : U_FAIL;
}

/** Is this block a bad block ? */
UBOOL uffs_FlashIsBadBlock(uffs_Device *dev, int block)
{
	u8 status = 0xFF;

	if (dev->ops->IsBadBlock)
		return dev->ops->IsBadBlock(dev, block) == 1 ? U_TRUE : U_FALSE;

	if (dev->ops->WritePageSpare == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"flash driver must provide 'ReadPageSpare' function!\n");
		return U_FALSE;
	}

	dev->ops->ReadPageSpare(dev, block, 0, &status, dev->attr->block_status_offs, 1);

	if (status == 0xFF) {
		dev->ops->ReadPageSpare(dev, block, 1, &status, dev->attr->block_status_offs, 1);
		if (status == 0xFF)
			return U_FALSE;
	}

	return U_TRUE;
}

/** Erase flash block */
URET uffs_FlashEraseBlock(uffs_Device *dev, int block)
{
	int ret;

	ret = dev->ops->EraseBlock(dev, block);

	if (UFFS_FLASH_IS_BAD_BLOCK(ret))
		uffs_BadBlockAdd(dev, block);

	return ret;
}

