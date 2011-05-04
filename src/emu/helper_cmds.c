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
 * \file helper_cmds.c
 * \brief helper commands for test uffs
 * \author Ricky Zheng
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "uffs/uffs_config.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_fs.h"
#include "uffs/uffs_utils.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs_mtb.h"
#include "uffs/uffs_find.h"
#include "cmdline.h"
#include "uffs/uffs_fd.h"
#include "uffs/uffs_mtb.h"
#include "uffs_fileem.h"

#define PFX "cmd: "

#define MAX_PATH_LENGTH 128

#define MSGLN(msg,...) uffs_Perror(UFFS_ERR_NORMAL, msg, ## __VA_ARGS__)
#define MSG(msg,...) uffs_PerrorRaw(UFFS_ERR_NORMAL, msg, ## __VA_ARGS__)

static BOOL cmdFormat(const char *tail)
{
	URET ret;
	const char *mount = "/";
	uffs_Device *dev;
	const char *next;
	UBOOL force = U_FALSE;

	if (tail) {
		mount = cli_getparam(tail, &next);
		if (next && strcmp(next, "-f") == 0)
			force = U_TRUE;
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
	return TRUE;	
}

static BOOL cmdMkf(const char *tail)
{
	int fd;
	const char *name;
	int oflags = UO_RDWR | UO_CREATE;

	if (tail == NULL) {
		return FALSE;
	}

	name = cli_getparam(tail, NULL);
	fd = uffs_open(name, oflags);
	if (fd < 0) {
		MSGLN("Create %s fail, err: %d", name, uffs_get_error());
	}
	else {
		MSGLN("Create %s succ.", name);
		uffs_close(fd);
	}
	
	return TRUE;
}

static BOOL cmdMkdir(const char *tail)
{
	const char *name;

	if (tail == NULL) {
		return FALSE;
	}

	name = cli_getparam(tail, NULL);
	
	if (uffs_mkdir(name) < 0) {
		MSGLN("Create %s fail, err: %d", name, uffs_get_error());
	}
	else {
		MSGLN("Create %s succ.", name);
	}	
	return TRUE;
}


static int CountObjectUnder(const char *dir)
{
	int count = 0;
	uffs_DIR *dirp;

	dirp = uffs_opendir(dir);
	if (dirp) {
		while (uffs_readdir(dirp) != NULL)
			count++;
		uffs_closedir(dirp);
	}
	return count;
}

static BOOL cmdPwd(const char *tail)
{
	MSGLN("not supported.");
	return TRUE;
}

static BOOL cmdCd(const char *tail)
{
	MSGLN("Not supported");
	return TRUE;
}

static BOOL cmdLs(const char *tail)
{
	uffs_DIR *dirp;
	struct uffs_dirent *ent;
	struct uffs_stat stat_buf;
	int count = 0;
	char buf[MAX_PATH_LENGTH+2];
	char *name = (char *)tail;
	char *sub;

	if (name == NULL) {
		MSGLN("Must provide file/dir name.");
		return FALSE;
	}

	dirp = uffs_opendir(name);
	if (dirp == NULL) {
		MSGLN("Can't open '%s' for list", name);
	}
	else {
		MSG("------name-----------size---------serial-----" TENDSTR);
		ent = uffs_readdir(dirp);
		while (ent) {
			MSG("%9s", ent->d_name);
			strcpy(buf, name);
			sub = buf;
			if (name[strlen(name)-1] != '/')
				sub = strcat(buf, "/");
			sub = strcat(sub, ent->d_name);
			if (ent->d_type & FILE_ATTR_DIR) {
				sub = strcat(sub, "/");
				MSG("/  \t<%8d>", CountObjectUnder(sub));
			}
			else {
				uffs_stat(sub, &stat_buf);
				MSG("   \t %8d ", stat_buf.st_size);
			}
			MSG("\t%6d" TENDSTR, ent->d_ino);
			count++;
			ent = uffs_readdir(dirp);
		}
		
		uffs_closedir(dirp);

		MSG("Total: %d objects." TENDSTR, count);
	}

	return TRUE;
}

static BOOL cmdRm(const char *tail)
{
	const char *name = NULL;
	int ret = 0;
	struct uffs_stat st;

	if (tail == NULL) return FALSE;

	name = cli_getparam(tail, NULL);

	if (uffs_stat(name, &st) < 0) {
		MSGLN("Can't stat '%s'", name);
		return TRUE;
	}

	if (st.st_mode & US_IFDIR) {
		ret = uffs_rmdir(name);
	}
	else {
		ret = uffs_remove(name);
	}

	if (ret == 0)
		MSGLN("Delete '%s' succ.", name);
	else
		MSGLN("Delete '%s' fail!", name);

	return TRUE;
}

static BOOL cmdRen(const char *tail)
{
	const char *oldname;
	const char *newname;

	if (tail == NULL) 
		return FALSE;
	oldname = cli_getparam(tail, &newname);
	if (newname == NULL)
		return FALSE;

	if (uffs_rename(oldname, newname) == 0) {
		MSGLN("Rename from '%s' to '%s' succ.", oldname, newname);
	}
	else {
		MSGLN("Rename from '%s' to '%s' fail!", oldname, newname);
	}
	return TRUE;
}

static void dump_msg_to_stdout(struct uffs_DeviceSt *dev, const char *fmt, ...)
{
	uffs_FileEmu *emu = (uffs_FileEmu *)(dev->attr->_private);
	va_list args;

	va_start(args, fmt);
	//vprintf(fmt, args);
	if (emu && emu->dump_fp)
		vfprintf(emu->dump_fp, fmt, args);
	va_end(args);
}

static BOOL cmdDump(const char *tail)
{
	uffs_Device *dev;
	uffs_FileEmu *emu;
	const char *mount = "/";

	if (tail) {
		mount = cli_getparam(tail, NULL);
	}

	dev = uffs_GetDeviceFromMountPoint(mount);
	if (dev == NULL) {
		MSGLN("Can't get device from mount point %s", mount);
		return TRUE;
	}

	emu = (uffs_FileEmu *)(dev->attr->_private);
	emu->dump_fp = fopen("dump.txt", "w");

	uffs_DumpDevice(dev, dump_msg_to_stdout);

	if (emu->dump_fp)
		fclose(emu->dump_fp);

	return TRUE;
}

static BOOL cmdSt(const char *tail)
{
	uffs_Device *dev;
	const char *mount = "/";
	uffs_FlashStat *s;
	TreeNode *node;

	if (tail) {
		mount = cli_getparam(tail, NULL);
	}

	dev = uffs_GetDeviceFromMountPoint(mount);
	if (dev == NULL) {
		MSGLN("Can't get device from mount point %s", mount);
		return TRUE;
	}

	s = &(dev->st);

	MSG("----------- basic info -----------" TENDSTR);
	MSG("TreeNode size:         %d" TENDSTR, sizeof(TreeNode));
	MSG("TagStore size:         %d" TENDSTR, sizeof(struct uffs_TagStoreSt));
	MSG("MaxCachedBlockInfo:    %d" TENDSTR, MAX_CACHED_BLOCK_INFO);
	MSG("MaxPageBuffers:        %d" TENDSTR, MAX_PAGE_BUFFERS);
	MSG("MaxDirtyPagesPerBlock: %d" TENDSTR, MAX_DIRTY_PAGES_IN_A_BLOCK);
	MSG("MaxPathLength:         %d" TENDSTR, MAX_PATH_LENGTH);
	MSG("MaxObjectHandles:      %d" TENDSTR, MAX_OBJECT_HANDLE);
	MSG("FreeObjectHandles:     %d" TENDSTR, uffs_PoolGetFreeCount(uffs_GetObjectPool()));
	MSG("MaxDirHandles:         %d" TENDSTR, MAX_DIR_HANDLE);
	MSG("FreeDirHandles:        %d" TENDSTR, uffs_PoolGetFreeCount(uffs_DirEntryBufGetPool()));

	MSG("----------- statistics for '%s' -----------" TENDSTR, mount);
	MSG("Device Ref:            %d" TENDSTR, dev->ref_count);
	MSG("Block Erased:          %d" TENDSTR, s->block_erase_count);
	MSG("Write Page:            %d" TENDSTR, s->page_write_count);
	MSG("Write Spare:           %d" TENDSTR, s->spare_write_count);
	MSG("Read Page:             %d" TENDSTR, s->page_read_count - s->page_header_read_count);
	MSG("Read Header:           %d" TENDSTR, s->page_header_read_count);
	MSG("Read Spare:            %d" TENDSTR, s->spare_read_count);
	MSG("I/O Read:              %lu" TENDSTR, s->io_read);
	MSG("I/O Write:             %lu" TENDSTR, s->io_write);

	MSG("--------- partition info for '%s' ---------" TENDSTR, mount);
	MSG("Space total:           %d" TENDSTR, uffs_GetDeviceTotal(dev));
	MSG("Space used:            %d" TENDSTR, uffs_GetDeviceUsed(dev));
	MSG("Space free:            %d" TENDSTR, uffs_GetDeviceFree(dev));
	MSG("Page Size:             %d" TENDSTR, dev->attr->page_data_size);
	MSG("Spare Size:            %d" TENDSTR, dev->attr->spare_size);
	MSG("Pages Per Block:       %d" TENDSTR, dev->attr->pages_per_block);
	MSG("Block size:            %d" TENDSTR, dev->attr->page_data_size * dev->attr->pages_per_block);
	MSG("Total blocks:          %d of %d" TENDSTR, (dev->par.end - dev->par.start + 1), dev->attr->total_blocks);
	if (dev->tree.bad) {
		MSG("Bad blocks: ");
		node = dev->tree.bad;
		while(node) {
			MSG("%d, ", node->u.list.block);
			node = node->u.list.next;
		}
		MSG(TENDSTR);
	}

	uffs_PutDevice(dev);

	return TRUE;

}


static BOOL cmdCp(const char *tail)
{
	const char *src;
	const char *des;
	char buf[100];
	int fd1 = -1, fd2 = -1;
	int len;
	BOOL src_local = FALSE, des_local = FALSE;
	FILE *fp1 = NULL, *fp2 = NULL;

	if (!tail) 
		return FALSE;

	src = cli_getparam(tail, &des);

	if (!des)
		return FALSE;

	if (memcmp(src, "::", 2) == 0) {
		src += 2;
		src_local = TRUE;
	}
	if (memcmp(des, "::", 2) == 0) {
		des += 2;
		des_local = TRUE;
	}
	
	if (src_local) {
		if ((fp1 = fopen(src, "rb")) == NULL) {
			MSGLN("Can't open %s for copy.", src);
			goto fail_ext;
		}
	}
	else {
		if ((fd1 = uffs_open(src, UO_RDONLY)) < 0) {
			MSGLN("Can't open %s for copy.", src);
			goto fail_ext;
		}
	}

	if (des_local) {
		if ((fp2 = fopen(des, "wb")) == NULL) {
			MSGLN("Can't open %s for copy.", des);
			goto fail_ext;
		}
	}
	else {
		if ((fd2 = uffs_open(des, UO_RDWR|UO_CREATE|UO_TRUNC)) < 0) {
			MSGLN("Can't open %s for copy.", des);
			goto fail_ext;
		}
	}

	while (	(src_local ? (feof(fp1) == 0) : (uffs_eof(fd1) == 0)) ) {
		if (src_local) {
			len = fread(buf, 1, sizeof(buf), fp1);
		}
		else {
			len = uffs_read(fd1, buf, sizeof(buf));
		}
		if (len == 0) 
			break;
		if (len < 0) {
			MSGLN("read file %s fail ?", src);
			break;
		}
		if (des_local) {
			if ((int)fwrite(buf, 1, len, fp2) != len) {
				MSGLN("write file %s fail ? ", des);
				break;
			}
		}
		else {
			if (uffs_write(fd2, buf, len) != len) {
				MSGLN("write file %s fail ? ", des);
				break;
			}
		}
	}

fail_ext:
	if (fd1 > 0)
		uffs_close(fd1);
	if (fd2 > 0)
		uffs_close(fd2);
	if (fp1) 
		fclose(fp1);
	if (fp2)
		fclose(fp2);

	return TRUE;
}

static BOOL cmdCat(const char *tail)
{
	int fd;
	const char *name;
	const char *next;
	char buf[100];
	int start = 0, size = 0, printed = 0, n, len;

	if (!tail) 
		return FALSE;

	name = cli_getparam(tail, &next);

	if ((fd = uffs_open(name, UO_RDONLY)) < 0) {
		MSGLN("Can't open %s", name);
		goto fail;
	}

	if (next) {
		start = strtol(next, (char **) &next, 10);
		if (next) size = strtol(next, NULL, 10);
	}

	if (start >= 0)
		uffs_seek(fd, start, USEEK_SET);
	else
		uffs_seek(fd, -start, USEEK_END);

	while (uffs_eof(fd) == 0) {
		len = uffs_read(fd, buf, sizeof(buf) - 1);
		if (len == 0) 
			break;
		if (len > 0) {
			if (size == 0 || printed < size) {
				n = (size == 0 ? len : (size - printed > len ? len : size - printed));
				buf[n] = 0;
				MSG("%s", buf);
				printed += n;
			}
			else {
				break;
			}
		}
	}
	MSG(TENDSTR);
	uffs_close(fd);

fail:

	return TRUE;
}


static BOOL cmdMount(const char *tail)
{
	uffs_MountTable *tab = uffs_GetMountTable();
	tail = tail;

	while (tab) {
		MSGLN(" %s : (%d) ~ (%d)", tab->mount, tab->start_block, tab->end_block);
		tab = tab->next;
	}

	return TRUE;
}

static BOOL cmdInspBuf(const char *tail)
{
	uffs_Device *dev;
	const char *mount = "/";

	if (tail) {
		mount = cli_getparam(tail, NULL);
	}

	dev = uffs_GetDeviceFromMountPoint(mount);
	if (dev == NULL) {
		MSGLN("Can't get device from mount point %s", mount);
		return TRUE;
	}

	uffs_BufInspect(dev);

	uffs_PutDevice(dev);

	return TRUE;

}

static BOOL cmdWareLevelInfo(const char *tail)
{
	const char *mount = "/";
	uffs_Device *dev;
	struct uffs_PartitionSt *par;
	uffs_FileEmu *emu;
	int i, max;
	u32 n;

#define NUM_PER_LINE	10

	if (tail) {
		mount = cli_getparam(tail, NULL);
	}

	dev = uffs_GetDeviceFromMountPoint(mount);
	if (dev == NULL) {
		MSGLN("Can't get device from mount point %s", mount);
		return TRUE;
	}

	par = &dev->par;
	emu = (uffs_FileEmu *)(dev->attr->_private);
	max = -1;

	for (i = 0; i < par->end - par->start; i++) {
		if ((i % NUM_PER_LINE) == 0) {
			MSG("%04d:", i + par->start);
		}
		n = i + par->start;
		max = (max == -1 ? n :
				(emu->em_monitor_block[n] > emu->em_monitor_block[max] ? n : max)
			   );
		MSG(" %4d", emu->em_monitor_block[n]);
		MSG("%c", uffs_TreeFindErasedNodeByBlock(dev, n) == NULL ? '*' : ' ');
		if (((i + 1) % NUM_PER_LINE) == 0)
			MSG("\n");
	}
	MSG("\n");
	MSG("Total blocks %d, peak erase count %d at block %d\n",
		par->end - par->start, max == -1 ? 0 : emu->em_monitor_block[max], max);

	uffs_PutDevice(dev);

	return TRUE;
}

static struct cli_commandset cmdset[] = 
{
    { cmdFormat,	"format",		"[<mount>]",		"Format device" },
    { cmdMkf,		"mkfile",		"<name>",			"create a new file" },
    { cmdMkdir,		"mkdir",		"<name>",			"create a new directory" },
    { cmdRm,		"rm",			"<name>",			"delete file/directory" },
    { cmdRen,		"mv|ren",		"<old> <new>",		"rename file/directory" },
    { cmdLs,		"ls",			"<dir>",			"list dirs and files" },
    { cmdSt,		"info|st",		"<mount>",			"show statistic infomation" },
    { cmdCp,		"cp",			"<src> <des>",		"copy files. the local file name start with '::'" },
    { cmdCat,		"cat",			"<name>",			"show file content" },
    { cmdPwd,		"pwd",			NULL,				"show current dir" },
    { cmdCd,		"cd",			"<path>",			"change current dir" },
    { cmdMount,		"mount",		NULL,				"list mounted file systems" },
	{ cmdDump,		"dump",			"[<mount>]",		"dump file system", },
	{ cmdWareLevelInfo,	"wl",		"[<mount>]",		"show block wear-leveling info", },
	{ cmdInspBuf,	"inspb",		"[<mount>]",		"inspect buffer", },
    { NULL, NULL, NULL, NULL }
};


struct cli_commandset * get_helper_cmds()
{
	return cmdset;
};
