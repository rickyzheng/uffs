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
 * \file uffs_ecc.h
 * \brief file handle operations
 * \author Ricky Zheng, created 8th Jun, 2005
 */
#include <string.h>

#include "uffs/uffs_fs.h"
#include "uffs/uffs_config.h"
#include "uffs/uffs_core.h"

#define MAX_ECC_LENGTH	24	//2K page ecc length is 24 bytes.

int uffs_GetEccSize256(uffs_Device *dev);
void uffs_MakeEcc256(uffs_Device *dev, void *data, void *ecc);
int uffs_EccCorrect256(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc);

int uffs_GetEccSize512(uffs_Device *dev);
void uffs_MakeEcc512(uffs_Device *dev, void *data, void *ecc);
int uffs_EccCorrect512(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc);

int uffs_GetEccSize1K(uffs_Device *dev);
void uffs_MakeEcc1K(uffs_Device *dev, void *data, void *ecc);
int uffs_EccCorrect1K(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc);

int uffs_GetEccSize2K(uffs_Device *dev);
void uffs_MakeEcc2K(uffs_Device *dev, void *data, void *ecc);
int uffs_EccCorrect2K(uffs_Device *dev, void *data, void *read_ecc, const void *test_ecc);


