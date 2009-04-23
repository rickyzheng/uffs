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
 * \file uffs_buf.h
 * \brief page buffers
 * \author Ricky Zheng
 */

#ifndef UFFS_BUF_H
#define UFFS_BUF_H

#include "uffs/uffs_types.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_tree.h"
#include "uffs/uffs_core.h"

#ifdef __cplusplus
extern "C"{
#endif
	
#define CLONE_BUF_MARK		0xffff

#define UFFS_BUF_EMPTY		0
#define UFFS_BUF_VALID		1
#define UFFS_BUF_DIRTY		2

/** uffs page buffer */
struct uffs_BufSt{
	struct uffs_BufSt *next;	//!< link to next buffer
	struct uffs_BufSt *prev;	//!< link to previous buffer
	struct uffs_BufSt *nextDirty;
	struct uffs_BufSt *prevDirty;
	u8 type;					//!< file, dir, or data
	u16 father;					//!< father serial
	u16 serial;					//!< serial 
	u16 pageID;					//!< page id 
	u16 mark;					//!< #UFFS_BUF_EMPTY, #UFFS_BUF_VALID, or #UFFS_BUF_DIRTY ?
	u16 refCount;				//!< reference counter
	u16 dataLen;				//!< length of data
	u8 * data;					//!< data buffer
	u8 * ecc;					//!< ecc buffer
};


URET uffs_BufInit(struct uffs_DeviceSt *dev, int maxBuf, int maxDirtyBuf);
URET uffs_BufReleaseAll(struct uffs_DeviceSt *dev);

uffs_Buf * uffs_BufGet(struct uffs_DeviceSt *dev, u16 father, u16 serial, u16 pageID);
uffs_Buf *uffs_BufNew(struct uffs_DeviceSt *dev, u8 type, u16 father, u16 serial, u16 pageID);
uffs_Buf *uffs_BufGetEx(struct uffs_DeviceSt *dev, u8 type, TreeNode *node, u16 pageID);
uffs_Buf * uffs_BufFind(uffs_Device *dev, u16 father, u16 serial, u16 pageID);

URET uffs_BufPut(uffs_Device *dev, uffs_Buf *buf);

void uffs_BufIncRef(uffs_Buf *buf);
void uffs_BufDecRef(uffs_Buf *buf);
URET uffs_BufWrite(struct uffs_DeviceSt *dev, uffs_Buf *buf, void *data, u32 ofs, u32 len);
URET uffs_BufRead(struct uffs_DeviceSt *dev, uffs_Buf *buf, void *data, u32 ofs, u32 len);
void uffs_BufSetMark(uffs_Buf *buf, int mark);
URET uffs_BufFlush(struct uffs_DeviceSt *dev);
URET uffs_BufFlushEx(struct uffs_DeviceSt *dev, UBOOL force_block_recover);

UBOOL uffs_BufIsAllFree(struct uffs_DeviceSt *dev);
UBOOL uffs_BufIsAllEmpty(struct uffs_DeviceSt *dev);
URET uffs_BufSetAllEmpty(struct uffs_DeviceSt *dev);

uffs_Buf * uffs_BufClone(struct uffs_DeviceSt *dev, uffs_Buf *buf);
void uffs_BufFreeClone(uffs_Device *dev, uffs_Buf *buf);

URET uffs_LoadPhiDataToBuf(uffs_Device *dev, uffs_Buf *buf, u32 block, u32 page);
URET uffs_LoadPhiDataToBufEccUnCare(uffs_Device *dev, uffs_Buf *buf, u32 block, u32 page);

void uffs_BufInspect(uffs_Device *dev);

#ifdef __cplusplus
}
#endif


#endif
