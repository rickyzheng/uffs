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
 * \file uffs_ecc.c
 * \brief ecc maker and correct
 * \author Ricky Zheng, created in 12th Jun, 2005
 */

#include "uffs/uffs_fs.h"
#include "uffs/uffs_config.h"
#include <string.h>

#define PFX "ecc: "

static const u8 bits_tbl[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

static const u8 line_parity_tbl[16] =       {
	0x00, 0x02, 0x08, 0x0a, 0x20, 0x22, 0x28, 0x2a, 0x80, 0x82, 0x88, 0x8a, 0xa0, 0xa2, 0xa8, 0xaa
 };
static const u8 line_parity_prime_tbl[16] = {
	0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15, 0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55
 };

static const u8 column_parity_tbl[256] = {
	0x00, 0x55, 0x59, 0x0c, 0x65, 0x30, 0x3c, 0x69, 0x69, 0x3c, 0x30, 0x65, 0x0c, 0x59, 0x55, 0x00, 
	0x95, 0xc0, 0xcc, 0x99, 0xf0, 0xa5, 0xa9, 0xfc, 0xfc, 0xa9, 0xa5, 0xf0, 0x99, 0xcc, 0xc0, 0x95, 
	0x99, 0xcc, 0xc0, 0x95, 0xfc, 0xa9, 0xa5, 0xf0, 0xf0, 0xa5, 0xa9, 0xfc, 0x95, 0xc0, 0xcc, 0x99, 
	0x0c, 0x59, 0x55, 0x00, 0x69, 0x3c, 0x30, 0x65, 0x65, 0x30, 0x3c, 0x69, 0x00, 0x55, 0x59, 0x0c, 
	0xa5, 0xf0, 0xfc, 0xa9, 0xc0, 0x95, 0x99, 0xcc, 0xcc, 0x99, 0x95, 0xc0, 0xa9, 0xfc, 0xf0, 0xa5, 
	0x30, 0x65, 0x69, 0x3c, 0x55, 0x00, 0x0c, 0x59, 0x59, 0x0c, 0x00, 0x55, 0x3c, 0x69, 0x65, 0x30, 
	0x3c, 0x69, 0x65, 0x30, 0x59, 0x0c, 0x00, 0x55, 0x55, 0x00, 0x0c, 0x59, 0x30, 0x65, 0x69, 0x3c, 
	0xa9, 0xfc, 0xf0, 0xa5, 0xcc, 0x99, 0x95, 0xc0, 0xc0, 0x95, 0x99, 0xcc, 0xa5, 0xf0, 0xfc, 0xa9, 
	0xa9, 0xfc, 0xf0, 0xa5, 0xcc, 0x99, 0x95, 0xc0, 0xc0, 0x95, 0x99, 0xcc, 0xa5, 0xf0, 0xfc, 0xa9, 
	0x3c, 0x69, 0x65, 0x30, 0x59, 0x0c, 0x00, 0x55, 0x55, 0x00, 0x0c, 0x59, 0x30, 0x65, 0x69, 0x3c, 
	0x30, 0x65, 0x69, 0x3c, 0x55, 0x00, 0x0c, 0x59, 0x59, 0x0c, 0x00, 0x55, 0x3c, 0x69, 0x65, 0x30, 
	0xa5, 0xf0, 0xfc, 0xa9, 0xc0, 0x95, 0x99, 0xcc, 0xcc, 0x99, 0x95, 0xc0, 0xa9, 0xfc, 0xf0, 0xa5, 
	0x0c, 0x59, 0x55, 0x00, 0x69, 0x3c, 0x30, 0x65, 0x65, 0x30, 0x3c, 0x69, 0x00, 0x55, 0x59, 0x0c, 
	0x99, 0xcc, 0xc0, 0x95, 0xfc, 0xa9, 0xa5, 0xf0, 0xf0, 0xa5, 0xa9, 0xfc, 0x95, 0xc0, 0xcc, 0x99, 
	0x95, 0xc0, 0xcc, 0x99, 0xf0, 0xa5, 0xa9, 0xfc, 0xfc, 0xa9, 0xa5, 0xf0, 0x99, 0xcc, 0xc0, 0x95, 
	0x00, 0x55, 0x59, 0x0c, 0x65, 0x30, 0x3c, 0x69, 0x69, 0x3c, 0x30, 0x65, 0x0c, 0x59, 0x55, 0x00, 
};


int uffs_GetEccSize256(uffs_Device *dev)
{
	dev = dev;
	return 3;
}

int uffs_GetEccSize512(uffs_Device *dev)
{
	dev = dev;
	return 6;
}

int uffs_GetEccSize1K(uffs_Device *dev)
{
	dev = dev;
	return 12;
}

int uffs_GetEccSize2K(uffs_Device *dev)
{
	dev = dev;
	return 24;
}


static void uffs_MakeEccChunk256(uffs_Device *dev, void *data, void *ecc, u16 len)
{
	u8 *pecc = (u8 *)ecc;
	u8 *p = (u8 *)data;
	u8 i, b, col_parity = 0, line_parity = 0, line_parity_prime = 0;

	dev = dev;

	for(i = 0; i < len; i++) {
		b = column_parity_tbl[*p++];
		col_parity ^= b;
		if(b & 0x01) { // odd number of bits in the byte
			line_parity ^= i;
			line_parity_prime ^= ~i;
		}
	}

	for(i = 0; i < 256 - len; i++) {
		b = column_parity_tbl[0];	//always use 0 for the rest of data
		col_parity ^= b;

		if(b & 0x01) { // odd number of bits in the byte
			line_parity ^= i;
			line_parity_prime ^= ~i;
		}
	}
	// ECC layout:
	// Byte[0]  P64   | P64'   | P32  | P32'  | P8   | P8'
	// Byte[1]  P1024 | P1024' | P512 | P512' | P128 | P128'
	// Byte[2]  P4    | P4'    | P2   | P2'   | 1    | 1
	pecc[0] = ~(line_parity_tbl[line_parity & 0xf] | line_parity_prime_tbl[line_parity_prime & 0xf]);
	pecc[1] = ~(line_parity_tbl[line_parity >> 4] | line_parity_prime_tbl[line_parity_prime >> 4]);
	pecc[2] = (~col_parity) | 0x03;

}

void uffs_MakeEcc256(uffs_Device *dev, void *data, void *ecc)
{
	uffs_MakeEccChunk256(dev, data, ecc, 256 - 3);
}

void uffs_MakeEcc512(uffs_Device *dev, void *data, void *ecc)
{
	uffs_MakeEccChunk256(dev, data, ecc, 256);
	uffs_MakeEccChunk256(dev, (char *)data + 256, (char *)ecc + 3, 256 - 6);
}

void uffs_MakeEcc1K(uffs_Device *dev, void *data, void *ecc)
{
	int i;
	char *p = (char *)data;
	char *pecc = (char *)ecc;

	for (i = 0; i < 3; i++) {
		uffs_MakeEccChunk256(dev, p, pecc, 256);
		p += 256; pecc += 3;
	}
	uffs_MakeEccChunk256(dev, p, pecc, 256 - 12);
}

void uffs_MakeEcc2K(uffs_Device *dev, void *data, void *ecc)
{
	int i;
	char *p = (char *)data;
	char *pecc = (char *)ecc;

	for (i = 0; i < 7; i++) {
		uffs_MakeEccChunk256(dev, p, pecc, 256);
		p += 256; pecc += 3;
	}
	uffs_MakeEccChunk256(dev, p, pecc, 256 - 24);
}

static int uffs_EccCorrectChunk256(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc, int errtop)
{
	u8 d0, d1, d2;		/* deltas */
	u8 *p = (u8 *)data;
	u8 *pread_ecc = (u8 *)read_ecc, *ptest_ecc = (u8 *)test_ecc;

	dev = dev;
	
	d0 = pread_ecc[0] ^ ptest_ecc[0];
	d1 = pread_ecc[1] ^ ptest_ecc[1];
	d2 = pread_ecc[2] ^ ptest_ecc[2];
	
	if((d0 | d1 | d2) == 0)
	{
		return 0;
	}
	
	if( ((d0 ^ (d0 >> 1)) & 0x55) == 0x55 &&
	    ((d1 ^ (d1 >> 1)) & 0x55) == 0x55 &&
	    ((d2 ^ (d2 >> 1)) & 0x54) == 0x54)
	{
		// Single bit (recoverable) error in data

		u8 b;
		u8 bit;
		
		bit = b = 0;		
		
		if(d1 & 0x80) b |= 0x80;
		if(d1 & 0x20) b |= 0x40;
		if(d1 & 0x08) b |= 0x20;
		if(d1 & 0x02) b |= 0x10;
		if(d0 & 0x80) b |= 0x08;
		if(d0 & 0x20) b |= 0x04;
		if(d0 & 0x08) b |= 0x02;
		if(d0 & 0x02) b |= 0x01;

		if(d2 & 0x80) bit |= 0x04;
		if(d2 & 0x20) bit |= 0x02;
		if(d2 & 0x08) bit |= 0x01;

		if (b >= (u8)errtop) return -1;

		p[b] ^= (1 << bit);
		
		return 1;
	}
	
	if((bits_tbl[d0]+bits_tbl[d1]+bits_tbl[d2]) == 1) {
		// error in ecc, no action need		
		return 1;
	}
	
	// Unrecoverable error
	return -1;
}

/** 
 * return:   0 -- no error
 *			-1 -- can not be correct
 *			>0 -- how many bits corrected
 */
int uffs_EccCorrect256(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc)
{
	return uffs_EccCorrectChunk256(dev, data, read_ecc, test_ecc, 256 - 3);
}

/** 
 * return:   0 -- no error
 *			-1 -- can not be correct
 *			>0 -- how many bits corrected
 */
int uffs_EccCorrect512(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc)
{
	int ret1, ret2;

	ret1 = uffs_EccCorrectChunk256(dev, data, read_ecc, test_ecc, 256);
	if (ret1 < 0) return ret1;

	ret2 = uffs_EccCorrectChunk256(dev, (char *)data + 256, (char *)read_ecc + 3, (const char *)test_ecc + 3, 250);
	if (ret2 < 0) return ret2;

	return ret1 + ret2;
}

/** 
 * return:   0 -- no error
 *			-1 -- can not be correct
 *			>0 -- how many bits corrected
 */
int uffs_EccCorrect1K(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc)
{
	int ret[4];
	int i;
	char *p = (char *)data;
	char *pecc = (char *)read_ecc;
	const char *ptecc = (const char *)test_ecc;

	for (i = 0; i < 3; i++) {
		ret[i] = uffs_EccCorrectChunk256(dev, p, pecc, ptecc, 256);
		if (ret[i] < 0) return ret[i];
		p += 256; pecc += 3; ptecc += 3;
	}
	ret[i] = uffs_EccCorrectChunk256(dev, p, pecc, ptecc, 256 - 12); //last chunk
	if (ret[i] < 0) return ret[i];

	return ret[0] + ret[1] + ret[2] + ret[3];
}

/** 
 * return:   0 -- no error
 *			-1 -- can not be correct
 *			>0 -- how many bits corrected
 */
int uffs_EccCorrect2K(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc)
{
	int ret[8];
	int i;
	char *p = (char *)data;
	char *pecc = (char *)read_ecc;
	const char *ptecc = (const char *)test_ecc;

	for (i = 0; i < 7; i++) {
		ret[i] = uffs_EccCorrectChunk256(dev, p, pecc, ptecc, 256);
		if (ret[i] < 0) return ret[i];
		p += 256; pecc += 3; ptecc += 3;
	}
	ret[i] = uffs_EccCorrectChunk256(dev, p, pecc, ptecc, 256 - 24); //last chunk
	if (ret[i] < 0) return ret[i];

	return ret[0] + ret[1] + ret[2] + ret[3] + ret[4] + ret[5] + ret[6] + ret[7];
}


