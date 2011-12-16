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
 * \file api_srv.c
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


#include "uffs_config.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include "uffs/uffs_fs.h"
#include "uffs/uffs_utils.h"
#include "uffs/uffs_core.h"
#include "uffs/uffs_mtb.h"
#include "uffs/uffs_crc.h"
#include "uffs/uffs_fd.h"
#include "uffs/uffs_version.h"
#include "api_srv.h"

#define PFX NULL
#define MSG(msg,...) uffs_PerrorRaw(UFFS_MSG_NORMAL, msg, ## __VA_ARGS__)
#define MSGLN(msg,...) uffs_Perror(UFFS_MSG_NORMAL, msg, ## __VA_ARGS__)

#define BACKLOGS	10

struct uffs_ApiSrvMsgSt {
	struct uffs_ApiSrvHeaderSt header;
	u8 *data;
};

// unload parameters from message.
static int unload_params(struct uffs_ApiSrvMsgSt *msg, ...)
{
	int ret = 0;
	int n;
	u8 *p;
	u8 *data;
	struct uffs_ApiSrvHeaderSt *header = &msg->header;

	va_list args;

	va_start(args, msg);

	for (n = 0, data = msg->data; n < header->n_params; n++) {
		p = va_arg(args, void *);
		if (p == NULL)
			break;
		memcpy(p, data, header->param_size[n]);
		data += header->param_size[n];
	}

	if (n != header->n_params) {
		MSGLN("Extract parameter for cmd %d failed ! parameter number mismatched.");
		ret = -1;
	}

	if (data - msg->data != header->data_len) {
		MSGLN("Extract parameter for cmd %d failed ! data len mismatched.");
		ret = -1;
	}

	va_end(args);

	return ret;
}

// load response data. the parameter list: (&param1, size1, &param2, size2, .... NULL)
static int make_response(struct uffs_ApiSrvMsgSt *msg, ...)
{
	int ret = 0;
	int n;
	size_t len;
	struct uffs_ApiSrvHeaderSt *header = &msg->header;
	u8 *p;
	u8 * params[UFFS_API_MAX_PARAMS];

	va_list args;

	header->cmd |= UFFS_API_ACK_BIT;

	va_start(args, msg);

	for (n = 0, len = 0; n < UFFS_API_MAX_PARAMS; n++) {
		p = va_arg(args, u8 *);
		if (p == NULL)	// terminator
			break;
		params[n] = p;
		header->param_size[n] = va_arg(args, size_t);
		len += header->param_size[n];
	}
	header->n_params = n;

	va_end(args);

	if (len > header->data_len) {
		free(msg->data);
		header->data_len = 0;
		msg->data = (u8 *)malloc(len);
		if (msg->data == NULL) {
			MSGLN("Fail to malloc %d bytes", len);
			ret = -1;
			goto ext;
		}
		header->data_len = len;
	}

	for (n = 0, p = msg->data; n < header->n_params; n++) {
		memcpy(p, params[n], header->param_size[n]);
		p += header->param_size[n];
	}

ext:

	return ret;
}

// calculate crc and send msg.
static int reply(int fd, struct uffs_ApiSrvMsgSt *msg)
{
	int ret;

	msg->header.data_crc = uffs_crc16sum(msg->data, msg->header.data_len);
	msg->header.header_crc = uffs_crc16sum(&msg->header, sizeof(struct uffs_ApiSrvHeaderSt));

	ret = send(fd, &msg->header, sizeof(struct uffs_ApiSrvHeaderSt), 0);
	if (ret < 0) {
		perror("Sending header failed");
	}
	else {
		ret = send(fd, msg->data, msg->header.data_len, 0);
		if (ret < 0) {
			perror("Sending data failed");
		}
	}

	return ret;
}


static int check_apisrv_header(struct uffs_ApiSrvHeaderSt *header)
{
	return 0;
}

static int check_apisrv_msg(struct uffs_ApiSrvMsgSt *msg)
{
	return 0;
}

static int process_cmd(int fd, struct uffs_ApiSrvMsgSt *msg)
{
	struct uffs_ApiSrvHeaderSt *header = &msg->header;
	int ret = 0;
	char name[256];

	switch(UFFS_API_CMD(header)) {
	case UFFS_API_GET_VER_CMD:
	{
		int val;
		val = uffs_GetVersion();
		ret = make_response(msg, &val, sizeof(val), NULL);
		if (ret == 0)
			reply(fd, msg);
		break;
	}
	case UFFS_API_OPEN_CMD:
	{
		int open_mode;
		unload_params(msg, name, &open_mode);
		break;
	}
	case UFFS_API_CLOSE_CMD:
		break;
	case UFFS_API_READ_CMD:
		break;
	case UFFS_API_WRITE_CMD:
		break;
	case UFFS_API_FLUSH_CMD:
		break;
	case UFFS_API_SEEK_CMD:
		break;
	case UFFS_API_TELL_CMD:
		break;
	case UFFS_API_EOF_CMD:
		break;
	case UFFS_API_RENAME_CMD:
		break;
	case UFFS_API_REMOVE_CMD:
		break;
	case UFFS_API_TRUNCATE_CMD:
		break;
	case UFFS_API_MKDIR_CMD:
		break;
	case UFFS_API_RMDIR_CMD:
		break;
	case UFFS_API_STAT_CMD:
		break;
	case UFFS_API_LSTAT_CMD:
		break;
	case UFFS_API_FSTAT_CMD:
		break;
	case UFFS_API_OPEN_DIR_CMD:
		break;
	case UFFS_API_CLOSE_DIR_CMD:
		break;
	case UFFS_API_READ_DIR_CMD:
		break;
	case UFFS_API_REWIND_DIR_CMD:
		break;
	case UFFS_API_GET_ERR_CMD:
		break;
	case UFFS_API_SET_ERR_CMD:
		break;
	case UFFS_API_FORMAT_CMD:
		break;
	case UFFS_API_GET_TOTAL_CMD:
		break;
	case UFFS_API_GET_FREE_CMD:
		break;
	case UFFS_API_GET_USED_CMD:
		break;
	default:
		MSGLN("Unknown command %x", header->cmd);
		ret = -1;
		break;
	}

	return ret;
}

static int apisrv_serve(int fd)
{
	int ret = 0;
	struct uffs_ApiSrvMsgSt msg;
	struct uffs_ApiSrvHeaderSt *header = &msg.header;
	u8 *data = NULL;

	memset(&msg, 0, sizeof(struct uffs_ApiSrvMsgSt));

	ret = recv(fd, header, sizeof(struct uffs_ApiSrvHeaderSt), MSG_WAITALL);
	if (ret < 0)
		goto ext;

	ret = check_apisrv_header(header);
	if (ret < 0)
		goto ext;

	if (header->data_len > 0) {
		data = (u8 *)malloc(header->data_len);
		if (data == NULL) {
			MSGLN("Malloc %d failed", header->data_len);
			ret = -1;
			goto ext;
		}
	}

	msg.data = data;

	ret = check_apisrv_msg(&msg);
	if (ret == 0)
		ret = process_cmd(fd, &msg);
	else
		MSGLN("Data CRC check failed!");

ext:
	if (data)
		free(data);

	return ret;
}

int apisrv_start(void)
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

	do {
		new_fd = accept(srv_fd, (struct sockaddr *)&peer_addr, &sin_size);
		if (new_fd >= 0) {
			ret = apisrv_serve(new_fd);
			if (ret >= 0)
				close(new_fd);
		}
	} while (ret > 0);

ext:
	if (srv_fd >= 0)
		close(srv_fd);

	return ret;
}


