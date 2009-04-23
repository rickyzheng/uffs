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
 * \file uffs.h
 * \brief uffs basic defines
 * \author Ricky Zheng
 */

#ifndef _UFFS_H_
#define _UFFS_H_

#include "uffs/uffs_types.h"

#ifdef __cplusplus
extern "C"{
#endif


#define ENCODE_MBCS		1
#define ENCODE_UNICODE	2
#define UFFS_DEFAULT_ENCODE		ENCODE_MBCS



#define US_IWRITE		0x0000200
#define US_IREAD		0x0000400

#define UO_RDONLY		0x0000		//read only
#define UO_WRONLY		0x0001		//write only
#define UO_RDWR			0x0002		//read and write
#define UO_APPEND		0x0008		//append

#define UO_BINARY		0x0000		//no used in uffs

#define UO_CREATE		0x0100
#define UO_TRUNC		0x0200
#define UO_EXCL			0x0400		//

#define UO_DIR			0x1000		//open a directory



#define UENOERR 0		/* no error */
#define UEACCES	1		/* Tried to open read-only file
						 for writing, or files sharing mode
						 does not allow specified operations,
						 or given path is directory */
#define UEEXIST	2		/* _O_CREAT and _O_EXCL flags specified,
							but filename already exists */
#define UEINVAL	3		/* Invalid oflag or pmode argument */
#define UEMFILE	4		/* No more file handles available
						  (too many open files)  */
#define UENOENT	5		/* file or path not found */
#define UETIME	6		/* can't set file time */
#define UEBADF	9		/* invalid file handle */
#define UENOMEM	10		/* no enough memory */
#define UEUNKNOWN	11	/* unknown error */



#define _SEEK_CUR		0		/* seek from current position */
#define _SEEK_SET		1		/* seek from beginning of file */
#define _SEEK_END		2		/* seek from end of file */

#define USEEK_CUR		_SEEK_CUR
#define USEEK_SET		_SEEK_SET
#define USEEK_END		_SEEK_END



/** 
 * \def MAX_FILENAME_LENGTH 
 * \note Be careful: it's part of the physical format (see: uffs_fileInfoSt.name)
 *    !!DO NOT CHANGE IT AFTER FILE SYSTEM IS FORMATED!!
 */
#define MAX_FILENAME_LENGTH			32

#define FILE_ATTR_DIR		(1 << 24)	//!< attribute for directory
#define FILE_ATTR_WRITE		(1 << 0)	//!< writable


/** file info in physical format */
struct uffs_fileInfoSt {
	u32 attr;
	u32 createTime;
	u32 lastModify;
	u32 access;
	u32 reserved;
	u32 name_len;
	char name[MAX_FILENAME_LENGTH];
};

typedef struct uffs_fileInfoSt uffs_fileInfo;

typedef struct uffs_ObjectInfoSt {
	uffs_fileInfo info;
	u32 len;
	u16 serial;
} uffs_ObjectInfo;


#ifdef __cplusplus
}
#endif


#endif

