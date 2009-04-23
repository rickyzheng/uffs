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
 * \file uffs_class.c
 * \brief define flash class table, and initialize flash class
 *        for each types of flash, should register one item in class table
 * \author Ricky Zheng, created 8th Jun, 2005
 */

#include "uffs/uffs_fs.h"
#include "uffs/uffs_config.h"
#include <string.h>

#define PFX "class:"

extern struct uffs_FlashClassSt Samsung_FlashClass;		// Samsung nand flash class
//extern struct uffs_FlashClassSt Toshiba_FlashClass;
extern struct uffs_FlashClassSt Simram_FlashClass;		// when using RAM simulator

static struct uffs_FlashClassSt *  flash_class_tab[] = {
	&Samsung_FlashClass,
//  &Toshiba_FlashClass,
	&Simram_FlashClass,
	NULL,					/*< terminated by NULL */
};

#define MAX_CHIP_ID_LIST	40

URET uffs_InitFlashClass(uffs_Device *dev)
{
	struct uffs_FlashClassSt **work_p = &(flash_class_tab[0]);
	struct uffs_FlashClassSt *work;
	int i;

	while(work_p != NULL) {
		work = *work_p;
		if (work == NULL) return U_FAIL;
		if(dev->attr->maker == work->maker) {
			if (work->id_list) {	/* if flash class give an id list, we need to check the list */
				for(i = 0; i < MAX_CHIP_ID_LIST; i++) {
					if(work->id_list[i] == -1){
						return U_FAIL;
					}
					if(work->id_list[i] == dev->attr->id) {
						break;
					}
				}
			}
			if (work->InitClass) {  /* if flash class has init function, call it. */
				if(work->InitClass(dev, dev->attr->id) != U_SUCC) {
					return U_FAIL;
				}
			}
			else {
				/* no init function? copy default static flash class's ops to device */
				dev->flash = work->flash;
				if (dev->flash == NULL) return U_FAIL;
			}
			return U_SUCC;
		}
		work_p++;	/* try next flash class */
	}

	return U_FAIL;
}

