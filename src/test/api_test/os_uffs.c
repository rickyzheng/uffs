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
* \file os_uffs.c
* \brief This file is for interfacing UFFS to sqlite3 UNIX VFS, so that we
*        can use sqlite3's test case to test out UFFS.
* 
* \author Ricky Zheng, created at 22 Dec, 2011 
*/

#define _GNU_SOURCE
#define _XOPEN_SOURCE 500

#include "uffs/uffs_types.h"
#include "uffs/uffs_fd.h"

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "api_test.h"

#define MAX_OPEN_FDS 50
struct fdmap_s {
    int unix_fd;
    int uffs_fd;
    int valid;
};
static struct fdmap_s m_fdmap[MAX_OPEN_FDS];
static struct fdmap_s m_fdmap_ext[MAX_OPEN_FDS];
static pthread_mutex_t m_fdmap_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK() pthread_mutex_lock(&m_fdmap_mutex)
#define UNLOCK() pthread_mutex_unlock(&m_fdmap_mutex)

static int unix2uffs(int unix_fd)
{
    int fd = -1;
    struct fdmap_s *p, *end;

    p = &m_fdmap[0];
    end = &m_fdmap[MAX_OPEN_FDS];

    LOCK();
    while (p < end) {
        if (p->valid && p->unix_fd == unix_fd) {
            fd = p->uffs_fd;
            break;
        }
        p++;
    }
    UNLOCK();

    return fd;
}

static int push_newfd(int unix_fd, int uffs_fd)
{
    int ret = -1;
    struct fdmap_s *p, *end;

    if (unix_fd < 0)
        return -1;

    if (uffs_fd >= 0) {
        p = &m_fdmap[0];
        end = &m_fdmap[MAX_OPEN_FDS];
    }
    else {
        p = &m_fdmap_ext[0];
        end = &m_fdmap_ext[MAX_OPEN_FDS];
    }

    LOCK();
    while (p < end) {
        if (!p->valid) {
            p->unix_fd = unix_fd;
            p->uffs_fd = uffs_fd;
            p->valid = 1;
            ret = 0;
            break;
        }
        p++;
    }
    UNLOCK();

    return ret;
}

static int remove_fd(int unix_fd)
{
    int ret = -1;
    struct fdmap_s *p, *end;


    LOCK();

    p = &m_fdmap[0];
    end = &m_fdmap[MAX_OPEN_FDS];

    while (p < end) {
        if (p->valid && p->unix_fd == unix_fd) {
            p->valid = 0;
            ret = 0;
            break;
        }
        p++;
    }

    if (ret != 0) {
        p = &m_fdmap_ext[0];
        end = &m_fdmap_ext[MAX_OPEN_FDS];

        while (p < end) {
            if (p->valid && p->unix_fd == unix_fd) {
                p->valid = 0;
                ret = 0;
                break;
            }
            p++;
        }
    }

    UNLOCK();

    return ret;
}

int os_open(const char *name, int flags, int mode)
{
    int fd = -1;
    int uffs_fd = -1, uffs_flags = 0;
	char *p;

    fd = open(name, flags, mode);
    if (fd >= 0) {
        uffs_flags = 0;
        if (flags & O_WRONLY) uffs_flags |= UO_WRONLY;
        if (flags & O_RDWR) uffs_flags |= UO_RDWR;
        if (flags & O_CREAT) uffs_flags |= UO_CREATE;
        if (flags & O_TRUNC) uffs_flags |= UO_TRUNC;
        if (flags & O_EXCL) uffs_flags |= UO_EXCL;
		
		p = (char *)name + strlen(name) - 1;
		for (; *p != '/' && p > name; p--);
        uffs_fd = uffs_open(p, uffs_flags);
        assert(!push_newfd(fd, uffs_fd));

		// sqlite3 testing script delete test.db file outside the control of sqlite lib, 
		// so we need to detect that situation.
		if (strcmp(p, "/test.db") == 0 && uffs_fd >= 0) {
			struct stat sbuf;
			if (fstat(fd, &sbuf) == 0 && sbuf.st_size == 0) {
				// "test.db" file just been created, we should also do that on UFFS as well
				uffs_ftruncate(uffs_fd, 0);
			}
		}
    }


    return fd;
}

int os_unlink(const char *name)
{
	int ret = -1, uffs_ret = -1;
	char *p;

	if (name) {
		p = (char *)name + strlen(name) - 1;
		for (; *p != '/' && p > name; p--);
		uffs_ret = uffs_remove(p);
		ret = unlink(name);

		assert(uffs_ret == ret);
	}

	return ret;
}

int os_close(int fd)
{
    int uffs_fd = -1;
    int ret = -1;

    if (fd >= 0) {
        uffs_fd = unix2uffs(fd);
        if (uffs_fd >= 0) {
            uffs_close(uffs_fd);
        }
        assert(!remove_fd(fd));
        ret = close(fd);
    }
    return ret;
}

int os_read(int fd, void *buf, int len)
{
	int uffs_fd = -1, uffs_ret = -1;
	int ret = -1;
	void *uffs_buf = NULL;

	if (fd >= 0) {
		uffs_fd = unix2uffs(fd);
		if (uffs_fd >= 0) {
			uffs_buf = malloc(len);
			assert(uffs_buf);
			uffs_ret = uffs_read(uffs_fd, uffs_buf, len);
		}
		ret = read(fd, buf, len);
		if (uffs_fd >= 0) {
			assert(ret == uffs_ret);
			//assert(memcmp(buf, uffs_buf, len) == 0);	
			if (ret > 0) {
				if (memcmp(buf, uffs_buf, ret) != 0) {
					printf(" E: read result different! from fd = %d/%d, len = %d, ret = %d\n", fd, uffs_fd, len, ret);
					assert(0);
				}
			}
		}
	}

	if (uffs_buf)
		free(uffs_buf);

	return ret;
}

int os_write(int fd, const void *buf, int len)
{
	int uffs_fd = -1, uffs_ret = -1;
	int ret = -1;

	if (fd >= 0) {
		uffs_fd = unix2uffs(fd);
		if (uffs_fd >= 0) {
			uffs_ret = uffs_write(uffs_fd, buf, len);
		}
		ret = write(fd, buf, len);
		if (uffs_fd >= 0) {
			assert(ret == uffs_ret);
		}
	}

	return ret;
}

int os_lseek(int fd, long offset, int origin)
{
	long ret = -1;
	int uffs_fd = -1;
	long uffs_ret = -1;
	int uffs_origin = 0;

	if (fd >= 0) {
		uffs_fd = unix2uffs(fd);
		if (uffs_fd >= 0) {
			if (origin == SEEK_CUR)
				uffs_origin = USEEK_CUR;
			else if (origin == SEEK_SET)
				uffs_origin = USEEK_SET;
			else if (origin == SEEK_END)
				uffs_origin = USEEK_END;

			uffs_ret = uffs_seek(uffs_fd, offset, uffs_origin);
		}

		ret = lseek(fd, offset, origin);

		if (uffs_fd >= 0) {
			assert(ret == uffs_ret);
		}
	}

	return ret;
}

int os_pread(int fd, void *buf, int count, long offset)
{
	printf("--- pread(fd = %d, buf = {...}, count = %d, offset = %ld) ---\n", fd, count, offset);
	return pread(fd, buf, count, offset);
}

int os_pwrite(int fd, const void *buf, int count, long offset)
{
	printf("--- pwrite(fd = %d, buf = {..}, count = %d, offset = %ld) ---\n", fd, count, offset);
	return pwrite(fd, buf, count, offset);
}

int os_ftruncate(int fd, long length)
{
	printf("--- ftruncate(fd = %d, length = %ld)\n", fd, length);
	return ftruncate(fd, length);
}

int os_posix_fallocate(int fd, long offset, long len)
{
	printf("--- posix_fallocate(fd = %d, offset = %ld, len = %ld) ---\n", fd, offset, len);
	return posix_fallocate(fd, offset, len);
}

int os_uffs_init(void)
{
	printf("---- os_uffs_init() called ------\n");
	return api_client_init("127.0.0.1"); // the host is on localhost
}
