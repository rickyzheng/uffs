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


/**
 * Read page spare, fill tag and ECC
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[out] tag
 * \param[out] ecc
 */
URET uffs_ReadPageSpare(uffs_Device *dev, int block, int page, uffs_Tags *tag, u8 *ecc)
{
	//TODO: ...
	return U_FAIL;
}

/**
 * Read page data to page buf and calculate ecc.
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[out] buf
 * \param[out] ecc
 */
URET uffs_ReadPage(uffs_Device *dev, int block, int page, uffs_Buf *buf, u8 *ecc)
{
	//TODO: ...
	return U_FAIL;
}

/**
 * write page data, tag and ecc 
 * \param[in] dev uffs device
 * \param[in] block
 * \param[in] page
 * \param[in] buf
 * \param[in] tag
 * \param[in] ecc
 */
URET uffs_WritePageCombine(uffs_Device *dev, int block, int page, uffs_Buf *buf, uffs_Tags *tag, u8 *ecc)
{
	//TODO: ...
	return U_FAIL;
}

/** Mark this block as bad block */
URET uffs_MarkBadBlock(uffs_Device *dev, int block)
{
	//TODO: ...
	return U_FAIL;
}

/** Erase flash block */
URET uffs_EraseBlock(uffs_Device *dev, int block)
{
	//TODO: ...
	return U_FAIL;
}

