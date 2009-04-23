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
#ifndef UFFS_OS_H
#define UFFS_OS_H

#ifdef __cplusplus
extern "C"{
#endif

#include "uffs/uffs_device.h"
#include "uffs/uffs_core.h"

#define UFFS_TASK_ID_NOT_EXIST	-1

typedef int OSSEM;

/* OS specific functions */
int uffs_SemCreate(int n);
int uffs_SemWait(int sem);
int uffs_SemSignal(int sem);
int uffs_SemDelete(int sem);

void uffs_CriticalEnter(void);
void uffs_CriticalExit(void);

int uffs_OSGetTaskId(void);	//get current task id
unsigned int uffs_GetCurDateTime(void);

#ifdef __cplusplus
}
#endif


#endif

