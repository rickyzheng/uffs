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
 * \file test_cmds.c
 * \brief commands for test uffs
 * \author Ricky Zheng
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uffs/uffs_config.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_fd.h"
#include "uffs/uffs_utils.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs_mtb.h"
#include "uffs/uffs_find.h"
#include "uffs/uffs_badblock.h"
#include "cmdline.h"

#define PFX "test:"


static BOOL cmdTestPageReadWrite(const char *tail)
{
	TreeNode *node;
	uffs_Device *dev;
	uffs_Tags local_tag;
	uffs_Tags *tag = &local_tag;
	int ret;
	u16 block;
	u16 page;
	uffs_Buf *buf;

	u32 i;

	dev = uffs_GetDeviceFromMountPoint("/");
	if (!dev)
		goto ext;

	buf = uffs_BufClone(dev, NULL);
	if (!buf)
		goto ext;

	node = uffs_TreeGetErasedNode(dev);
	if (!node) {
		uffs_Perror(UFFS_ERR_SERIOUS, "no free block ?");
		goto ext;
	}

	for (i = 0; i < dev->com.pg_data_size; i++) {
		buf->data[i] = i & 0xFF;
	}

	block = node->u.list.block;
	page = 1;

	TAG_DATA_LEN(tag) = dev->com.pg_data_size;
	TAG_TYPE(tag) = UFFS_TYPE_DATA;
	TAG_PAGE_ID(tag) = 3;
	TAG_PARENT(tag) = 100;
	TAG_SERIAL(tag) = 10;
	TAG_BLOCK_TS(tag) = 1;

	ret = uffs_FlashWritePageCombine(dev, block, page, buf, tag);
	if (UFFS_FLASH_HAVE_ERR(ret)) {
		uffs_Perror(UFFS_ERR_SERIOUS, "Write page error: %d", ret);
		goto ext;
	}

	ret = uffs_FlashReadPage(dev, block, page, buf);
	if (UFFS_FLASH_HAVE_ERR(ret)) {
		uffs_Perror(UFFS_ERR_SERIOUS, "Read page error: %d", ret);
		goto ext;
	}

	for (i = 0; i < dev->com.pg_data_size; i++) {
		if (buf->data[i] != (i & 0xFF)) {
			uffs_Perror(UFFS_ERR_SERIOUS, "Data verify fail at: %d", i);
			goto ext;
		}
	}

	ret = uffs_FlashReadPageTag(dev, block, page, tag);
	if (UFFS_FLASH_HAVE_ERR(ret)) {
		uffs_Perror(UFFS_ERR_SERIOUS, "Read tag (page spare) error: %d", ret);
		goto ext;
	}
	
	// verify tag:
	if (!TAG_IS_DIRTY(tag)) {
		uffs_Perror(UFFS_ERR_SERIOUS, "not dirty ? Tag verify fail!");
		goto ext;
	}

	if (!TAG_IS_VALID(tag)) {
		uffs_Perror(UFFS_ERR_SERIOUS, "not valid ? Tag verify fail!");
		goto ext;
	}

	if (TAG_DATA_LEN(tag) != dev->com.pg_data_size ||
		TAG_TYPE(tag) != UFFS_TYPE_DATA ||
		TAG_PAGE_ID(tag) != 3 ||
		TAG_PARENT(tag) != 100 ||
		TAG_SERIAL(tag) != 10 ||
		TAG_BLOCK_TS(tag) != 1) {

		uffs_Perror(UFFS_ERR_SERIOUS, "Tag verify fail!");
		goto ext;
	}

	uffs_Perror(UFFS_ERR_SERIOUS, "Page read/write test succ.");

ext:
	if (node) {
		uffs_FlashEraseBlock(dev, node->u.list.block);
		if (HAVE_BADBLOCK(dev))
			uffs_BadBlockProcess(dev, node);
		else
			uffs_InsertToErasedListHead(dev, node);
	}

	if (dev)
		uffs_PutDevice(dev);

	if (buf)
		uffs_BufFreeClone(dev, buf);

	return TRUE;
}

static BOOL cmdTestFormat(const char *tail)
{
	URET ret;
	const char *mount = "/";
	uffs_Device *dev;
	const char *next;
	UBOOL force = U_FALSE;
	const char *test_file = "/a.txt";
	int fd;

	if (tail) {
		mount = cli_getparam(tail, &next);
		if (next && strcmp(next, "-f") == 0)
			force = U_TRUE;
	}

	fd = uffs_open(test_file, UO_RDWR | UO_CREATE);
	if (fd < 0) {
		uffs_Perror(UFFS_ERR_NORMAL, "can't create test file %s", test_file);
		return U_TRUE;
	}

	uffs_Perror(UFFS_ERR_NORMAL, "Formating %s ... ", mount);

	dev = uffs_GetDeviceFromMountPoint(mount);
	if (dev == NULL) {
		uffs_Perror(UFFS_ERR_NORMAL, "Can't get device from mount point.");
	}
	else {
		ret = uffs_FormatDevice(dev, force);
		if (ret != U_SUCC) {
			uffs_Perror(UFFS_ERR_NORMAL, "Format fail.");
		}
		else {
			uffs_Perror(UFFS_ERR_NORMAL, "Format succ.");
		}
		uffs_PutDevice(dev);
	}

	uffs_close(fd);  // this should fail on signature check !

	return TRUE;
}



/**
 * usage: t_pfs <start> <n>
 *
 * for example: t_pfs /x/ 100
 *
 * This test case performs:
 *   1) create <n> files under <start>, write full file name as file content
 *   2) list files under <start>, check files are all listed once
 *   3) check file content aganist file name
 *   4) delete files on success
 */
static BOOL cmdTestPopulateFiles(const char *tail)
{
	const char *start = "/";
	int count = 80;
	int i, fd, num;
	char name[128];
	char buf[128];
	uffs_DIR *dirp;
	struct uffs_dirent *ent;
	struct uffs_stat stat_buf;
	unsigned long bitmap[50] = {0};	// one bit per file, maximu 32*50 = 1600 files
	UBOOL succ = U_TRUE;

#define SBIT(n) bitmap[(n)/(sizeof(bitmap[0]) * 8)] |= (1 << ((n) % (sizeof(bitmap[0]) * 8)))
#define GBIT(n) (bitmap[(n)/(sizeof(bitmap[0]) * 8)] & (1 << ((n) % (sizeof(bitmap[0]) * 8))))

	if (tail) {
		start = cli_getparam(tail, &tail);
		if (tail) {
			count = strtol(tail, NULL, 10);
		}	
	}

	if (count > sizeof(bitmap) * 8)
		count = sizeof(bitmap) * 8;

	for (i = 0, fd = -1; i < count; i++) {
		sprintf(name, "%sFile%03d", start, i);
		fd = uffs_open(name, UO_RDWR|UO_CREATE|UO_TRUNC);
		if (fd < 0) {
			uffs_Perror(UFFS_ERR_NORMAL, "Create file %s failed", name);
			break;
		}
		if (uffs_write(fd, name, strlen(name)) != strlen(name)) { // write full path name to file
			uffs_Perror(UFFS_ERR_NORMAL, "Write to file %s failed", name);
			uffs_close(fd);
			break;
		}
		uffs_close(fd);
	}

	if (i < count) {
		// not success, need to clean up
		for (; i >= 0; i--) {
			sprintf(name, "%sFile%03d", start, i);
			if (uffs_remove(name) < 0)
				uffs_Perror(UFFS_ERR_NORMAL, "Delete file %s failed", name);
		}
		succ = U_FALSE;
		goto ext;
	}

	uffs_Perror(UFFS_ERR_NORMAL, "%d files created.", count);

	// list files
	dirp = uffs_opendir(start);
	if (dirp == NULL) {
		uffs_Perror(UFFS_ERR_NORMAL, "Can't open dir %s !", start);
		succ = U_FALSE;
		goto ext;
	}
	ent = uffs_readdir(dirp);
	while (ent && succ) {

		if (!(ent->d_type & FILE_ATTR_DIR) &&					// not a dir
			ent->d_namelen == strlen("File000") &&				// check file name length
			memcmp(ent->d_name, "File", strlen("File")) == 0) {	// file name start with "File"
			
			uffs_Perror(UFFS_ERR_NORMAL, "List entry %s", ent->d_name);

			num = strtol(ent->d_name + 4, NULL, 10);
			if (GBIT(num)) {
				// file already listed ?
				uffs_Perror(UFFS_ERR_NORMAL, "File %d listed twice !", ent->d_name);
				succ = U_FALSE;
				break;
			}
			SBIT(num);

			// check file content
			sprintf(name, "%s%s", start, ent->d_name);
			fd = uffs_open(name, UO_RDONLY);
			if (fd < 0) {
				uffs_Perror(UFFS_ERR_NORMAL, "Open file %d for read failed !", name);
			}
			else {
				memset(buf, 0, sizeof(buf));
				num = uffs_read(fd, buf, sizeof(buf));
				if (num != strlen(name)) {
					uffs_Perror(UFFS_ERR_NORMAL, "%s Read data length expect %d but got %d !", name, strlen(name), num);
					succ = U_FALSE;
				}
				else {
					if (memcmp(name, buf, num) != 0) {
						uffs_Perror(UFFS_ERR_NORMAL, "File %s have wrong content '%s' !", name, buf);
						succ = U_FALSE;
					}
				}
				uffs_close(fd);
			}
		}
		ent = uffs_readdir(dirp);
	}
	uffs_closedir(dirp);

	// check absent files
	for (i = 0; i < count; i++) {
		if (GBIT(i) == 0) {
			sprintf(name, "%sFile%03d", start, i);
			uffs_Perror(UFFS_ERR_NORMAL, "File %s not listed !", name);
			succ = U_FALSE;
		}
	}
	
	// delete files if pass the test
	for (i = 0; succ && i < count; i++) {
		sprintf(name, "%sFile%03d", start, i);
		if (uffs_remove(name) < 0) {
			uffs_Perror(UFFS_ERR_NORMAL, "Delete file %s failed", name);
			succ = U_FALSE;
		}
	}

ext:
	uffs_Perror(UFFS_ERR_NORMAL, "Populate files test %s !", succ ? "SUCC" : "FAILED");
	return U_TRUE;

}


static struct cli_commandset cmdset[] = 
{
    { cmdTestPageReadWrite,		"t_pgrw",		NULL,		"test page read/write" },
    { cmdTestFormat,			"t_format",		NULL,		"test format file system" },
	{ cmdTestPopulateFiles,		"t_pfs",		"[<start> [<n>]]",	"test populate <n> files under <start>" },
    { NULL, NULL, NULL, NULL }
};


struct cli_commandset * get_test_cmds()
{
	return cmdset;
};



