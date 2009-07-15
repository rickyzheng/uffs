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
 * \file uffs_find.c
 * \brief find objects under dir
 * \author Ricky Zheng, created 13th July, 2009
 */

#include <string.h> 
#include <stdio.h>
#include "uffs/uffs_find.h"


static URET _LoadObjectInfo(uffs_Device *dev, TreeNode *node, uffs_ObjectInfo *info, int type)
{
	uffs_Buf *buf;

	buf = uffs_BufGetEx(dev, (u8)type, node, 0);

	if (buf == NULL) {
		return U_FAIL;
	}

	memcpy(&(info->info), buf->data, sizeof(uffs_FileInfo));

	if (type == UFFS_TYPE_DIR) {
		info->len = 0;
		info->serial = node->u.dir.serial;
	}
	else {
		info->len = node->u.file.len;
		info->serial = node->u.file.serial;
	}

	uffs_BufPut(dev, buf);

	return U_SUCC;
}

/**
 * get object information
 *
 * \param[in] obj the object to be revealed
 * \param[out] info object information will be loaded to info
 *
 * \return U_SUCC or U_FAIL
 *
 * \node the obj should be openned before call this function.
 */
URET uffs_GetObjectInfo(uffs_Object *obj, uffs_ObjectInfo *info)
{
	uffs_Device *dev = obj->dev;
	URET ret = U_FAIL;

	uffs_DeviceLock(dev);

	if (obj && dev && info)
		ret = _LoadObjectInfo(dev, obj->node, info, obj->type);

	uffs_DeviceUnLock(dev);

	return ret;
}


/**
 * Open a FindInfo for finding objects under dir
 *
 * \param[out] f uffs_FindInfo structure
 * \param[in] dir an openned dir object (openned by uffs_OpenObject() ). 
 *
 * \return U_SUCC if success, U_FAIL if invalid param or the dir
 *			is not been openned.
 */
URET uffs_OpenFindObject(uffs_FindInfo *f, uffs_Object *dir)
{
	if (f == NULL || dir == NULL || dir->dev == NULL || dir->open_succ != U_TRUE)
		return U_FAIL;

	f->dev = dir->dev;
	f->serial = dir->serial;
	f->hash = 0;
	f->work = NULL;
	f->step = 0;

	return U_SUCC;
}

/**
 * Open a FindInfo for finding objects under dir
 *
 * \param[out] f uffs_FindInfo structure
 * \param[in] dev uffs device
 * \param[in] dir serial number of the dir to be searched
 *
 * \return U_SUCC if success, U_FAIL if invalid param or the dir
 *			serial number is not valid.
 */
URET uffs_OpenFindObjectEx(uffs_FindInfo *f, uffs_Device *dev, int dir)
{
	TreeNode *node;

	if (f == NULL || dev == NULL)
		return U_FAIL;

	node = uffs_FindDirNodeFromTree(dev, dir);

	if (node == NULL)
		return U_FAIL;

	f->serial = dir;
	f->dev = dev;
	f->hash = 0;
	f->work = NULL;
	f->step = 0;

	return U_SUCC;
}


/**
 * Find the first object
 *
 * \param[out] info the object information will be filled to info.
 *				if info is NULL, then skip this object.
 * \param[in] f uffs_FindInfo structure, openned by uffs_OpenFindObject().
 *
 * \return U_SUCC if an object is found, U_FAIL if no object is found.
 */
URET uffs_FindFirstObject(uffs_ObjectInfo * info, uffs_FindInfo * f)
{
	uffs_Device *dev = f->dev;
	TreeNode *node;
	u16 x;
	URET ret = U_SUCC;

	uffs_DeviceLock(dev);
	f->step = 0;

	if (f->step == 0) {
		for (f->hash = 0;
			f->hash < DIR_NODE_ENTRY_LEN;
			f->hash++) {

			x = dev->tree.dir_entry[f->hash];

			while (x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if(node->u.dir.parent == f->serial) {
					f->work = node;
					if (info) 
						ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_DIR);
					goto ext;
				}
				x = node->hash_next;
			}
		}

		//no subdirs, then lookup the files
		f->step++;
	}

	if (f->step == 1) {
		for (f->hash = 0;
			f->hash < FILE_NODE_ENTRY_LEN;
			f->hash++) {

			x = dev->tree.file_entry[f->hash];

			while (x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if (node->u.file.parent == f->serial) {
					f->work = node;
					if (info)
						ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_FILE);
					goto ext;
				}
				x = node->hash_next;
			}
		}
		f->step++;
	}

	ret = U_FAIL;
ext:
	uffs_DeviceUnLock(dev);

	return ret;
}

/**
 * Find the next object.
 *
 * \param[out] info the object information will be filled to info.
 *				if info is NULL, then skip this object.
 * \param[in] f uffs_FindInfo structure, openned by uffs_OpenFindObject().
 *
 * \return U_SUCC if an object is found, U_FAIL if no object is found.
 *
 * \note uffs_FindFirstObject() should be called before uffs_FindNextObject().
 */
URET uffs_FindNextObject(uffs_ObjectInfo *info, uffs_FindInfo * f)
{
	uffs_Device *dev = f->dev;
	TreeNode *node;
	u16 x;
	URET ret = U_SUCC;

	if (f->dev == NULL ||
		f->work == NULL ||
		f->step > 1) 
		return U_FAIL;

	uffs_DeviceLock(dev);

	x = f->work->hash_next;

	if (f->step == 0) { //!< working on dirs
		while (x != EMPTY_NODE) {
			node = FROM_IDX(x, &(dev->tree.dis));
			if (node->u.dir.parent == f->serial) {
				f->work = node;
				if (info)
					ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_DIR);
				goto ext;
			}
			x = node->hash_next;
		}

		f->hash++; //come to next hash entry

		for (; f->hash < DIR_NODE_ENTRY_LEN; f->hash++) {
			x = dev->tree.dir_entry[f->hash];
			while (x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if (node->u.dir.parent == f->serial) {
					f->work = node;
					if (info)
						ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_DIR);
					goto ext;
				}
				x = node->hash_next;
			}
		}

		//no subdirs, then lookup files ..
		f->step++;
		f->hash = 0;
		x = EMPTY_NODE;
	}

	if (f->step == 1) {

		while (x != EMPTY_NODE) {
			node = FROM_IDX(x, &(dev->tree.dis));
			if (node->u.file.parent == f->serial) {
				f->work = node;
				if (info)
					ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_FILE);
				goto ext;
			}
			x = node->hash_next;
		}

		f->hash++; //come to next hash entry

		for (; f->hash < FILE_NODE_ENTRY_LEN; f->hash++) {
			x = dev->tree.file_entry[f->hash];
			while (x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if (node->u.file.parent == f->serial) {
					f->work = node;
					if (info) 
						ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_FILE);
					goto ext;
				}
				x = node->hash_next;
			}
		}

		//no any files, stopped.
		f->step++;
	}

	ret = U_FAIL;
ext:
	uffs_DeviceUnLock(dev);

	return ret;
}

/**
 * Rewind a find object process.
 *
 * \note After rewind, you can call uffs_FindFirstObject() to start find object process.
 */
URET uffs_FindObjectRewind(uffs_FindInfo *f)
{
	if (f == NULL)
		return U_FAIL;

	f->hash = 0;
	f->work = NULL;
	f->step = 0;

	return U_SUCC;
}

/**
 * Close Find Object.
 *
 * \param[in] f uffs_FindInfo structure, openned by uffs_OpenFindObject().
 *
 * \return U_SUCC if success, U_FAIL if invalid param.
 */
URET uffs_CloseFindObject(uffs_FindInfo * f)
{
	if (f == NULL)
		return U_FAIL;

	f->dev = NULL;
	f->hash = 0;
	f->work = NULL;

	return U_SUCC;
}



