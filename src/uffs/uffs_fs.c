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
 * \file uffs_fs.c
 * \brief basic file operations
 * \author Ricky Zheng, created 12th May, 2005
 */

#include "uffs/uffs_fs.h"
#include "uffs/uffs_config.h"
#include "uffs/ubuffer.h"
#include "uffs/uffs_ecc.h"
#include "uffs/uffs_badblock.h"
#include "uffs/uffs_os.h"
#include <string.h> 
#include <stdio.h>

#define PFX "fs:"

#define GET_SERIAL_FROM_NODE(obj) ((obj)->type == UFFS_TYPE_DIR ? (obj)->node->u.dir.serial : (obj)->node->u.file.serial)
#define GET_BLOCK_FROM_NODE(obj) ((obj)->type == UFFS_TYPE_DIR ? (obj)->node->u.dir.block : (obj)->node->u.file.block)

static URET _PrepareOpenObj(uffs_Object *obj, const char *fullname, int oflag, int pmode);
static URET _CreateObjectUnder(uffs_Object *obj);
static void _ReleaseObjectResource(uffs_Object *obj);
static URET _TruncateObject(uffs_Object *obj, u32 remain);

static int _object_buf[sizeof(uffs_Object) * MAX_OBJECT_HANDLE / sizeof(int)];
static struct ubufm _object_dis = {
	_object_buf,
	sizeof(uffs_Object),
	MAX_OBJECT_HANDLE,
	NULL,
	0,
};


/**
 * initialise object buffers, called by UFFS internal
 */
URET uffs_InitObjectBuf(void)
{
	if(uBufInit(&_object_dis) < 0) return U_FAIL;
	else return U_SUCC;
}

/**
 * Release object buffers, called by UFFS internal
 */
URET uffs_ReleaseObjectBuf(void)
{
	return uBufRelease(&_object_dis) < 0 ? U_FAIL : U_SUCC;
}

/**
 * alloc a new object structure
 * \return the new object
 */
uffs_Object * uffs_GetObject(void)
{
	uffs_Object * obj;
	obj = (uffs_Object *)uBufGet(&_object_dis);
	if (obj) {
		memset(obj, 0, sizeof(uffs_Object));
		obj->openSucc = U_FALSE;
	}
	return obj;
}

/**
 * put the object struct back to system
 */
void uffs_PutObject(uffs_Object *obj)
{
	if(obj) uBufPut(obj, &_object_dis);
}

/**
 * \return the internal index num of object
 */
int uffs_GetObjectIndex(uffs_Object *obj)
{
	return uBufGetIndex(obj, &_object_dis);
}

/**
 * \return the object by the internal index
 */
uffs_Object * uffs_GetObjectByIndex(int idx)
{
	return (uffs_Object *) uBufGetBufByIndex(idx, &_object_dis);
}

static void uffs_ObjectDevLock(uffs_Object *obj)
{
	if (obj) {
		if (obj->dev) {
			uffs_DeviceLock(obj->dev);
			obj->devLockCount++;
		}
	}
}

static void uffs_ObjectDevUnLock(uffs_Object *obj)
{
	if (obj) {
		if (obj->dev) {
			obj->devLockCount--;
			uffs_DeviceUnLock(obj->dev);
		}
	}
} 

/** get path from fullname, return path length
 * fullname: "/abc/def"		|	"/abc/def/"		| "/"
 * ==> path: "/abc/"        |   "/abc/def/"		| "/"
 */
static int _getpath(const char *fullname, char *path)
{
	int i;
	int len;

	if (fullname[0] == 0) return 0;

	len = strlen(fullname);
	for (i = len - 1; i > 0 && fullname[i] != '/'; i--);

	i++;
	if (path) {
		memcpy(path, fullname, i);
		path[i] = 0;
	}

	return i;
}

/** get name from fullname
 * fullname: "/abc/def"		|	"/abc/def/"		| "/"
 *  returen: "def"          |   ""		        | ""
 */
static const char * _getname(const char *fullname)
{
	int pos = _getpath(fullname, NULL);

	return fullname + pos;
}

static int forwad_search_slash(const char *str, int pos)
{
	int len = strlen(str);
	while(str[pos] != '/' && pos < len) pos++;
	if (pos == len) 
		return -1;
	else
		return pos;
}

static int back_search_slash(const char *str, int pos)
{
	while (str[pos] != '/' && pos >= 0) pos--;
	return pos;
}

/**
 * create a new object and open it if success
 */
URET uffs_CreateObject(uffs_Object *obj, const char *fullname, int oflag, int pmode)
{
	URET ret = U_FAIL;

	oflag |= UO_CREATE;

	if (_PrepareOpenObj(obj, fullname, oflag, pmode) == U_SUCC) {
		uffs_ObjectDevLock(obj);
		ret  = _CreateObjectUnder(obj);
		uffs_ObjectDevUnLock(obj);
	}

	return ret;
}


/** find the matched mount point from path */
static int find_maxMatchedMountPoint(const char *path)
{
	char buf[MAX_PATH_LENGTH+1] = {0};
	int pos;
	uffs_Device *dev;

	pos = strlen(path);
	memcpy(buf, path, pos + 1);

	while(pos > 0) {
		if ((dev = uffs_GetDevice(buf)) != NULL ) {
			uffs_PutDevice(dev);
			return pos;
		}
		else {
			buf[pos - 1] = '\0'; //replace the last '/' with '\0'

			//back forward search the next '/'
			for (; pos > 0 && buf[pos-1] != '/'; pos--)
				buf[pos-1] = '\0';
		}
	}

	return pos;
}

static URET _CreateObjectUnder(uffs_Object *obj)
{
	/**
	 * \note: level 0 has been set in obj.
	 */
	uffs_Buf *buf = NULL;
	uffs_fileInfo fi, *pfi;
	URET ret;

	TreeNode *node;

	if (obj->type == UFFS_TYPE_DIR) {
		//find out whether have file with the same name
		node = uffs_FindFileNodeByName(obj->dev, obj->name, obj->nameLen, obj->sum, obj->father);
		if (node != NULL) {
			obj->err = UEEXIST;
			return U_FAIL;
		}
		obj->node = uffs_FindDirNodeByName(obj->dev, obj->name, obj->nameLen, obj->sum, obj->father);
	}
	else {
		//find out whether have dir with the same name
		node = uffs_FindDirNodeByName(obj->dev, obj->name, obj->nameLen, obj->sum, obj->father);
		if (node != NULL) {
			obj->err = UEEXIST;
			return U_FAIL;
		}
		obj->node = uffs_FindFileNodeByName(obj->dev, obj->name, obj->nameLen, obj->sum, obj->father);
	}

	if (obj->node) {
		/* dir|file already exist, truncate it to zero length */
		obj->serial = GET_SERIAL_FROM_NODE(obj);

		buf = uffs_BufGetEx(obj->dev, obj->type, obj->node, 0);

		if(buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"found in tree, but can' load buffer ?\n");
			goto err;
		}

		pfi = (uffs_fileInfo *)(buf->data);
		obj->access = pfi->access;
		obj->attr = pfi->attr;
		obj->createTime = pfi->createTime;
		obj->lastModify = pfi->lastModify;
		uffs_BufPut(obj->dev, buf);

		obj->pos = 0;

		obj->openSucc = U_TRUE;
		ret = _TruncateObject(obj, 0);
		if (ret != U_SUCC) {
			obj->openSucc = U_FALSE;
		}
		return ret;
	}

	/* dir|file does not exist, create a new one */
	obj->serial = uffs_FindFreeFsnSerial(obj->dev);
	if (obj->serial == INVALID_UFFS_SERIAL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"No free serial num!\n");
		goto err;
	}

	if(obj->dev->tree.erasedCount < MINIMUN_ERASED_BLOCK) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"insufficient block in create obj\n");
		goto err;
	}

	buf = uffs_BufNew(obj->dev, obj->type, obj->father, obj->serial, 0);
	if(buf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't new buffer when create obj!\n");
		goto err;
	}

	memset(&fi, 0, sizeof(uffs_fileInfo));
	memcpy(fi.name, obj->name, obj->nameLen);
	fi.name[obj->nameLen] = '\0';
	fi.name_len = obj->nameLen;
	fi.access = 0;
	fi.attr |= FILE_ATTR_WRITE;
	if (obj->type == UFFS_TYPE_DIR)
		fi.attr |= FILE_ATTR_DIR;
	fi.createTime = fi.lastModify = uffs_GetCurDateTime();

	obj->createTime = fi.createTime;
	obj->lastModify = fi.lastModify;
	obj->attr = fi.attr;
	obj->access = fi.access;

	uffs_BufWrite(obj->dev, buf, &fi, 0, sizeof(uffs_fileInfo));
	uffs_BufPut(obj->dev, buf);

	//flush buffer immediately, so that the new node will be inserted into the tree
	uffs_BufFlushGroup(obj->dev, obj->father, obj->serial);

	//update obj->node: after buf flushed, the NEW node can be found in the tree
	if (obj->type == UFFS_TYPE_DIR)
		obj->node = uffs_FindDirNodeFromTree(obj->dev, obj->serial);
	else
		obj->node = uffs_FindFileNodeFromTree(obj->dev, obj->serial);

	if(obj->node == NULL) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't find the node in the tree ?\n");
		goto err;
	}

	if (obj->type == UFFS_TYPE_DIR) {
		//do nothing for dir ...
	}
	else {
		obj->node->u.file.len = 0;	//init the length to 0
	}

	if (HAVE_BADBLOCK(obj->dev)) uffs_RecoverBadBlock(obj->dev);
	obj->openSucc = U_TRUE;

	return U_SUCC;

err:
	if (buf && obj->dev) {
		uffs_BufPut(obj->dev, buf);
		buf = NULL;
	}

	return U_FAIL;
}


static URET _OpenObjectUnder(uffs_Object *obj)
{
	uffs_Buf *buf = NULL;
	uffs_fileInfo *fi = NULL;


	/*************** init level 1 ***************/
	if (obj->type == UFFS_TYPE_DIR) {
		obj->node = uffs_FindDirNodeByName(obj->dev, obj->name, obj->nameLen, obj->sum, obj->father);
	}
	else {
		obj->node = uffs_FindFileNodeByName(obj->dev, obj->name, obj->nameLen, obj->sum, obj->father);
	}

	if(obj->node == NULL) {
		/* dir|file not exist */
		if(obj->oflag & UO_CREATE){
			//create dir|file
			return _CreateObjectUnder(obj);
		}
		else {
			obj->err = UENOENT;
			//uffs_Perror(UFFS_ERR_NOISY, PFX"dir or file not found\n");
			return U_FAIL;
		}
	}

	obj->serial = GET_SERIAL_FROM_NODE(obj);

	buf = uffs_BufGetEx(obj->dev, obj->type, obj->node, 0);

	if(buf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get buf when open!\n");
		return U_FAIL;
	}

	fi = (uffs_fileInfo *)(buf->data);
	obj->attr = fi->attr;
	obj->createTime = fi->createTime;
	obj->lastModify = fi->lastModify;
	obj->access = fi->access;
	uffs_BufPut(obj->dev, buf);

	if((obj->oflag & (UO_CREATE | UO_EXCL)) == (UO_CREATE | UO_EXCL)){
		obj->err = UEEXIST;
		return U_FAIL;
	}

	obj->openSucc = U_TRUE;
	
	if(obj->oflag & UO_TRUNC) {
		if(_TruncateObject(obj, 0) == U_FAIL){
			//obj->err will be set in _TruncateObject
			return U_FAIL;
		}
	}

	obj->err = UENOERR;


	return U_SUCC;
}

/**
 * \brief search '/' in path from *pos
 * \param path the string to be search
 * \param pos[in/out] search start position, *pos will be updated after search
 * \return matched length
 */
static int getPathPart(const char *path, int *pos)
{
	int len = 0;

	for (len = 0; path[len+(*pos)] != '\0' && path[len+(*pos)] != '/'; len++) ;

	*pos += len;

	return len;
}

/** get dir's serial No. 
 *  path is the relative path from mount point: "abc/def/"  |  "" (in this case, return father)
 */
static u16 _GetDirSerial(uffs_Device *dev, const char *path, u16 father)
{
	int pos = 0;
	int pos_last = 0;
	int len = 0;
	char part[MAX_FILENAME_LENGTH+1] = {0};
	TreeNode * node;
	u16 sum;
	u16 serial = father;

	while((len = getPathPart(path, &pos)) > 0) {
		memcpy(part, path + pos_last, len);
		part[len] = '\0';
		sum = uffs_MakeSum16(part, len);
		node = uffs_FindDirNodeByName(dev, part, len, sum, serial);
		if (node) {
			serial = node->u.dir.serial;
			pos_last = ++pos;
		}
		else {
			return INVALID_UFFS_SERIAL;
		}
	}

	return serial;
}

/** prepare the object struct for open */
static URET _PrepareOpenObj(uffs_Object *obj, const char *fullname, int oflag, int pmode)
{
	char buf[MAX_PATH_LENGTH+1] = {0};
	char mount[MAX_FILENAME_LENGTH+1] = {0};
	char *start_path = mount;
	int pos;
	int i, len;
	u8 type;

	type = (oflag & UO_DIR) ? UFFS_TYPE_DIR : UFFS_TYPE_FILE;

	obj->dev = NULL;

	if((oflag & (UO_WRONLY | UO_RDWR)) == (UO_WRONLY | UO_RDWR)){
		/* UO_WRONLY and UO_RDWR can't appear together */
		uffs_Perror(UFFS_ERR_NOISY, PFX"UO_WRONLY and UO_RDWR can't appear together\n");
		obj->err = UEINVAL;
		return U_FAIL;
	}

	obj->pos = 0;

	if (!fullname) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"bad file/dir name: %s\n", fullname);
		obj->err = UEINVAL;
		return U_FAIL;
	}

	/************** init level 0 **************/
	obj->oflag = oflag;
	obj->pmode = pmode;

	len = strlen(fullname);

	if (oflag & UO_DIR) {
		/** open a directory */
		obj->type = UFFS_TYPE_DIR;

		// dir name should end with '/'
		if (fullname[len-1] != '/') {
			uffs_Perror(UFFS_ERR_NOISY, PFX"bad dir name: %s\n", fullname);
			obj->err = UEINVAL;
			return U_FAIL;
		}
	}
	else {
		/** open a file */
		obj->type = UFFS_TYPE_FILE;

		if (fullname[len-1] == '/') {
			uffs_Perror(UFFS_ERR_NOISY, PFX"bad file name: %s\n", fullname);
			obj->err = UEINVAL;
			return U_FAIL;
		}
	}

	_getpath(fullname, buf);
	// get path from full name, now what's on buf:
	//	if it's dir:        "/abc/def/xyz/"  |  "/abc/"  |   "/abc/"  |  "/"
	//  if it's file:       "/abc/def/"      |  X        |   "/"      |  X


	pos = find_maxMatchedMountPoint(buf);
	if (pos == 0) {
		/* can't not find any mount point from the path ??? */
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't find any mount point from the path %s\n", buf);
		obj->err = UENOENT;
		return U_FAIL;
	}

	// get mount point: "/abc/"	or "/"
	memcpy(mount, buf, pos);
	mount[pos] = '\0';

	obj->dev = uffs_GetDevice(mount);
	if (obj->dev == NULL) {
		// uffs_Perror(UFFS_ERR_NOISY, "Can't get device from mount point: %s\r\n", mount);
		obj->err = UENOENT;
		return U_FAIL;
	}

	//here is a good chance to deal with bad block ...
	if (HAVE_BADBLOCK(obj->dev)) uffs_RecoverBadBlock(obj->dev);

	// get the rest name (fullname - mount point) to buf: 
	//   if the mount point is "/abc/", then:
	//		if it's dir:   "def/xyz/"     |   ""      |  ""     |  X
	//		if it's file:  "def/xyz"      |   X       |  X      |  X
	//   if the mount point is "/", then:
	//      if it's dir:   "abc/def/xyz/  |   "abc/"  |  "abc/" |  ""
	//      if it's file:  "abc/def/xyz"  |   X       |  "abc"  |  X
	len = strlen(fullname) - strlen(mount);
	memcpy(buf, fullname + strlen(mount), len);
	buf[len] = '\0';

	if (len == 0 && (oflag & UO_DIR) == 0) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"bad file name: %s\n", fullname);
		obj->err = UEINVAL;
		goto err;
	}

	if (oflag & UO_DIR) {
		if (len == 0) {

			// uffs_Perror(UFFS_ERR_NOISY, "Zero length name ? root dir ?\r\n");

			obj->father = PARENT_OF_ROOT;
			obj->serial = ROOT_DIR_ID;
			obj->nameLen = 0;
		}
		else {
			if (buf[len-1] == '/') {
				buf[len-1] = 0;
				len--;
			}
			for (i = len - 1; i >= 0 && buf[i] != '/'; i--);
			obj->nameLen = len - i - 1;
			strcpy(obj->name, buf + i + 1);
			if (i < 0) {
				obj->father = ROOT_DIR_ID;
			}
			else {
				buf[i+1] = 0;
				obj->father = _GetDirSerial(obj->dev, buf, ROOT_DIR_ID);
			}
		}
	}
	else {
		for(i = len - 1; i >= 0 && buf[i] != '/'; i--);
		obj->nameLen = len - i - 1;
		strcpy(obj->name, buf + i + 1);
		if (i < 0) {
			obj->father = ROOT_DIR_ID;
		}
		else {
			buf[i+1] = 0;
			obj->father = _GetDirSerial(obj->dev, buf, ROOT_DIR_ID);
		}
	}

	if (obj->father == INVALID_UFFS_SERIAL) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"can't open path %s\n", start_path);
		obj->err = UENOENT;
		goto err;
	}

	obj->sum = uffs_MakeSum16(obj->name, obj->nameLen);
	obj->encode = UFFS_DEFAULT_ENCODE;
	obj->pagesOnHead = obj->dev->attr->pages_per_block - 1;

	//uffs_Perror(UFFS_ERR_NOISY, "Prepare name: %s\r\n", obj->name);

	return U_SUCC;

err:
	if (obj->dev) {
		uffs_PutDevice(obj->dev);
		obj->dev = NULL;
	}

	return U_FAIL;
}

URET uffs_OpenObject(uffs_Object *obj, const char *fullname, int oflag, int pmode)
{
	URET ret = U_FAIL;

	if ((ret = _PrepareOpenObj(obj, fullname, oflag, pmode)) == U_SUCC) {
		if (obj->nameLen > 0) {
			uffs_ObjectDevLock(obj);
			ret = _OpenObjectUnder(obj);
			uffs_ObjectDevUnLock(obj);
		}
	}

	if(ret == U_FAIL)
		_ReleaseObjectResource(obj);

	return ret;
}

static void _ReleaseObjectResource(uffs_Object *obj)
{
	if (obj) {
		if (obj->dev) {
			if (HAVE_BADBLOCK(obj->dev)) uffs_RecoverBadBlock(obj->dev);
			if (obj->devLockCount > 0) {
				uffs_ObjectDevUnLock(obj);
			}
			uffs_PutDevice(obj->dev);
			obj->dev = NULL;
		}
	}
}


static URET _FlushObject(uffs_Object *obj)
{
	uffs_Device *dev;

	dev = obj->dev;
	if (obj->node) {
		if (obj->type == UFFS_TYPE_DIR)
			return uffs_BufFlushGroup(dev, obj->node->u.dir.father, obj->node->u.dir.serial);
		else {
			return (uffs_BufFlushGroupMatchFather(dev, obj->node->u.file.serial) == U_SUCC &&
				uffs_BufFlushGroup(dev, obj->node->u.file.father, obj->node->u.file.serial) == U_SUCC) ? U_SUCC : U_FAIL;
		}
	}
	return U_SUCC;
}

URET uffs_FlushObject(uffs_Object *obj)
{
	uffs_Device *dev;
	URET ret;

	if(obj == NULL || obj->dev == NULL) return U_FAIL;
	if (obj->openSucc != U_TRUE) return U_FAIL;

	dev = obj->dev;
	uffs_ObjectDevLock(obj);
	ret = _FlushObject(obj);
	uffs_ObjectDevUnLock(obj);

	return ret;
}

URET uffs_CloseObject(uffs_Object *obj)
{
	uffs_Device *dev;
	uffs_Buf *buf;
	uffs_fileInfo fi;

	if(obj == NULL || obj->dev == NULL) return U_FAIL;
	if (obj->openSucc != U_TRUE) goto out;

	dev = obj->dev;
	uffs_ObjectDevLock(obj);

	if(obj->oflag & (UO_WRONLY|UO_RDWR|UO_APPEND|UO_CREATE|UO_TRUNC)) {

#ifdef CHANGE_MODIFY_TIME
		if (obj->node) {
			//need to change the last modify time stamp
			if (obj->type == UFFS_TYPE_DIR)
				buf = uffs_BufGetEx(dev, UFFS_TYPE_DIR, obj->node, 0);
			else
				buf = uffs_BufGetEx(dev, UFFS_TYPE_FILE, obj->node, 0);

			if(buf == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get file header\n");
				_FlushObject(obj);
				uffs_ObjectDevUnLock(obj);
				goto out;
			}
			uffs_BufRead(dev, buf, &fi, 0, sizeof(uffs_fileInfo));
			fi.lastModify = uffs_GetCurDateTime();
			uffs_BufWrite(dev, buf, &fi, 0, sizeof(uffs_fileInfo));
			uffs_BufPut(dev, buf);
		}
#endif
		_FlushObject(obj);
	}

	uffs_ObjectDevUnLock(obj);

out:
	_ReleaseObjectResource(obj);

	return U_SUCC;
}

static u16 _GetFdnByOfs(uffs_Object *obj, u32 ofs)
{
	uffs_Device *dev = obj->dev;
	if(ofs < obj->pagesOnHead * dev->com.pgDataSize) {
		return 0;
	}
	else {
		ofs -= obj->pagesOnHead * dev->com.pgDataSize;
		return (ofs / (dev->com.pgDataSize * dev->attr->pages_per_block)) + 1;
	}
}

//static u16 _GetPageIdByOfs(uffs_Object *obj, u32 ofs)
//{
//	uffs_Device *dev = obj->dev;
//	if(ofs < obj->pagesOnHead * dev->com.pgDataSize) {
//		return (ofs / dev->com.pgDataSize) + 1; //in file header, pageID start from 1, not 0
//	}
//	else {
//		ofs -= (obj->pagesOnHead * dev->com.pgDataSize);
//		ofs %= (dev->com.pgDataSize * dev->attr->pages_per_block);
//		return ofs / dev->com.pgDataSize;
//	}
//}
//
//static UBOOL _IsAtTheStartOfBlock(uffs_Object *obj, u32 ofs)
//{
//	uffs_Device *dev = obj->dev;
//	int n;
//
//	if((ofs % dev->com.pgDataSize) != 0) return U_FALSE;
//	if(ofs < obj->pagesOnHead * dev->com.pgDataSize) {
//		return U_FALSE;
//	}
//	else {
//		n = ofs - (obj->pagesOnHead * dev->com.pgDataSize);
//		if(n % (dev->com.pgDataSize * dev->attr->pages_per_block) == 0) return U_TRUE;
//	}
//
//	return U_FALSE;
//}
//
static u32 _GetStartOfDataBlock(uffs_Object *obj, u16 fdn)
{
	if(fdn == 0) {
		return 0;
	}
	else {
		return (obj->pagesOnHead * obj->dev->com.pgDataSize) +
			(fdn - 1) * (obj->dev->com.pgDataSize * obj->dev->attr->pages_per_block);
	}
}


static int _WriteNewBlock(uffs_Object *obj,
						  const void *data, u32 len,
						  u16 father,
						  u16 serial)
{
	uffs_Device *dev = obj->dev;
	u16 pageID;
	int wroteSize = 0;
	int size;
	uffs_Buf *buf;
	URET ret;

	for(pageID = 0; pageID < dev->attr->pages_per_block; pageID++) {
		size = (len - wroteSize) > dev->com.pgDataSize ?
			dev->com.pgDataSize : len - wroteSize;
		if(size <= 0) break;

		buf = uffs_BufNew(dev, UFFS_TYPE_DATA, father, serial, pageID);
		if(buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't create a new page ?\n");
			break;
		}
		ret = uffs_BufWrite(dev, buf, (u8 *)data + wroteSize, 0, size);
		uffs_BufPut(dev, buf);

		if(ret != U_SUCC) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"write data fail!\n");
			break;
		}
		wroteSize += size;
		obj->node->u.file.len += size;
	}
	return wroteSize;
}

static int _WriteInternalBlock(uffs_Object *obj,
							   TreeNode *node,
							   u16 fdn,
							   const void *data,
							   u32 len,
							   u32 blockOfs)
{
	uffs_Device *dev = obj->dev;
	u16 maxPageID;
	u16 pageID;
	u32 size;
	u32 pageOfs;
	u32 wroteSize = 0;
	URET ret;
	uffs_Buf *buf;
	u32 startOfBlock;
	u8 type;
	u16 father, serial;

	startOfBlock = _GetStartOfDataBlock(obj, fdn);
	if(fdn == 0) {
		type = UFFS_TYPE_FILE;
		father = node->u.file.father;
		serial = node->u.file.serial;
	}
	else {
		type = UFFS_TYPE_DATA;
		father = node->u.data.father;
		serial = fdn;
	}

	if(fdn == 0) maxPageID = obj->pagesOnHead;
	else maxPageID = dev->attr->pages_per_block - 1;


	while(wroteSize < len) {
		pageID = blockOfs / dev->com.pgDataSize;
		if(fdn == 0) pageID++; //in file header, pageID start from 1, not 0.
		if(pageID > maxPageID) break;

		pageOfs = blockOfs % dev->com.pgDataSize;
		size = (len - wroteSize + pageOfs) > dev->com.pgDataSize ?
			(dev->com.pgDataSize - pageOfs) : (len - wroteSize);

		if((obj->node->u.file.len % dev->com.pgDataSize) == 0 &&
			(blockOfs + startOfBlock) == obj->node->u.file.len) {

			buf = uffs_BufNew(dev, type, father, serial, pageID);
			if(buf == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can create a new buf!\n");
				break;
			}
		}
		else {
			buf = uffs_BufGetEx(dev, type, node, pageID);
			if(buf == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get buffer ?\n");
				break;
			}
		}

		ret = uffs_BufWrite(dev, buf, (u8 *)data + wroteSize, pageOfs, size);
		uffs_BufPut(dev, buf);
		if(ret == U_FAIL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"write inter data fail!\n");
			break;
		}

		wroteSize += size;
		blockOfs += size;

		if(startOfBlock + blockOfs > obj->node->u.file.len)
			obj->node->u.file.len = startOfBlock + blockOfs;

	}

	return wroteSize;
}



/**
 * write data to obj, from obj->pos
 * \param[in] obj file obj
 * \param[in] data data pointer
 * \param[in] len length of data to be write
 * \return bytes wrote to obj
 */
int uffs_WriteObject(uffs_Object *obj, const void *data, int len)
{
	uffs_Device *dev = obj->dev;
	TreeNode *fnode = obj->node;
	int remain = len;
	u16 fdn;
	u32 write_start;
	TreeNode *dnode;
	u32 size;

	if (obj == NULL) return 0;
	if (obj->dev == NULL || obj->openSucc == U_FALSE) return 0;

	if (obj->type == UFFS_TYPE_DIR) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't write to an dir object!\n");
		return 0;
	}

	if(obj->pos > fnode->u.file.len) return 0; //can't write file out of range

	if(obj->oflag == UO_RDONLY) return 0;

	uffs_ObjectDevLock(obj);

	if(obj->oflag & UO_APPEND) obj->pos = fnode->u.file.len;

	while(remain > 0) {

		write_start = obj->pos + len - remain;
		if(write_start > fnode->u.file.len) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"write point out of file ?\n");
			break;
		}

		fdn = _GetFdnByOfs(obj, write_start);

		if(write_start == fnode->u.file.len && fdn > 0 &&
			write_start == _GetStartOfDataBlock(obj, fdn)) {
			if(dev->tree.erasedCount < MINIMUN_ERASED_BLOCK) {
				uffs_Perror(UFFS_ERR_NOISY, PFX"insufficient block in write obj, new block\n");
				break;
			}
			size = _WriteNewBlock(obj, (u8 *)data + len - remain, remain, fnode->u.file.serial, fdn);

			//Flush immediately, so that the new data node will be created and put in the tree.
			uffs_BufFlushGroup(dev, fnode->u.file.serial, fdn);

			if(size == 0) break;
			remain -= size;
		}
		else {

			if(fdn == 0)
				dnode = obj->node;
			else
				dnode = uffs_FindDataNode(dev, fnode->u.file.serial, fdn);

			if(dnode == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't find data node in tree ?\n");
				break;
			}
			size = _WriteInternalBlock(obj, dnode, fdn,
									(u8 *)data + len - remain, remain,
									write_start - _GetStartOfDataBlock(obj, fdn));
#ifdef FLUSH_BUF_AFTER_WRITE
			uffs_BufFlushAll(dev);
#endif
			if(size == 0) break;
			remain -= size;
		}
	}

	obj->pos += (len - remain);

	if (HAVE_BADBLOCK(dev)) uffs_RecoverBadBlock(dev);

	uffs_ObjectDevUnLock(obj);

	return len - remain;
}

/**
 * read data from obj
 * \param[in] obj uffs object
 * \param[out] data output data buffer
 * \param[in] len required length of data to be read from object->pos
 * \return return bytes of data have been read
 */
int uffs_ReadObject(uffs_Object *obj, void *data, int len)
{
	uffs_Device *dev = obj->dev;
	TreeNode *fnode = obj->node;
	u32 remain = len;
	u16 fdn;
	u32 read_start;
	TreeNode *dnode;
	u32 size;
	uffs_Buf *buf;
	u32 blockOfs;
	u16 pageID;
	u8 type;
	u32 pageOfs;

	if (obj == NULL) return 0;
	if (obj->dev == NULL || obj->openSucc == U_FALSE) return 0;


	if (obj->type == UFFS_TYPE_DIR) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't read from a dir object!\n");
		return 0;
	}

	if(obj->pos > fnode->u.file.len) return 0; //can't read file out of range
	if(obj->oflag & UO_WRONLY) return 0;

	uffs_ObjectDevLock(obj);

	while(remain > 0) {
		read_start = obj->pos + len - remain;
		if(read_start >= fnode->u.file.len) {
			//uffs_Perror(UFFS_ERR_NOISY, PFX"read point out of file ?\n");
			break;
		}

		fdn = _GetFdnByOfs(obj, read_start);
		if(fdn == 0) {
			dnode = obj->node;
			type = UFFS_TYPE_FILE;
		}
		else {
			type = UFFS_TYPE_DATA;
			dnode = uffs_FindDataNode(dev, fnode->u.file.serial, fdn);
			if(dnode == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get data node in entry!\n");
				break;
			}
		}

		blockOfs = _GetStartOfDataBlock(obj, fdn);
		pageID = (read_start - blockOfs) / dev->com.pgDataSize;

		if(fdn == 0) {
			/**
			 * fdn == 0: this means that the reading range is start from the first block,
			 * since the page 0 is for file attr, so we move to the next page ID.
			 */
			pageID++;
		}

		buf = uffs_BufGetEx(dev, type, dnode, (u16)pageID);
		if(buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get buffer when read obj.\n");
			break;
		}

		pageOfs = read_start % dev->com.pgDataSize;
		if(pageOfs >= buf->dataLen) {
			//uffs_Perror(UFFS_ERR_NOISY, PFX"read data out of page range ?\n");
			uffs_BufPut(dev, buf);
			break;
		}
		size = (remain + pageOfs > buf->dataLen ? buf->dataLen - pageOfs: remain);

		uffs_BufRead(dev, buf, (u8 *)data + len - remain, pageOfs, size);
		uffs_BufPut(dev, buf);

		remain -= size;
	}

	obj->pos += (len - remain);

	if (HAVE_BADBLOCK(dev)) uffs_RecoverBadBlock(dev);

	uffs_ObjectDevUnLock(obj);

	return len - remain;
}

/**
 * move the file pointer
 * \param[in] obj uffs object
 * \param[in] offset offset from origin
 * \param[in] origin the origin position, one of:
 * \return return the new file pointer position
 */
long uffs_SeekObject(uffs_Object *obj, long offset, int origin)
{
	if (obj->type == UFFS_TYPE_DIR) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't seek a dir object!\n");
		return 0;
	}

	uffs_ObjectDevLock(obj);

	switch(origin) {
		case USEEK_CUR:
			if(obj->pos + offset > obj->node->u.file.len) {
				obj->pos = obj->node->u.file.len;
			}
			else {
				obj->pos += offset;
			}
			break;
		case USEEK_SET:
			if(offset > (long) obj->node->u.file.len) {
				obj->pos = obj->node->u.file.len;
			}
			else {
				obj->pos = offset;
			}
			break;
		case USEEK_END:
			if ( offset>0 ) {
				obj->pos = obj->node->u.file.len;
			}
			else if((offset >= 0 ? offset : -offset) > (long) obj->node->u.file.len) {
				obj->pos = 0;
			}
			else {
				obj->pos = obj->node->u.file.len + offset;
			}
			break;
	}

	uffs_ObjectDevUnLock(obj);

	return (long) obj->pos;
}

/**
 * return current file pointer
 * \param[in] obj uffs object
 * \return return the file pointer position if the obj is valid, return -1 if obj is invalid.
 */
int uffs_GetCurOffset(uffs_Object *obj)
{
	if (obj) {
		if (obj->dev && obj->openSucc == U_TRUE)
			return obj->pos;
	}
	return -1;
}

/**
 * check whether the file pointer is at the end of file
 * \param[in] obj uffs object
 * \return return 1 if file pointer is at the end of file, return -1 if error occur, else return 0.
 */
int uffs_EndOfFile(uffs_Object *obj)
{
	if (obj) {
		if (obj->dev && obj->type == UFFS_TYPE_FILE && obj->openSucc == U_TRUE) {
			if (obj->pos >= obj->node->u.file.len) {
				return 1;
			}
			else {
				return 0;
			}
		}
	}

	return -1;
}

static URET _CoverOnePage(uffs_Device *dev,
						  uffs_Tags *old,
						  uffs_Tags *newTag,
						  u16 newBlock,
						  u16 page,
						  int newTimeStamp,
						  uffs_Buf *buf,
						  u32 length)
{
	newTag->father = buf->father;
	newTag->serial = buf->serial;
	newTag->type = buf->type;
	newTag->blockTimeStamp = newTimeStamp;
	newTag->dataLength = length;
	newTag->dataSum = old->dataSum;
	newTag->pageID = (u8)(buf->pageID);


	return uffs_WriteDataToNewPage(dev, newBlock, page, newTag, buf);
}

static URET _TruncateInternalWithBlockRecover(uffs_Object *obj, u16 fdn, u32 remain)
{
	uffs_Device *dev = obj->dev;
	TreeNode *fnode = obj->node;
	u16 pageID, maxPageID;
	TreeNode *node, *newNode = NULL;
	u16 block = UFFS_INVALID_BLOCK, newBlock = UFFS_INVALID_BLOCK;
	uffs_blockInfo *bc = NULL, *newBc = NULL;
	uffs_Buf *buf = NULL;
	uffs_Tags *tag, *newTag;
	URET ret = U_FAIL;
	u8 type = UFFS_TYPE_RESV;
	u32 startOfBlock;
	u32 end;
	int timeStamp;
	u16 page;

	if(fdn == 0) {
		node = fnode;
		block = node->u.file.block;
		type = UFFS_TYPE_FILE;
		maxPageID = obj->pagesOnHead;
	}
	else {
		node = uffs_FindDataNode(dev, fnode->u.file.serial, fdn);
		if(node == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't find data node when truncate obj\n");
			goto _err;
		}
		block = node->u.data.block;
		type = UFFS_TYPE_DATA;
		maxPageID = dev->attr->pages_per_block - 1;
	}


	bc = uffs_GetBlockInfo(dev, block);
	if(bc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get block info when truncate obj\n");
		goto _err;
	}

	newNode = uffs_GetErased(dev);
	if(newNode == NULL) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"insufficient erased block, can't truncate obj.\n");
		goto _err;
	}
	newBlock = newNode->u.list.block;
	newBc = uffs_GetBlockInfo(dev, newBlock);
	if(newBc == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get block info when truncate obj\n");
		goto _err;
	}

	startOfBlock = _GetStartOfDataBlock(obj, fdn);
	timeStamp = uffs_GetBlockTimeStamp(dev, bc);
	timeStamp = uffs_GetNextBlockTimeStamp(timeStamp);

	for(pageID = 0; pageID <= maxPageID; pageID++) {
		page = uffs_FindPageInBlockWithPageId(dev, bc, pageID);
		if(page == UFFS_INVALID_PAGE) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"unknown error, truncate\n");
			break;
		}
		page = uffs_FindBestPageInBlock(dev, bc, page);
		buf = uffs_BufClone(dev, NULL);
		if(buf == NULL) {
			uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't clone page buffer\n");
			goto _err;
		}
		tag = &(bc->spares[page].tag);
		uffs_LoadPhiDataToBuf(dev, buf, bc->blockNum, page);

		buf->father = tag->father;
		buf->serial = tag->serial;
		buf->type = tag->type;
		buf->pageID = tag->pageID;
		buf->dataLen = tag->dataLength;

		newTag = &(newBc->spares[pageID].tag);
		if(fdn == 0 && pageID == 0) {
			//copy the page file information
			ret = _CoverOnePage(dev, tag, newTag, newBlock, pageID, timeStamp, buf, buf->dataLen);
			if(ret != U_SUCC) break;
		}
		else {
			end = ((fdn == 0) ? (pageID - 1) * dev->com.pgDataSize :
					pageID * dev->com.pgDataSize);
			end += tag->dataLength;
			end += startOfBlock;

			if(remain > end) {
				if(tag->dataLength != dev->com.pgDataSize) {
					uffs_Perror(UFFS_ERR_NOISY, PFX" ???? unknown error when truncate. \n");
					break;
				}
				ret = _CoverOnePage(dev, tag, newTag, newBlock, pageID, timeStamp, buf, buf->dataLen);
				if(ret != U_SUCC) break;
			}
			else if(remain == end) {
				ret = _CoverOnePage(dev, tag, newTag, newBlock, pageID, timeStamp, buf, buf->dataLen);
				if(ret != U_SUCC) break;
			}
			else if(remain < end) {
				buf->dataLen = tag->dataLength - (end - remain);
				if(buf->dataLen == 0) {
					ret = U_SUCC;
					break;
				}
				memset(buf->data + buf->dataLen, 0, dev->com.pgDataSize - buf->dataLen);
				ret = _CoverOnePage(dev, tag, newTag, newBlock, pageID, timeStamp, buf, buf->dataLen);
				break;
			}
		}
		uffs_BufFreeClone(dev, buf);
		buf = NULL;
	}

_err:
	if(buf != NULL) {
		uffs_BufFreeClone(dev, buf);
		buf = NULL;
	}
	if(ret == U_SUCC) {
		//ok, modify the tree, and erase old block
		//NOTE: Don't delete the 'old' node from tree, just replace the 'block' with new block,
		//      so that we don't need to modify obj->node :)
		uffs_SetTreeNodeBlock(type, node, newNode->u.list.block);
		newNode->u.list.block = block;
		dev->ops->EraseBlock(dev, newNode->u.list.block);
		uffs_InsertToErasedListTail(dev, newNode);
	}
	else {
		//fail to cover block, so erase new block
		dev->ops->EraseBlock(dev, newBlock);
		uffs_InsertToErasedListTail(dev, newNode);
	}

	if(bc) uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);
	if(bc) uffs_PutBlockInfo(dev, bc);
	if(newBc) uffs_ExpireBlockInfo(dev, newBc, UFFS_ALL_PAGES);
	if(newBc) uffs_PutBlockInfo(dev, newBc);

	return U_SUCC;
}

URET uffs_TruncateObject(uffs_Object *obj, u32 remain)
{
	URET ret;

	uffs_ObjectDevLock(obj);
	ret = _TruncateObject(obj, remain);
	uffs_ObjectDevUnLock(obj);

	return ret;
}

/** truncate obj without lock device */
static URET _TruncateObject(uffs_Object *obj, u32 remain)
{
	uffs_Device *dev = obj->dev;
	TreeNode *fnode = obj->node;
	u16 fdn;
	u32 flen;
	u32 startOfBlock;
	TreeNode *node;
	uffs_blockInfo *bc;
	uffs_Buf *buf;
	u16 page;

	if (obj == NULL) return U_FAIL;
	if (obj->dev == NULL || obj->openSucc == U_FALSE) return U_FAIL;

	/* do nothing if the obj is a dir */
	/* TODO: delete files under dir ? */
	if (obj->type == UFFS_TYPE_DIR) {
		obj->err = UEEXIST;
		return U_FAIL;
	}

	if(remain > fnode->u.file.len) return U_FAIL;

	uffs_BufFlushAll(dev); //flush dirty buffers first

	while(fnode->u.file.len > remain) {
		flen = fnode->u.file.len;
		fdn = _GetFdnByOfs(obj, flen - 1);

		startOfBlock = _GetStartOfDataBlock(obj, fdn);
		if(remain <= startOfBlock && fdn > 0) {
			node = uffs_FindDataNode(dev, fnode->u.file.serial, fdn);
			if(node == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't find data node when trancate obj.\n");
				return U_FAIL;
			}
			bc = uffs_GetBlockInfo(dev, node->u.data.block);
			if(bc == NULL) {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get block info when trancate obj.\n");
				return U_FAIL;
			}
			for(page = 0; page < dev->attr->pages_per_block; page++) {
				buf = uffs_BufFind(dev, fnode->u.file.serial, fdn, page);
				if(buf) uffs_BufSetMark(buf, UFFS_BUF_EMPTY);
			}
			uffs_ExpireBlockInfo(dev, bc, UFFS_ALL_PAGES);
			dev->ops->EraseBlock(dev, node->u.data.block);
			uffs_BreakFromEntry(dev, UFFS_TYPE_DATA, node);
			node->u.list.block = bc->blockNum;
			uffs_PutBlockInfo(dev, bc);
			uffs_InsertToErasedListTail(dev, node);
			fnode->u.file.len = startOfBlock;
		}
		else {
			if(_TruncateInternalWithBlockRecover(obj, fdn, remain) == U_SUCC) {
				fnode->u.file.len = remain;
			}
			else {
				uffs_Perror(UFFS_ERR_SERIOUS, PFX"fail to truncate obj\n");
				return U_FAIL;
			}
		}
	}

	if (HAVE_BADBLOCK(dev)) uffs_RecoverBadBlock(dev);

	return U_SUCC;

}


/**
 * delete uffs object
 */
URET uffs_DeleteObject(const char * name)
{
	uffs_Object *obj;
	TreeNode *node;
	uffs_Device *dev;
	u16 block;
	uffs_Buf *buf;
	URET ret = U_FAIL;

	obj = uffs_GetObject();
	if (obj == NULL) goto err1;

	if (uffs_OpenObject(obj, name, UO_RDWR|UO_DIR, US_IREAD|US_IWRITE) == U_FAIL) {
		if (uffs_OpenObject(obj, name, UO_RDWR, US_IREAD|US_IWRITE) == U_FAIL)
			goto err1;
	}

	uffs_TruncateObject(obj, 0);

	uffs_ObjectDevLock(obj);
	dev = obj->dev;

	if (obj->type == UFFS_TYPE_DIR) {
		// if the dir is not empty, can't delete it.
		node = uffs_FindDirNodeFromTreeWithFather(dev, obj->serial);
		if (node != NULL) goto err;  //have sub dirs ?

		node = uffs_FindFileNodeFromTreeWithFather(dev, obj->serial);
		if (node != NULL) goto err;  //have sub files ?
	}

	block = GET_BLOCK_FROM_NODE(obj);
	node = obj->node;

	// before erase the block, we need to take care of the buffer ...
	uffs_BufFlushAll(dev);
	if (HAVE_BADBLOCK(dev)) uffs_RecoverBadBlock(dev);

	buf = uffs_BufFind(dev, obj->father, obj->serial, 0);
	if (buf) {
		//need to expire this buffer ...
		if (buf->refCount != 0) {
			//there is other obj for this file still in use ?
			uffs_Perror(UFFS_ERR_NORMAL, PFX"Try to delete object but still have buf referenced.\n");
			goto err;
		}
		buf->mark = UFFS_BUF_EMPTY; //!< make this buffer expired.
	}
	//FIXME !!: need to take care of other obj->node ?
	uffs_BreakFromEntry(dev, obj->type, node);
	dev->ops->EraseBlock(dev, block); //!< ok, the object is deleted now.
	node->u.list.block = block;
	uffs_InsertToErasedListTail(dev, node);

	ret = U_SUCC;
err:
	uffs_ObjectDevUnLock(obj);
err1:
	_ReleaseObjectResource(obj);

	uffs_PutObject(obj);

	return ret;
}

URET uffs_RenameObject(const char *old_name, const char *new_name)
{
	uffs_Object *obj = NULL, *obj_new = NULL;
	uffs_Device *dev;
	TreeNode *node;
	uffs_Buf *buf;
	uffs_fileInfo fi;
	int pos, pos1, len, new_len, old_len;
	u16 father_new;
	URET ret = U_FAIL;

	obj = uffs_GetObject();
	obj_new = uffs_GetObject();

	if (!obj || !obj_new) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"Can't allocate uffs_Object.\n");
		return U_FAIL;
	}

	new_len = strlen(new_name);
	old_len = strlen(old_name);

	if(old_len >= MAX_PATH_LENGTH ||
		new_len >= MAX_PATH_LENGTH) return U_FAIL;

	if ((old_name[old_len-1] == '/' && new_name[new_len-1] != '/') ||
		(old_name[old_len-1] != '/' && new_name[new_len-1] == '/')) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't rename object between file and dir.\n");
		goto err_exit;
	}

	pos = find_maxMatchedMountPoint(old_name);
	pos1 = find_maxMatchedMountPoint(new_name);
	if (pos <= 0 || pos <= 0 || pos != pos1 || strncmp(old_name, new_name, pos) != 0) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't moving object to different mount point\n");
		goto err_exit;
	}

	/* check whether the new object exist ? */
	memset(obj, 0, sizeof(uffs_Object));
	if (new_name[new_len-1] == '/') {
		if (uffs_OpenObject(obj, new_name, UO_DIR|UO_RDONLY, US_IREAD) == U_SUCC) {
			uffs_CloseObject(obj);
			goto err_exit;
		}
	}
	else {
		if (uffs_OpenObject(obj, new_name, UO_RDONLY, US_IREAD) == U_SUCC) {
			uffs_CloseObject(obj);
			goto err_exit;
		}
	}

	/* check whether the old object exist ? */
	memset(obj, 0, sizeof(uffs_Object));
	if (old_name[old_len-1] == '/') {
		if (uffs_OpenObject(obj, old_name, UO_DIR|UO_RDONLY, US_IREAD) == U_FAIL) {
			goto err_exit;
		}
	}
	else {
		if (uffs_OpenObject(obj, old_name, UO_RDONLY, US_IREAD) == U_FAIL) {
			goto err_exit;
		}
	}

	/* get new object's farther's serail No. by prepare create the new object */
	memset(obj_new, 0, sizeof(uffs_Object));
	if (_PrepareOpenObj(obj_new, new_name, UO_RDWR, US_IREAD|US_IWRITE) == U_FAIL) {
		goto err_exit;
	}
	father_new = obj_new->father;
	_ReleaseObjectResource(obj_new);

	dev = obj->dev;
	node = obj->node;

	uffs_ObjectDevLock(obj);

	uffs_BufFlushAll(dev);

	buf = uffs_BufGetEx(dev, obj->type, node, 0);
	if(buf == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"can't get buf when rename!\n");
		uffs_ObjectDevUnLock(obj);
		uffs_CloseObject(obj);
		goto err_exit;
	}

	memcpy(&fi, buf->data, sizeof(uffs_fileInfo));
	if (new_name[new_len-1] == '/') {
		pos = back_search_slash(new_name, new_len - 2);
		len = new_len - 2 - pos;
	}
	else {
		pos = back_search_slash(new_name, new_len - 1);
		len = new_len - 1 - pos;
	}
	memcpy(fi.name, new_name + pos + 1, len);
	fi.name[len] = 0;
	fi.name_len = len;
	fi.lastModify = uffs_GetCurDateTime();

	obj->sum = uffs_MakeSum16(fi.name, fi.name_len);

	//update the check sum of tree node
	if (obj->type == UFFS_TYPE_DIR) {
		obj->node->u.dir.checkSum = obj->sum;
		obj->node->u.dir.father = father_new;
	}
	else {
		obj->node->u.file.checkSum = obj->sum;
		obj->node->u.file.father = father_new;
	}

	buf->father = father_new;	// !! need to manually change the 'father' !!

	uffs_BufWrite(dev, buf, &fi, 0, sizeof(uffs_fileInfo));
	uffs_BufPut(dev, buf);

	// FIXME: take care of dirty group !
	uffs_BufFlushEx(dev, U_TRUE);	// !! force a block recover so that all old tag will be expired !!
									// This is important so we only need to check the first spare when mount UFFS :)

	uffs_ObjectDevUnLock(obj);
	uffs_CloseObject(obj);

	ret = U_SUCC;

err_exit:
	if (obj) uffs_PutObject(obj);
	if (obj_new) uffs_PutObject(obj_new);

	return ret;
}


static URET _LoadObjectInfo(uffs_Device *dev, TreeNode *node, uffs_ObjectInfo *info, int type)
{
	uffs_Buf *buf;

	buf = uffs_BufGetEx(dev, (u8)type, node, 0);

	if(buf == NULL) {
		return U_FAIL;
	}

	memcpy(&(info->info), buf->data, sizeof(uffs_fileInfo));

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

URET uffs_GetObjectInfo(uffs_Object *obj, uffs_ObjectInfo *info)
{
	uffs_Device *dev = obj->dev;
	URET ret = U_FAIL;

	uffs_ObjectDevLock(obj);

	if (obj && obj->dev && info)
		ret = _LoadObjectInfo(dev, obj->node, info, obj->type);

	uffs_ObjectDevUnLock(obj);

	return ret;
}

/* find objects */
URET uffs_OpenFindObject(uffs_FindInfo *f, const char * dir)
{
	if (f == NULL) return U_FAIL;

	f->obj = uffs_GetObject();
	if (f->obj == NULL) {
		uffs_Perror(UFFS_ERR_SERIOUS, PFX"Can't get a new object\n");
		goto err;
	}

	if (_PrepareOpenObj(f->obj, dir, UO_RDONLY|UO_DIR, US_IREAD|US_IWRITE) == U_FAIL) {
		uffs_Perror(UFFS_ERR_NOISY, PFX"Can't prepare open dir %s for read.\n", dir);
		goto err;
	}

	f->dev = f->obj->dev;

	if (f->obj->nameLen == 0) {
		// This is the root dir !!!, do not try to open it !
		// obj->father and obj->serial should already prepared.
	}
	else {
		uffs_ObjectDevLock(f->obj);
		if (_OpenObjectUnder(f->obj) == U_FAIL) {
			uffs_ObjectDevUnLock(f->obj);
			uffs_Perror(UFFS_ERR_NOISY, PFX"Can't open dir %s, doesn't exist ?\n", dir);
			goto err;
		}
		uffs_ObjectDevUnLock(f->obj);
	}

	f->hash = 0;
	f->work = NULL;
	f->step = 0;

	return U_SUCC;

err:
	if (f->obj) {
		_ReleaseObjectResource(f->obj);
		uffs_PutObject(f->obj);
		f->obj = NULL;
	}

	return U_FAIL;
}

URET uffs_FindFirstObject(uffs_ObjectInfo * info, uffs_FindInfo * f)
{
	uffs_Device *dev = f->dev;
	TreeNode *node;
	u16 x;
	URET ret = U_SUCC;

	uffs_DeviceLock(dev);
	f->step = 0;

	if (f->step == 0) {
		for(f->hash = 0;
			f->hash < DIR_NODE_ENTRY_LEN;
			f->hash++) {

			x = dev->tree.dirEntry[f->hash];

			while(x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if(node->u.dir.father == f->obj->serial) {
					f->work = node;
					if (info) ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_DIR);
					uffs_DeviceUnLock(dev);
					return ret;
				}
				x = node->hashNext;
			}
		}

		//no subdirs, then lookup the files
		f->step++;
	}

	if(f->step == 1) {
		for(f->hash = 0;
			f->hash < FILE_NODE_ENTRY_LEN;
			f->hash++) {

			x = dev->tree.fileEntry[f->hash];

			while(x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if(node->u.file.father == f->obj->serial) {
					f->work = node;
					if(info) ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_FILE);
					uffs_DeviceUnLock(dev);
					return ret;
				}
				x = node->hashNext;
			}
		}
		f->step++;
	}

	uffs_DeviceUnLock(dev);
	return U_FAIL;
}

URET uffs_FindNextObject(uffs_ObjectInfo *info, uffs_FindInfo * f)
{
	uffs_Device *dev = f->dev;
	TreeNode *node;
	u16 x;
	URET ret = U_SUCC;

	if(f->dev == NULL ||
		f->work == NULL ||
		f->step > 1) return U_FAIL;

	uffs_DeviceLock(dev);

	x = f->work->hashNext;

	if (f->step == 0) { //!< working on dirs
		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(dev->tree.dis));
			if(node->u.dir.father == f->obj->serial) {
				f->work = node;
				if(info) ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_DIR);
				uffs_DeviceUnLock(dev);
				return ret;
			}
			x = node->hashNext;
		}

		f->hash++; //come to next hash entry

		for(; f->hash < DIR_NODE_ENTRY_LEN; f->hash++) {
			x = dev->tree.dirEntry[f->hash];
			while(x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if(node->u.dir.father == f->obj->serial) {
					f->work = node;
					if(info) ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_DIR);
					uffs_DeviceUnLock(dev);
					return ret;
				}
				x = node->hashNext;
			}
		}

		//no subdirs, then lookup files ..
		f->step++;
		f->hash = 0;
		x = EMPTY_NODE;
	}

	if (f->step == 1) {

		while(x != EMPTY_NODE) {
			node = FROM_IDX(x, &(dev->tree.dis));
			if(node->u.file.father == f->obj->serial) {
				f->work = node;
				if(info) ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_FILE);
				uffs_DeviceUnLock(dev);
				return ret;
			}
			x = node->hashNext;
		}

		f->hash++; //come to next hash entry

		for(; f->hash < FILE_NODE_ENTRY_LEN; f->hash++) {
			x = dev->tree.fileEntry[f->hash];
			while(x != EMPTY_NODE) {
				node = FROM_IDX(x, &(dev->tree.dis));
				if(node->u.file.father == f->obj->serial) {
					f->work = node;
					if(info) ret = _LoadObjectInfo(dev, node, info, UFFS_TYPE_FILE);
					uffs_DeviceUnLock(dev);
					return ret;
				}
				x = node->hashNext;
			}
		}

		//no any files, stopped.
		f->step++;
	}

	uffs_DeviceUnLock(dev);
	return U_FAIL;
}

URET uffs_CloseFindObject(uffs_FindInfo * f)
{
	if (f == NULL) return U_FAIL;

	if (f->obj) {
		/* close dir */
		_ReleaseObjectResource(f->obj);
		uffs_PutObject(f->obj);
		f->obj = NULL;
	}

	f->dev = NULL;
	f->hash = 0;
	f->work = NULL;

	return U_SUCC;
}


