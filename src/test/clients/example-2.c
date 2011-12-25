/*
 * This is a APT test client example
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uffs/uffs_fd.h"
#include "api_test.h"


int main(int argc, char *argv[])
{
	int version;
	int fd;
	char buf[4096];
	long offset = 76800;
	int len = 1024;

	api_client_init(NULL);

	version = uffs_version();
	printf("Version: %08X\n", version);

	fd = uffs_open("/test.db", UO_RDWR);
	if (fd < 0) {
		printf("Can't create /test.db\n");
		return -1;
	}

	if (uffs_seek(fd, offset, USEEK_SET) != offset) {
		printf("call uffs_seek failed\n");
	}
	else if (uffs_read(fd, buf, len) != len) {
		printf("call uffs_write failed.\n");
	}

	if (uffs_close(fd) < 0) {
		printf("uffs_close failed.\n");
	}

	return 0;
}

