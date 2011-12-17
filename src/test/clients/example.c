/*
 * This is a APT test client example
 */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>

#include "uffs/uffs_fd.h"
#include "uffs/uffs_crc.h"
#include "api_srv.h"


static int _io_read(int fd, void *buf, size_t len)
{
    return recv(fd, buf, len, MSG_WAITALL);
}

static int _io_write(int fd, const void *buf, size_t len)
{
    return send(fd, buf, len, 0);
}

static int _io_open(void *addr)
{
	int sock;  
	struct hostent *host;
	struct sockaddr_in server_addr;  

	host = gethostbyname((const char *)addr);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		return -1;
	}

	server_addr.sin_family = AF_INET;     
	server_addr.sin_port = htons(SRV_PORT);   
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_addr.sin_zero),8); 

	if (connect(sock, (struct sockaddr *)&server_addr,
				sizeof(struct sockaddr)) == -1) 
	{
		perror("Connect");
		return -1;
	}

	return sock;
}

static int _io_close(int fd)
{
	return close(fd);
}

static struct uffs_ApiSrvIoSt m_io = {
	.open = _io_open,
	.read = _io_read,
	.write = _io_write,
	.close = _io_close,
	.addr = (void *)"127.0.0.1",
};


int main(int argc, char *argv[])
{
	int version;
	struct uffs_ApiSt *api = apisrv_get_client();

	apisrv_setup_io(&m_io);

	version = api->uffs_version();
	printf("Version: %08X\n", version);

	return 0;
}

