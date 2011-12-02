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
 * \file uffs_debug.c
 * \brief output debug messages
 * \author Ricky Zheng, created 10th May, 2005
 */
#include "uffs_config.h"
#include "uffs/uffs_os.h"
#include "uffs/uffs_public.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define MAX_DEBUG_LINE_LENGTH	128

static struct uffs_DebugMsgOutputSt * m_ops = NULL;

static void uffs_vprintf(const char *fmt, va_list args)
{
	char line_buf[MAX_DEBUG_LINE_LENGTH];

	if (m_ops && m_ops->output) {
		vsnprintf(line_buf, MAX_DEBUG_LINE_LENGTH, fmt, args);
		m_ops->output(line_buf);
	}
}

/**
 * \brief Initialize debug message output functions
 */
URET uffs_InitDebugMessageOutput(struct uffs_DebugMsgOutputSt *ops)
{
	m_ops = ops;

	if (m_ops == NULL || m_ops->output == NULL) {
		m_ops = NULL;
		return U_FAIL;
	}
	else if (m_ops->vprintf == NULL)
		m_ops->vprintf = uffs_vprintf;

	return U_SUCC;
}


#ifdef CONFIG_ENABLE_UFFS_DEBUG_MSG

/**
 * \brief The main debug message output function
 */
void uffs_DebugMessage(int level, const char *prefix,
					   const char *suffix, const char *errFmt, ...)
{
	va_list arg;

	if (m_ops && level >= UFFS_DBG_LEVEL) {
		if (prefix)
			m_ops->output(prefix);

		va_start(arg, errFmt);
		m_ops->vprintf(errFmt, arg);
		va_end(arg);

		if (suffix)
			m_ops->output(suffix);
	}
}

/**
 * \brief Called when an assert occurred.
 * This method is called when an assert occurred and should stop the
 * application from running, as this there is a severe error condition.
 * \param[in] file Source filename
 * \param[in] line Source line of code
 * \param[in] msg Assert message
 */
void uffs_AssertCall(const char *file, int line, const char *msg, ...)
{
	va_list args;
	char buf[32];

	if (m_ops) {
		m_ops->output("ASSERT ");
		m_ops->output(file);
		sprintf(buf, ":%d - :", line);
		m_ops->output(buf);
		va_start(args, msg);
		m_ops->vprintf(msg, args);
		va_end(args);
		m_ops->output(TENDSTR);
	}
}
#else

void uffs_DebugMessage(int level, const char *prefix, const char *suffix, const char *errFmt, ...) {};
void uffs_AssertCall(const char *file, int line, const char *msg, ...) {};

#endif
