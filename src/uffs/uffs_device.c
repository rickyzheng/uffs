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
 * \file uffs_device.c
 * \brief uffs device operation
 * \author Ricky Zheng, created 10th May, 2005
 */
#include "uffs/uffs_device.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include <string.h>

#define PFX "dev: "

static struct uffs_mountTableSt *g_mtabHead = NULL;

uffs_mountTable * uffs_GetMountTable(void)
{
	return g_mtabHead;
}

int uffs_RegisterMountTable(uffs_mountTable *mtab)
{
	uffs_mountTable *work = g_mtabHead;

	if (mtab == NULL) 
		return -1;

	if (work == NULL) {
		g_mtabHead = mtab;
		return 0;
	}

	while (work) {
		if (mtab == work) {
			/* already registered */
			return 0;
		}
		if (work->next == NULL) {
			work->next = mtab;
			mtab->next = NULL;
			return 0;
		}
		work = work->next;
	}

	return -1;
}

uffs_Device * uffs_GetDevice(const char *mountPoint)
{
	struct uffs_mountTableSt *devTab = uffs_GetMountTable();

	while (devTab) {
		if (strcmp(mountPoint, devTab->mountPoint) == 0) {
			devTab->dev->ref_count++;
			return devTab->dev;
		}
		devTab = devTab->next;
	}

	return NULL;
}

const char * uffs_GetDeviceMountPoint(uffs_Device *dev)
{
	struct uffs_mountTableSt * devTab = uffs_GetMountTable();

	while (devTab) {
		if (devTab->dev == dev) {
			return devTab->mountPoint;
		}
		devTab = devTab->next;
	}

	return NULL;	
}

void uffs_PutDevice(uffs_Device *dev)
{
	dev->ref_count--;
}

URET uffs_DeviceInitLock(uffs_Device *dev)
{
	dev->lock.sem = uffs_SemCreate(1);
	dev->lock.task_id = UFFS_TASK_ID_NOT_EXIST;
	dev->lock.counter = 0;

	return U_SUCC;
}

URET uffs_DeviceReleaseLock(uffs_Device *dev)
{
	if (dev->lock.sem) {
		uffs_SemDelete(dev->lock.sem);
		dev->lock.sem = 0;
	}

	return U_SUCC;
}

URET uffs_DeviceLock(uffs_Device *dev)
{

	uffs_SemWait(dev->lock.sem);
	
	if (dev->lock.counter != 0) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"Lock device, counter %d NOT zero?!\n", dev->lock.counter);
	}

	dev->lock.counter++;

	return U_SUCC;
}

URET uffs_DeviceUnLock(uffs_Device *dev)
{

	dev->lock.counter--;

	if (dev->lock.counter != 0) {
		uffs_Perror(UFFS_ERR_NORMAL, PFX"Unlock device, counter %d NOT zero?!\n", dev->lock.counter);
	}
	
	uffs_SemSignal(dev->lock.sem);

	return U_SUCC;
}

