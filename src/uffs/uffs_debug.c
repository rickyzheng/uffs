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
 * \file uffs_debug.c
 * \brief output debug messages
 * \author Ricky Zheng, created 10th May, 2005
 */
#include "uffs/uffs_public.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if !defined(_UBASE_)
#define ENABLE_DEBUG
//#define OUTPUT_TOFILE
#endif

#if !defined(_UBASE_)


#ifdef OUTPUT_TOFILE
#define DEBUG_LOGFILE	"log.txt"
#endif

void uffs_Perror( int level, const char *errFmt, ...)
{

#ifdef ENABLE_DEBUG
	if (level >= UFFS_DBG_LEVEL) {

		char buf[1024] = "";
#ifdef OUTPUT_TOFILE
		FILE *fp = NULL;	
#endif
		
		va_list arg;

		if( strlen(errFmt) > 800 ) {
			// dangerous!!
			printf("uffs_Perror buffer is not enough !\r\n");
			return;
		}

		va_start(arg, errFmt);
		vsprintf(buf, errFmt, arg);
		va_end(arg);

#ifdef OUTPUT_TOFILE
		fp = fopen(DEBUG_LOGFILE, "a+b");
		if(fp) {
			fwrite(buf, 1, strlen(buf), fp);
			fclose(fp);
		}
#else
		printf("%s", buf);
#endif
	}
#endif //ENABLE_DEBUG
}

#else

#define ENABLE_DEBUG

#include <uBase.h>
#include <sys/debug.h>


void uffs_Perror( int level, const char *errFmt, ...)
{
#ifdef ENABLE_DEBUG
	va_list args;
	if (level >= UFFS_DBG_LEVEL) {
		va_start(args, errFmt);
		//uffs_vTrace(errFmt, args);
		dbg_simple_vprintf(errFmt, args);
		va_end(args);
	}
#else
	level = level;
	errFmt = errFmt;
#endif //ENABLE_DEBUG
}

#endif

