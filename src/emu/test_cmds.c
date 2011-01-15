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

#define MSGLN(msg,...) uffs_Perror(UFFS_ERR_NORMAL, msg, ## __VA_ARGS__)

static URET do_write_test_file(int fd, int size)
{
	long pos;
	unsigned char buf[100];
	unsigned char data;
	int i, len;

	while (size > 0) {
		pos = uffs_seek(fd, 0, USEEK_CUR);
		len = (size > sizeof(buf) ? sizeof(buf) : size);
		data = pos & 0xFF;
		for (i = 0; i < len; i++, data++) {
			buf[i] = data;
		}
		if (uffs_write(fd, buf, len) != len) {
			MSGLN("Write file failed, size %d at %d", len, pos);
			return U_FAIL;
		}
		size -= len;
	}

	return U_SUCC;
}

static URET test_write_file(const char *file_name, int pos, int size)
{
	int ret = U_FAIL;
	int fd = -1;

	if ((fd = uffs_open(file_name, UO_RDWR|UO_CREATE)) < 0) {
		MSGLN("Can't open file %s for write.", file_name);
		goto test_exit;
	}

	if (uffs_seek(fd, pos, USEEK_SET) != pos) {
		MSGLN("Can't seek file %s at pos %d", file_name, pos);
		goto test_failed;
	}

	if (do_write_test_file(fd, size) == U_FAIL) {
		MSGLN("Write file %s failed.", file_name);
		goto test_failed;
	}
	ret = U_SUCC;

test_failed:
	uffs_close(fd);

test_exit:

	return ret;
}


static URET test_verify_file(const char *file_name)
{
	int fd;
	int ret = U_FAIL;
	unsigned char buf[100];
	int i, pos, len;

	if ((fd = uffs_open(file_name, UO_RDONLY)) < 0) {
		MSGLN("Can't open file %s for read.", file_name);
		goto test_exit;
	}

	pos = 0;
	while (!uffs_eof(fd)) {
		len = uffs_read(fd, buf, sizeof(buf));
		if (len <= 0)
			goto test_failed;
		for (i = 0; i < len; i++) {
			if (buf[i] != (pos++ & 0xFF)) {
				pos--;
				MSGLN("Verify file %s failed at: %d, expect %x but got %x", file_name, pos, pos & 0xFF, buf[i]);
				goto test_failed;
			}
		}
	}

	if (pos != uffs_seek(fd, 0, USEEK_END)) {
		MSGLN("Verify file %s failed. invalid file length.", file_name);
		goto test_failed;
	}

	MSGLN("Verify file %s succ.", file_name);
	ret = U_SUCC;

test_failed:
	uffs_close(fd);

test_exit:

	return ret;
}

static URET test_append_file(const char *file_name, int size)
{
	int ret = U_FAIL;
	int fd = -1;

	if ((fd = uffs_open(file_name, UO_RDWR|UO_APPEND|UO_CREATE)) < 0) {
		MSGLN("Can't open file %s for append.", file_name);
		goto test_exit;
	}

	uffs_seek(fd, 0, USEEK_END);

	if (do_write_test_file(fd, size) == U_FAIL) {
		MSGLN("Write file %s failed.", file_name);
		goto test_failed;
	}
	ret = U_SUCC;

test_failed:
	uffs_close(fd);

test_exit:

	return ret;
}


/* test create file, write file and read back */
static BOOL cmdTest1(const char *tail)
{
	int fd;
	URET ret;
	char buf[100];
	const char *name;

	if (!tail) {
		return FALSE;
	}

	name = cli_getparam(tail, NULL);

	fd = uffs_open(name, UO_RDWR|UO_CREATE|UO_TRUNC);
	if (fd < 0) {
		MSGLN("Can't open %s", name);
		goto fail;
	}

	sprintf(buf, "123456789ABCDEF");
	ret = uffs_write(fd, buf, strlen(buf));
	MSGLN("write %d bytes to file, content: %s", ret, buf);

	ret = uffs_seek(fd, 3, USEEK_SET);
	MSGLN("new file position: %d", ret);

	memset(buf, 0, sizeof(buf));
	ret = uffs_read(fd, buf, 5);
	MSGLN("read %d bytes, content: %s", ret, buf);

	uffs_close(fd);

fail:

	return TRUE;
}


static URET DoTest2(void)
{
	int fd = -1;
	URET ret = U_FAIL;
	char buf[100], buf_1[100];

	fd = uffs_open("/abc/", UO_RDWR|UO_DIR);
	if (fd < 0) {
		MSGLN("Can't open dir abc, err: %d", uffs_get_error());
		MSGLN("Try to create a new one...");
		fd = uffs_open("/abc/", UO_RDWR|UO_CREATE|UO_DIR);
		if (fd < 0) {
			MSGLN("Can't create new dir /abc/");
			goto exit_test;
		}
		else {
			uffs_close(fd);
		}
	}
	else {
		uffs_close(fd);
	}
	
	fd = uffs_open("/abc/test.txt", UO_RDWR|UO_CREATE);
	if (fd < 0) {
		MSGLN("Can't open /abc/test.txt");
		goto exit_test;
	}

	sprintf(buf, "123456789ABCDEF");
	ret = uffs_write(fd, buf, strlen(buf));
	MSGLN("write %d bytes to file, content: %s", ret, buf);

	ret = uffs_seek(fd, 3, USEEK_SET);
	MSGLN("new file position: %d", ret);

	memset(buf_1, 0, sizeof(buf_1));
	ret = uffs_read(fd, buf_1, 5);
	MSGLN("read %d bytes, content: %s", ret, buf_1);

	if (memcmp(buf + 3, buf_1, 5) != 0) {
		ret = U_FAIL;
	}
	else {
		ret = U_SUCC;
	}

	uffs_close(fd);

exit_test:

	return ret;
}


static BOOL cmdTest2(const char *tail)
{
	MSGLN("Test return: %s !", DoTest2() == U_SUCC ? "succ" : "failed");

	return TRUE;
}

/* Test file append and 'random' write */
static BOOL cmdTest3(const char *tail)
{
	const char *name;
	int i;
	int write_test_seq[] = { 20, 10, 500, 40, 1140, 900, 329, 4560, 352, 1100 };

	if (!tail) {
		return FALSE;
	}

	name = cli_getparam(tail, NULL);
	MSGLN("Test append file %s ...", name);
	for (i = 1; i < 500; i += 29) {
		if (test_append_file(name, i) != U_SUCC) {
			MSGLN("Append file %s test failed at %d !", name, i);
			return TRUE;
		}
	}

	MSGLN("Check file %s ... ", name);
	if (test_verify_file(name) != U_SUCC) {
		MSGLN("Verify file %s failed.", name);
		return TRUE;
	}

	MSGLN("Test write file ...");
	for (i = 0; i < sizeof(write_test_seq) / sizeof(int) - 1; i++) {
		if (test_write_file(name, write_test_seq[i], write_test_seq[i+1]) != U_SUCC) {
			MSGLN("Test write file failed !");
			return TRUE;
		}
	}

	MSGLN("Check file %s ... ", name);
	if (test_verify_file(name) != U_SUCC) {
		MSGLN("Verify file %s failed.", name);
		return TRUE;
	}

	MSGLN("Test succ !");

	return TRUE;
}

/* open two files and test write */
static BOOL cmdTest4(const char *tail)
{
	int fd1 = -1, fd2 = -1;

	MSGLN("open /a ...");
	if ((fd1 = uffs_open("/a", UO_RDWR | UO_CREATE)) < 0) {
		MSGLN("Can't open /a");
		goto fail_exit;
	}

	MSGLN("open /b ...");
	if ((fd2 = uffs_open("/b", UO_RDWR | UO_CREATE)) < 0) {
		MSGLN("Can't open /b");
		uffs_close(fd1);
		goto fail_exit;
	}

	MSGLN("write (1) to /a ...");
	uffs_write(fd1, "Hello,", 6);
	MSGLN("write (1) to /b ...");
	uffs_write(fd2, "Hello,", 6);
	MSGLN("write (2) to /a ...");
	uffs_write(fd1, "World.", 6);
	MSGLN("write (2) to /b ...");
	uffs_write(fd2, "World.", 6);
	MSGLN("close /a ...");
	uffs_close(fd1);
	MSGLN("close /b ...");
	uffs_close(fd2);

	return TRUE;

fail_exit:
	return TRUE;
}

/* test appending file */
static BOOL cmdTest5(const char *tail)
{
	int fd = -1;
	URET ret;
	char buf[100];
	const char *name;

	if (!tail) {
		return FALSE;
	}

	name = cli_getparam(tail, NULL);

	fd = uffs_open(name, UO_RDWR|UO_APPEND);
	if (fd < 0) {
		MSGLN("Can't open %s", name);
		goto fail;
	}

	sprintf(buf, "append test...");
	ret = uffs_write(fd, buf, strlen(buf));
	if (ret != strlen(buf)) {
		MSGLN("write file failed, %d/%d", ret, strlen(buf));
	}
	else {
		MSGLN("write %d bytes to file, content: %s", ret, buf);
	}

	uffs_close(fd);

fail:

	return TRUE;
}



/* usage: t_pgrw
 *
 * This test case test page read/write
 */
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
		MSGLN("no free block ?");
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
		MSGLN("Write page error: %d", ret);
		goto ext;
	}

	ret = uffs_FlashReadPage(dev, block, page, buf, U_FALSE);
	if (UFFS_FLASH_HAVE_ERR(ret)) {
		MSGLN("Read page error: %d", ret);
		goto ext;
	}

	for (i = 0; i < dev->com.pg_data_size; i++) {
		if (buf->data[i] != (i & 0xFF)) {
			MSGLN("Data verify fail at: %d", i);
			goto ext;
		}
	}

	ret = uffs_FlashReadPageTag(dev, block, page, tag);
	if (UFFS_FLASH_HAVE_ERR(ret)) {
		MSGLN("Read tag (page spare) error: %d", ret);
		goto ext;
	}
	
	// verify tag:
	if (!TAG_IS_DIRTY(tag)) {
		MSGLN("not dirty ? Tag verify fail!");
		goto ext;
	}

	if (!TAG_IS_VALID(tag)) {
		MSGLN("not valid ? Tag verify fail!");
		goto ext;
	}

	if (TAG_DATA_LEN(tag) != dev->com.pg_data_size ||
		TAG_TYPE(tag) != UFFS_TYPE_DATA ||
		TAG_PAGE_ID(tag) != 3 ||
		TAG_PARENT(tag) != 100 ||
		TAG_SERIAL(tag) != 10 ||
		TAG_BLOCK_TS(tag) != 1) {

		MSGLN("Tag verify fail!");
		goto ext;
	}

	MSGLN("Page read/write test succ.");

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
		MSGLN("can't create test file %s", test_file);
		return U_TRUE;
	}

	MSGLN("Formating %s ... ", mount);

	dev = uffs_GetDeviceFromMountPoint(mount);
	if (dev == NULL) {
		MSGLN("Can't get device from mount point.");
	}
	else {
		ret = uffs_FormatDevice(dev, force);
		if (ret != U_SUCC) {
			MSGLN("Format fail.");
		}
		else {
			MSGLN("Format succ.");
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
			MSGLN("Create file %s failed", name);
			break;
		}
		if (uffs_write(fd, name, strlen(name)) != strlen(name)) { // write full path name to file
			MSGLN("Write to file %s failed", name);
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
				MSGLN("Delete file %s failed", name);
		}
		succ = U_FALSE;
		goto ext;
	}

	MSGLN("%d files created.", count);

	// list files
	dirp = uffs_opendir(start);
	if (dirp == NULL) {
		MSGLN("Can't open dir %s !", start);
		succ = U_FALSE;
		goto ext;
	}
	ent = uffs_readdir(dirp);
	while (ent && succ) {

		if (!(ent->d_type & FILE_ATTR_DIR) &&					// not a dir
			ent->d_namelen == strlen("File000") &&				// check file name length
			memcmp(ent->d_name, "File", strlen("File")) == 0) {	// file name start with "File"
			
			MSGLN("List entry %s", ent->d_name);

			num = strtol(ent->d_name + 4, NULL, 10);
			if (GBIT(num)) {
				// file already listed ?
				MSGLN("File %d listed twice !", ent->d_name);
				succ = U_FALSE;
				break;
			}
			SBIT(num);

			// check file content
			sprintf(name, "%s%s", start, ent->d_name);
			fd = uffs_open(name, UO_RDONLY);
			if (fd < 0) {
				MSGLN("Open file %d for read failed !", name);
			}
			else {
				memset(buf, 0, sizeof(buf));
				num = uffs_read(fd, buf, sizeof(buf));
				if (num != strlen(name)) {
					MSGLN("%s Read data length expect %d but got %d !", name, strlen(name), num);
					succ = U_FALSE;
				}
				else {
					if (memcmp(name, buf, num) != 0) {
						MSGLN("File %s have wrong content '%s' !", name, buf);
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
			MSGLN("File %s not listed !", name);
			succ = U_FALSE;
		}
	}
	
	// delete files if pass the test
	for (i = 0; succ && i < count; i++) {
		sprintf(name, "%sFile%03d", start, i);
		if (uffs_remove(name) < 0) {
			MSGLN("Delete file %s failed", name);
			succ = U_FALSE;
		}
	}

ext:
	MSGLN("Populate files test %s !", succ ? "SUCC" : "FAILED");
	return U_TRUE;

}


static struct cli_commandset cmdset[] = 
{
    { cmdTest1,		"t1",			"<name>",			"test 1" },
    { cmdTest2,		"t2",			NULL,				"test 2" },
    { cmdTest3,		"t3",			"<name>",			"test 3" },
    { cmdTest4,		"t4",			NULL,				"test 4" },
    { cmdTest5,		"t5",			"<name>",			"test 5" },
    { cmdTestPageReadWrite,		"t_pgrw",		NULL,		"test page read/write" },
    { cmdTestFormat,			"t_format",		NULL,		"test format file system" },
	{ cmdTestPopulateFiles,		"t_pfs",		"[<start> [<n>]]",	"test populate <n> files under <start>" },
    { NULL, NULL, NULL, NULL }
};


struct cli_commandset * get_test_cmds()
{
	return cmdset;
};



