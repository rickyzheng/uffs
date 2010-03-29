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
 * \file uffs_fd.c
 * \brief POSIX like, hight level file operations
 * \author Ricky Zheng, created 8th Jun, 2005
 */

#include "uffs/uffs_fs.h"
#include "uffs/uffs_config.h"
#include <string.h>

#define PFX "fd: "


static int _dir_pool_data[sizeof(uffs_DIR) * MAX_DIR_HANDLE / sizeof(int)];
static uffs_Pool _dir_pool;
static int _uffs_errno = 0;

#define FD_OFFSET		3	//!< just make file handler more like POSIX (0, 1, 2 for stdin/stdout/stderr)

#define FD2OBJ(fd)	(((fd) >= FD_OFFSET && (fd) < MAX_DIR_HANDLE + FD_OFFSET) ? \
						(uffs_Object *)uffs_PoolGetBufByIndex(&_dir_pool, (fd) - FD_OFFSET) : NULL )

#define OBJ2FD(obj)		(uffs_PoolGetIndex(&_dir_pool, obj) + FD_OFFSET)

#define CHK_OBJ(obj, ret)	do { \
								if (uffs_PoolCheckFreeList(&_dir_pool, (obj)) == U_FALSE) { \
									uffs_set_error(-UEBADF); \
									return (ret); \
								} \
							} while(0)


/**
 * initialise uffs_DIR buffers, called by UFFS internal
 */
URET uffs_InitDirEntryBuf(void)
{
	return uffs_PoolInit(&_dir_pool, _dir_pool_data, sizeof(_dir_pool_data),
			sizeof(uffs_DIR), MAX_DIR_HANDLE);
}

/**
 * Release uffs_DIR buffers, called by UFFS internal
 */
URET uffs_ReleaseDirEntryBuf(void)
{
	return uffs_PoolRelease(&_dir_pool);
}


/** get global errno
 */
int uffs_get_error(void)
{
	return _uffs_errno;
}

/** set global errno
 */
int uffs_set_error(int err)
{
	return (_uffs_errno = err);
}

/* POSIX compliant file system APIs */

int uffs_open(const char *name, int oflag, ...)
{
	uffs_Object *obj;
	int ret = 0;

	obj = uffs_GetObject();
	if (obj == NULL) {
		uffs_set_error(-UEMFILE);
		ret = -1;
	}
	else {
		if (uffs_OpenObject(obj, name, oflag) == U_FAIL) {
			uffs_set_error(-uffs_GetObjectErr(obj));
			uffs_PutObject(obj);
			ret = -1;
		}
		else {
			ret = OBJ2FD(obj);
		}
	}

	return ret;
}

int uffs_close(int fd)
{
	int ret = 0;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);

	uffs_ClearObjectErr(obj);
	if (uffs_CloseObject(obj) == U_FAIL) {
		uffs_set_error(-uffs_GetObjectErr(obj));
		ret = -1;
	}
	else {
		uffs_PutObject(obj);
		ret = 0;
	}

	return ret;
}

int uffs_read(int fd, void *data, int len)
{
	int ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = uffs_ReadObject(obj, data, len);
	uffs_set_error(-uffs_GetObjectErr(obj));

	return ret;
}

int uffs_write(int fd, void *data, int len)
{
	int ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = uffs_WriteObject(obj, data, len);
	uffs_set_error(-uffs_GetObjectErr(obj));

	return ret;
}

long uffs_seek(int fd, long offset, int origin)
{
	int ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = uffs_SeekObject(obj, offset, origin);
	uffs_set_error(-uffs_GetObjectErr(obj));
	
	return ret;
}

long uffs_tell(int fd)
{
	long ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = (long) uffs_GetCurOffset(obj);
	uffs_set_error(-uffs_GetObjectErr(obj));
	
	return ret;
}

int uffs_eof(int fd)
{
	int ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = uffs_EndOfFile(obj);
	uffs_set_error(-uffs_GetObjectErr(obj));
	
	return ret;
}

int uffs_flush(int fd)
{
	int ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = (uffs_FlushObject(obj) == U_SUCC) ? 0 : -1;
	uffs_set_error(-uffs_GetObjectErr(obj));
	
	return ret;
}

int uffs_rename(const char *old_name, const char *new_name)
{
	int err = 0;
	if (uffs_RenameObject(old_name, new_name, &err) == U_SUCC) {
		return 0;
	}
	else {
		uffs_set_error(-err);
		return -1;
	}
}

int uffs_remove(const char *name)
{
	int err = 0;
	if (uffs_DeleteObject(name, &err) == U_SUCC) {
		return 0;
	}
	else {
		uffs_set_error(-err);
		return -1;
	}
}

int uffs_truncate(int fd, long remain)
{
	int ret;
	uffs_Object *obj = FD2OBJ(fd);

	CHK_OBJ(obj, -1);
	uffs_ClearObjectErr(obj);
	ret = (uffs_TruncateObject(obj, remain) == U_SUCC) ? 0 : -1;
	uffs_set_error(-uffs_GetObjectErr(obj));
	
	return ret;
}

int uffs_stat(const char *filename, struct uffs_stat *buf);
int uffs_lstat(const char *filename, struct uffs_stat *buf);
int uffs_fstat(int fd, struct uffs_stat *buf);

int uffs_closedir(uffs_DIR *dirp);
uffs_DIR * uffs_opendir(const char *path);
struct uffs_dirent * uffs_readdir(uffs_DIR *dirp);

int uffs_readdir_r(uffs_DIR *restrict, struct uffs_dirent *restrict,
                   struct uffs_dirent **restrict);
void uffs_rewinddir(uffs_DIR *dirp);

void uffs_seekdir(uffs_DIR *dirp, long loc);
long uffs_telldir(uffs_DIR *dirp);

