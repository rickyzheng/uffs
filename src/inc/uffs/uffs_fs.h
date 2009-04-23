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
 * \file uffs_fs.h
 * \brief uffs basic file operations
 * \author Ricky Zheng
 */

#ifndef _UFFS_FS_H_
#define _UFFS_FS_H_

#include "uffs/uffs_types.h"
#include "uffs/uffs.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_core.h"

#ifdef __cplusplus
extern "C"{
#endif


#define ENCODE_MBCS		1
#define ENCODE_UNICODE	2
#define UFFS_DEFAULT_ENCODE		ENCODE_MBCS


/** file object */
struct uffs_ObjectSt {
	/******* objects manager ********/
	int devLockCount;
	int devGetCount;

	/******** init level 0 ********/
	char name[MAX_FILENAME_LENGTH];		//!< name, for open or create
	u32 nameLen;						//!< name length
	u16 sum;							//!< sum of name
	u32 encode;							//!< encode method of name
	uffs_Device *dev;					//!< uffs device
	u32 oflag;
	u32 pmode;
	u8 type;
	int pagesOnHead;					//!< data pages on file head block
	u16 father;

	/******* init level 1 ********/
	TreeNode *node;						//!< file entry node in tree
	u16 serial;
	u32 attr;
	u32 createTime;
	u32 lastModify;
	u32 access;
	
	/******* output ******/
	int err;					//!< error number

	/******* current *******/
	u32 pos;					//!< current position in file

	/***** others *******/
	UBOOL openSucc;			//!< U_TRUE: succ, U_FALSE: fail

};

typedef struct uffs_ObjectSt uffs_Object;


typedef struct uffs_FindInfoSt {
	uffs_Object *obj;
	uffs_Device *dev;
	u16 father;
	int step;		//step 0: working on dir entries, 1: working on file entries, 2: stoped.
	int hash;
	TreeNode *work;
} uffs_FindInfo;


URET uffs_InitObjectBuf(void);
URET uffs_ReleaseObjectBuf(void);
uffs_Object * uffs_GetObject(void);
void uffs_PutObject(uffs_Object *obj);
int uffs_GetObjectIndex(uffs_Object *obj);
uffs_Object * uffs_GetObjectByIndex(int idx);


URET uffs_OpenObject(uffs_Object *obj, const char *fullname, int oflag, int pmode);
URET uffs_TruncateObject(uffs_Object *obj, u32 remain);
URET uffs_CreateObject(uffs_Object *obj, const char *fullname, int oflag, int pmode);

URET uffs_CloseObject(uffs_Object *obj);
int uffs_WriteObject(uffs_Object *obj, const void *data, int len);
int uffs_ReadObject(uffs_Object *obj, void *data, int len);
long uffs_SeekObject(uffs_Object *obj, long offset, int origin);
int uffs_GetCurOffset(uffs_Object *obj);
int uffs_EndOfFile(uffs_Object *obj);

URET uffs_RenameObject(const char *old_name, const char *new_name);
URET uffs_DeleteObject(const char * name);
URET uffs_GetObjectInfo(uffs_Object *obj, uffs_ObjectInfo *info);

/* find objects */
URET uffs_OpenFindObject(uffs_FindInfo *find_handle, const char * dir);
URET uffs_FindFirstObject(uffs_ObjectInfo * info, uffs_FindInfo * find_handle);
URET uffs_FindNextObject(uffs_ObjectInfo *info, uffs_FindInfo * find_handle);
URET uffs_CloseFindObject(uffs_FindInfo * find_handle);

#ifdef __cplusplus
}
#endif


#endif

