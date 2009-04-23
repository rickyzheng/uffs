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
 * \file uffs_os_linux.c
 * \brief emulation on linux host
 * \author Ricky Zheng
 */
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include <memory.h>
#include <stdlib.h>
#include <time.h>

#define PFX "linuxemu:"

int uffs_SemCreate(int n)
{
	//TODO: ... create semaphore, return semaphore handler (rather then return n) ...
	return n;
}

int uffs_SemWait(int sem)
{
	if (sem) {
		//TODO: ... wait semaphore available ...
	}
	return 0;
}

int uffs_SemSignal(int sem)
{
	if (sem) {
		//TODO: ... release semaphore ...
	}
	return 0;
}

int uffs_SemDelete(int sem)
{
	if (sem) {
		//TODO: ... delete semaphore ...
	}
	return 0;
}

int uffs_OSGetTaskId(void)
{
	//TODO: ... return current task ID ...
	return 0;
}


void uffs_CriticalEnter(void)
{
	//TODO: enter critical section (for example, disable IRQ?)
	return;
}

void uffs_CriticalExit(void)
{
	//TODO: exit from critical section (for example, enable IRQ?)
	return;
}

unsigned int uffs_GetCurDateTime(void)
{
	// FIXME: return system time, please modify this for your platform ! 
	//			or just return 0 if you don't care about file time.
	time_t tvalue;

	tvalue = time(NULL);
	
	return (unsigned int)tvalue;
}
