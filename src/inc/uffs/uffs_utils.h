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
#ifndef UFFS_UTILS_H
#define UFFS_UTILS_H

#include "uffs/uffs_types.h"
#include "uffs/uffs_device.h"
#include "uffs/uffs_core.h"

#ifdef __cplusplus
extern "C"{
#endif


//begin method
#define PARTITION_FOLLOW_PRIVATE	0
#define PARTITION_BEGIN_ABSOLUTE	1
	
//alloc method
#define ALLOC_BY_SIZE		0
#define ALLOC_BY_ABSOLUTE	1
#define ALLOC_USE_FREE		2

//struct uffs_PartitionMakeInfoSt {
//	u32 begin_method;
//	u32	alloc_method;
//	union{
//		u32 begin_block;
//		u32 begin_offset;
//	};
//	union{
//		u32 end_block;
//		u32 size;
//		u32 remain_size;
//	};
//	u32 access;
//};
//
//
//URET uffs_MakePartition(struct uffs_DeviceSt *dev, struct uffs_PartitionMakeInfoSt *pi, int nums);
//
//void uffs_ListPartition(struct uffs_DeviceSt *dev);

//get UFFS disk version, if fail, return 0
int uffs_GetUFFSVersion(struct uffs_DeviceSt *dev);

URET uffs_FormatDevice(uffs_Device *dev);

#ifdef __cplusplus
}
#endif


#endif

