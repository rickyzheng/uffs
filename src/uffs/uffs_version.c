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
 * \file uffs_version.c
 * \brief uffs version information
 * \author Ricky Zheng, created 8th May, 2005
 */

#include "uffs/uffs_version.h"

#include <stdio.h>
#define PFX "ver:"


static char version_buf[8];

const char * uffs_Version2Str(int ver)
{
	sprintf(version_buf, "%1d.%02d.%04d", (ver&0xff000000) >> 24, (ver&0xff0000) >> 16, (ver&0xffff));
	return version_buf;
}

int uffs_GetVersion(void)
{
	return UFFS_VERSION;
}

int uffs_GetMainVersion(int ver)
{
	return (ver&0xff000000) >> 24;
}

int uffs_GetMinorVersion(int ver)
{
	return (ver&0xff0000) >> 16;
}
