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
#ifndef _UFFS_CORE_H_
#define _UFFS_CORE_H_



/** \typedef uffs_Device */
typedef struct uffs_DeviceSt		uffs_Device;	//NAND flash device
/** \typedef uffs_DevOps */
typedef struct uffs_DeviceOpsSt		uffs_DevOps;	//NAND flash operations

typedef struct uffs_blockInfoSt uffs_blockInfo;
typedef struct uffs_pageSpareSt uffs_pageSpare;
typedef struct uffs_TagsSt			uffs_Tags;		//UFFS page tags
typedef struct uffs_TagsSt_8		uffs_Tags_8;	//UFFS page tags, fit for 8 bytes spare space

typedef struct uffs_BufSt uffs_Buf;			//page buffer



#endif
