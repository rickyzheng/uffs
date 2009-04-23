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
 * \file uffs_badblock.h
 * \brief bad block management
 * \author Ricky Zheng
 */

#ifndef _UFFS_BADBLOCK_H_
#define _UFFS_BADBLOCK_H_

#include "uffs/uffs_public.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_core.h"

#define HAVE_BADBLOCK(dev) (dev->bad.block != UFFS_INVALID_BLOCK)

void uffs_InitBadBlock(uffs_Device *dev);
URET uffs_CheckBadBlock(uffs_Device *dev, uffs_Buf *buf, int block);
void uffs_ProcessBadBlock(uffs_Device *dev, TreeNode *node);
void uffs_RecoverBadBlock(uffs_Device *dev);

#endif
