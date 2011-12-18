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
* \file api_test.c
* \brief API test server public functions 
* \author Ricky Zheng, created in 16 Dec, 2011 
*/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "uffs/uffs_crc.h"
#include "api_test.h"


static struct uffs_ApiSrvIoSt *m_io = NULL;

int apisrv_setup_io(struct uffs_ApiSrvIoSt *io)
{
	m_io = io;
	return 0;
}

//
// unload parameters from message.
// parameters list:
//  param1, size1, param2, size2, ..., NULL
// size: maximum size of parameter.
//
static int apisrv_unload_params(struct uffs_ApiSrvMsgSt *msg, ...)
{
    int ret = 0;
    int n, len, size;
    u8 *p;
    u8 *data;
    struct uffs_ApiSrvHeaderSt *header = &msg->header;

    va_list args;

    va_start(args, msg);

    for (n = 0, data = msg->data; n < header->n_params; n++) {
        p = va_arg(args, void *);
        if (p == NULL)
            break;
		len = va_arg(args, int);
		size = header->param_size[n];
		if (size > len) {
			printf("cmd %d return param %d overflow (max %d but %d)\n",
					header->cmd, n, len, size);
			ret = -1;
			break;
		}
        memcpy(p, data, size);
        data += size;
    }

    if (n != header->n_params) {
        printf("Extract parameter for cmd %d failed ! parameter number mismatched.\n", header->cmd);
        ret = -1;
    }

    if (data - msg->data != header->data_len) {
        printf("Extract parameter for cmd %d failed ! data len mismatched.\n", header->cmd);
        ret = -1;
    }

    va_end(args);

    return ret;
}

// load response data. the parameter list: (&param1, size1, &param2, size2, .... NULL)
static int apisrv_make_message(struct uffs_ApiSrvMsgSt *msg, ...)
{
    int ret = 0;
    int n;
    size_t len;
    struct uffs_ApiSrvHeaderSt *header = &msg->header;
    u8 *p;
    u8 * params[UFFS_API_MAX_PARAMS];

    va_list args;

    va_start(args, msg);

    for (n = 0, len = 0; n < UFFS_API_MAX_PARAMS; n++) {
        p = va_arg(args, u8 *);
        if (p == NULL)  // terminator
            break;
        params[n] = p;
        header->param_size[n] = va_arg(args, size_t);
        len += header->param_size[n];
    }
    header->n_params = n;

    va_end(args);

	msg->data = (u8 *)malloc(len);
	if (msg->data == NULL) {
		printf("Fail to malloc %d bytes\n", len);
		ret = -1;
		goto ext;
	}
	header->data_len = len;

    for (n = 0, p = msg->data; n < header->n_params; n++) {
        memcpy(p, params[n], header->param_size[n]);
        p += header->param_size[n];
    }

ext:

    return ret;
}


// calculate crc, send message.
static int apisrv_send_message(int fd, struct uffs_ApiSrvMsgSt *msg)
{
    int ret;

    msg->header.data_crc = uffs_crc16sum(msg->data, msg->header.data_len);
    msg->header.header_crc = uffs_crc16sum(&msg->header, sizeof(struct uffs_ApiSrvHeaderSt));

    ret = m_io->write(fd, &msg->header, sizeof(struct uffs_ApiSrvHeaderSt));
    if (ret < 0) {
        perror("Sending header failed");
    }
    else {
        ret = m_io->write(fd, msg->data, msg->header.data_len);
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

static int apisrv_read_message(int fd, struct uffs_ApiSrvMsgSt *msg)
{
	int ret = -1;
	struct uffs_ApiSrvHeaderSt *header = &msg->header;
	u8 *data = NULL;

	memset(msg, 0, sizeof(struct uffs_ApiSrvMsgSt));
	ret = m_io->read(fd, header, sizeof(struct uffs_ApiSrvHeaderSt));
	if (ret < 0) {
		printf("Read header failed!\n");
		goto ext;
	}

	ret = check_apisrv_header(header);
	if (ret < 0)
		goto ext;

	if (header->data_len > 0) {
		data = (u8 *)malloc(header->data_len);
		if (data == NULL) {
			printf("malloc %d bytes failed\n", header->data_len);
			goto ext;
		}

		msg->data = data;
		ret = m_io->read(fd, data, header->data_len);
		if (ret < 0) {
			printf("read data failed\n");
			goto ext;
		}
	}

	ret = check_apisrv_msg(msg);
	if (ret < 0) {
		printf("check data CRC failed!\n");
		goto ext;
	}

ext:
	return ret;	
}

static int apisrv_free_message(struct uffs_ApiSrvMsgSt *msg)
{
	if (msg && msg->data) {
		free(msg->data);
		msg->data = NULL;
	}

	return 0;
}

static int process_cmd(int fd, struct uffs_ApiSrvMsgSt *msg, struct uffs_ApiSt *api)
{
    struct uffs_ApiSrvHeaderSt *header = &msg->header;
    int ret = 0;
    char name[256];

    printf("Received cmd = %d, data_len = %d\n", UFFS_API_CMD(header), header->data_len);

    switch(UFFS_API_CMD(header)) {
    case UFFS_API_GET_VER_CMD:
    {
        int val;
        val = api->uffs_version();
		apisrv_free_message(msg);
        ret = apisrv_make_message(msg, &val, sizeof(val), NULL);
        if (ret == 0)
            apisrv_send_message(fd, msg);
        break;
    }
    case UFFS_API_OPEN_CMD:
    {
        int open_mode;
		int fd;
        ret = apisrv_unload_params(msg, -1, 0, name, sizeof(name), &open_mode, sizeof(open_mode), NULL);
		apisrv_free_message(msg);
		if (ret == 0) {
			fd = api->uffs_open(name, open_mode);
			ret = apisrv_make_message(msg, &fd, sizeof(fd), -1, 0, -1, 0, NULL);
			if (ret == 0)
				apisrv_send_message(fd, msg);
		}
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
        printf("Unknown command %x\n", header->cmd);
        ret = -1;
        break;
    }

    return ret;
}

int apisrv_serve(int fd, struct uffs_ApiSt *api)
{
    int ret = 0;
    struct uffs_ApiSrvMsgSt msg;

	ret = apisrv_read_message(fd, &msg);

    if (ret == 0)
        ret = process_cmd(fd, &msg, api);

	apisrv_free_message(&msg);

    return ret;
}

/**
 * variable parameters list:
 *  &ret, size_ret, &param1, size_param1, &param2, size_param2, ... NULL
 **/
static int call_remote(int cmd, ...)
{
	struct uffs_ApiSrvMsgSt msg;
	struct uffs_ApiSrvHeaderSt *header = &msg.header;
	int ret = -1, fd = -1;
	int n = 0;
	u8 *p;
	int len, size;
	u8 * params[UFFS_API_MAX_PARAMS];
	int n_params = 0;
	va_list args;

	memset(&msg, 0, sizeof(struct uffs_ApiSrvMsgSt));

	fd = m_io->open(m_io->addr);
	if (fd < 0) 
		goto ext;

	header->cmd = cmd;

	va_start(args, cmd);
	for (n = 0, len = 0; n < UFFS_API_MAX_PARAMS; n++) {
		p = va_arg(args, u8 *);
		if (p == NULL)
			break;
		params[n] = p;
		size = va_arg(args, int);
		header->param_size[n] = size;
		len += size;
	}
	va_end(args);

	n_params = n;
	header->n_params = n;

	msg.data = (u8 *) malloc(len);
	if (msg.data == NULL) {
		printf("Fail to malloc %d bytes\n", len);
		goto ext;
	}
	header->data_len = len;
	ret = apisrv_send_message(fd, &msg);
	if (ret < 0)
		goto ext;

	apisrv_free_message(&msg);

	ret = apisrv_read_message(fd, &msg);
	if (ret < 0)
		goto ext;

	if (header->n_params != n_params) {
		printf("Response %d parameters but expect %d\n", header->n_params, n_params);
		ret = -1;
		goto ext;
	}

	// now, unload return parameters
	for (n = 0, p = msg.data; n < header->n_params; n++) {
		size = header->param_size[n];
		memcpy(params[n], p, size);
		p += size;
	}
	
ext:
	apisrv_free_message(&msg);
	if (fd >= 0)
		m_io->close(fd);

	return ret;

}

static int _uffs_version(void)
{
	int version;
	int ret;

	ret = call_remote(UFFS_API_GET_VER_CMD, &version, sizeof(version), NULL);

	return ret < 0 ? 0 : version;
}

static int _uffs_open(const char *name, int oflag, ...)
{
	int fd;
	int ret;

	ret = call_remote(UFFS_API_OPEN_CMD, &fd, sizeof(fd), name, strlen(name), &oflag, sizeof(oflag), NULL);

	return ret < 0 ? ret : fd;
}


static struct uffs_ApiSt m_client_api = {
	.uffs_version = _uffs_version,
	.uffs_open = _uffs_open,
};

struct uffs_ApiSt * apisrv_get_client(void)
{
	return &m_client_api;
}

