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
 * \file api_test_server_posix.c
 * \brief uffs API test server.
 * \author Ricky Zheng
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_fs.h"
#include "uffs/uffs_utils.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs_mtb.h"
#include "uffs/uffs_crc.h"
#include "uffs/uffs_fd.h"
#include "uffs/uffs_version.h"
#include "api_test.h"

#define PFX NULL

#define BACKLOGS	10

static int _io_read(int fd, void *buf, int len)
{
    return recv(fd, buf, len, MSG_WAITALL);
}

static int _io_write(int fd, const void *buf, int len)
{
    return send(fd, buf, len, 0);
}

static int _io_close(int fd)
{
    return close(fd);
}

static struct uffs_ApiSrvIoSt m_io = {
    .read = _io_read,
    .write = _io_write,
    .close = _io_close,
};

static struct uffs_ApiSt m_api = {
    uffs_version,
    uffs_open,
    uffs_close,
    uffs_read,
    uffs_write,
    uffs_flush,
    uffs_seek,
    uffs_tell,
    uffs_eof,
    uffs_rename,
    uffs_remove,
    uffs_ftruncate,
    uffs_mkdir,
    uffs_rmdir,
    uffs_stat,
    uffs_lstat,
    uffs_fstat,
    uffs_opendir,
    uffs_closedir,
    uffs_readdir,
    uffs_rewinddir,
    uffs_get_error,
    uffs_set_error,
    uffs_format,
    uffs_space_total,
    uffs_space_used,
    uffs_space_free,
};


int api_server_start(void)
{
	int srv_fd = -1, new_fd = -1;
	struct sockaddr_in my_addr;
	struct sockaddr_in peer_addr;
	socklen_t sin_size;
	int yes = 1;
	int ret = 0;

	srv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd < 0) {
		perror("create srv socket error");
		ret = -1;
		goto ext;
	}

	memset(&my_addr, 0, sizeof(struct sockaddr_in));
	my_addr.sin_family = AF_INET; 			/* host byte order */
	my_addr.sin_port = htons(SRV_PORT); 	/* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY;	/* 0.0.0.0 */

	ret = bind(srv_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
	if (ret < 0) {
	  perror("Server-bind() error");
	  goto ext;
	}

	/* "Address already in use" error message */
	ret = setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (ret < 0) {
	  perror("setsockopt() error");
	  goto ext;
	}

	ret = listen(srv_fd, BACKLOGS);
	if (ret < 0) {
		perror("listen() error");
		goto ext;
	}

	sin_size = sizeof(struct sockaddr_in);

	apisrv_setup_io(&m_io);

	do {
		new_fd = accept(srv_fd, (struct sockaddr *)&peer_addr, &sin_size);
		if (new_fd >= 0) {
			ret = apisrv_serve(new_fd, &m_api);
			if (ret >= 0)
				close(new_fd);
		}
	} while (ret >= 0);

ext:
	if (srv_fd >= 0)
		close(srv_fd);

	return ret;
}


