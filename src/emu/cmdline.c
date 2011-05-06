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
* \file cmdline.c
* \brief command line test interface
* \author Ricky Zheng, created in 22th Feb, 2007
*/

#include <string.h>
#include <stdio.h>
//#include <conio.h>
#include "cmdline.h"
#include "uffs/uffs_fs.h"

#define PROMPT "UFFS>"
#define MSG(msg,...) uffs_PerrorRaw(UFFS_ERR_NORMAL, msg, ## __VA_ARGS__)


#define MAX_CLI_ARGS_BUF_LEN	120
#define MAX_CLI_ARGS_NUM		20

#define MAX_CLI_CMDS	200


struct cli_arg {
	int argc;
	char *argv[MAX_CLI_ARGS_NUM];
	char _buf[MAX_CLI_ARGS_BUF_LEN];
};

static BOOL m_exit = FALSE;
static struct cli_commandset m_cmdset[MAX_CLI_CMDS] = {0};
static int m_cmdCount = 0;

static int m_last_return_code = 0;


static int cmdFind(const char *cmd);
static int cmd_help(int argc, char *argv[]);

/** exec command <n> times:
 *		exec <n> <cmd> [...]
 */
static int cmd_exec(int argc, char *argv[])
{
	int n = 0;
	int idx;

	if (argc < 3)
		return CLI_INVALID_ARG;
	if (sscanf(argv[1], "%d", &n) != 1)
		return CLI_INVALID_ARG;
	if (n <= 0)
		return CLI_INVALID_ARG;

	idx = cmdFind(argv[2]);
	if (idx < 0) {
		MSG("Unknown command '%s'\n", argv[2]);
		return -1;
	}
	else {
		argv += 2;
		while (n-- >= 0) {
			if (m_cmdset[idx].handler(argc - 2, argv) != 0)
				return -1;
		}
	}

	return 0;
}

/** 
 * if <cmd> is given, run <cmd>, and check <cmd> return against <x>.
 * if <cmd> is not given, check last return code against <x>.
 *		expect <x> [<cmd>] [...]
 */
static int cmd_expect(int argc, char *argv[])
{
	int x = 0;
	int idx;
	int ret;

	if (argc < 2)
		return CLI_INVALID_ARG;
	if (sscanf(argv[1], "%d", &x) != 1)
		return CLI_INVALID_ARG;

	if (argc > 2) {
		idx = cmdFind(argv[2]);
		if (idx < 0) {
			MSG("Unknown command '%s'\n", argv[2]);
			return -1;
		}
		argv += 2;
		ret = m_cmdset[idx].handler(argc - 2, argv);
	}
	else {
		ret = m_last_return_code;
	}

	return (ret == x ? 0 : -1);
}

/** if last command failed (return != 0), run <cmd>
 *    ! <cmd>
 */
static int cmd_failed(int argc, char *argv[])
{
	int idx;

	if (argc < 2)
		return CLI_INVALID_ARG;

	idx = cmdFind(argv[1]);
	if (idx < 0) {
		MSG("Unknown command '%s'\n", argv[1]);
		return -1;
	}
	argv++;
	return (m_last_return_code == 0 ? 0 : m_cmdset[idx].handler(argc - 1, argv));
}

/** print last command return code.
 *		@
 */
static int cmd_at(int argc, char *argv[])
{
	MSG("%d\n", m_last_return_code);
	return 0;
}

static int cmd_nop(int argc, char *argv[])
{
	return 0;
}

static int cmd_exit(int argc, char *argv[])
{
	m_exit = TRUE;
	return 0;
}

static struct cli_commandset default_cmdset[] = 
{
	{ cmd_help,		"help|?",	"[<command>]",		"show commands or help on one command" },
	{ cmd_exit,		"exit",		NULL,				"exit command line" },
	{ cmd_exec,		"*",		"<n> <cmd> [...]>",	"run <cmd> <n> times" },
	{ cmd_failed,	"!",		"<cmd> [...]",		"run <cmd> if last command failed" },
	{ cmd_at,		"@",		NULL,				"print return code of last command" },
	{ cmd_nop,		"#",		"[...]",			"do nothing" },
	{ cmd_expect,	"expect",	"<x> [<cmd>] [...]","expect <x> return from <cmd>(or last cmd if <cmd> not given)" },

	{ NULL, NULL, NULL, NULL }
};

static BOOL match_cmd(const char *src, int start, int end, const char *des)
{
	while (src[start] == ' ' && start < end) 
		start++;

	while (src[end] == ' ' && start < end) 
		end--;

	if ((int)strlen(des) == (end - start + 1)) {
		if (memcmp(src + start, des, end - start + 1) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL check_cmd(const char *cmds, const char *cmd)
{
	int start, end;

	for (start = end = 0; cmds[end] != 0 && cmds[end] != '|'; end++);

	while (end > start) {
		if (match_cmd(cmds, start, end - 1, cmd) == TRUE) 
			return TRUE;
		if (cmds[end] == 0) 
			break;
		if (cmds[end] == '|') {
			end++;
			for (start = end; cmds[end] != 0 && cmds[end] != '|'; end++);
		}
	} 

	return FALSE;
}

static int cmdFind(const char *cmd)
{
	int icmd;

	for (icmd = 0; m_cmdset[icmd].cmd != NULL; icmd++) {
		//MSG("cmdFind: Check cmd: %s with %s\n", cmd, m_cmdset[icmd].cmd);
		if (check_cmd(m_cmdset[icmd].cmd, cmd) == TRUE)
			return icmd;
	}
	return -1;
}

static void show_cmd_usage(int icmd)
{
	MSG("%s: %s\n", m_cmdset[icmd].cmd, m_cmdset[icmd].descr);
	MSG("Usage: %s %s\n", m_cmdset[icmd].cmd, m_cmdset[icmd].args ? m_cmdset[icmd].args : "");
}

static int cmd_help(int argc, char *argv[])
{
	int icmd;
	int i;

	if (argc < 2)  {
		MSG("Available commands:\n");
		for (icmd = 0; m_cmdset[icmd].cmd != NULL; icmd++) {
			MSG("%s", m_cmdset[icmd].cmd);
			for (i = strlen(m_cmdset[icmd].cmd); i%10; i++, MSG(" "));

			//if ((icmd & 7) == 7 || m_cmdset[icmd+1].cmd == NULL) MSG("\n");
		}
		MSG("\n");
	}
	else {
		icmd = cmdFind(argv[1]);
		if (icmd < 0) {
			MSG("No such command\n");
			return -1;
		}
		else {
			show_cmd_usage(icmd);
		}
	}

	return 0;
}

static void cli_parse_args(const char *cmd, struct cli_arg *arg)
{
	char *p;

	if (arg) {

		memset(arg, 0, sizeof(struct cli_arg));
		arg->argc = 0;

		if (cmd) {
			p = arg->_buf;
			while (*cmd && arg->argc < MAX_CLI_ARGS_NUM && (p - arg->_buf < MAX_CLI_ARGS_BUF_LEN)) {
				while(*cmd && (*cmd == ' ' || *cmd == '\t'))
					cmd++;

				arg->argv[arg->argc] = p;
				while (*cmd && (*cmd != ' ' && *cmd != '\t') && (p - arg->_buf < MAX_CLI_ARGS_BUF_LEN)) {
					*p++ = *cmd++;
				}
				*p++ = '\0';
				if (*(arg->argv[arg->argc]) == '\0')
					break;
				arg->argc++;
			}
		}
	}
}

int cli_interpret(const char *line)
{
	struct cli_arg arg;
	int idx;
	int ret = -1;

	cli_parse_args(line, &arg);

	if (arg.argc > 0) {
		idx = cmdFind(arg.argv[0]);
		if (idx < 0) {
			MSG("Unknown command '%s'\n", arg.argv[0]);
		}
		else {
			ret = m_cmdset[idx].handler(arg.argc, arg.argv);
			if (ret == CLI_INVALID_ARG) {
				show_cmd_usage(idx);
			}
		}
	}

	m_last_return_code = ret;

	return ret;
}

void cli_add_commandset(struct cli_commandset *cmds)
{
	int icmd;

	for (icmd = 0; cmds[icmd].cmd != NULL; icmd++) {
		memcpy(&(m_cmdset[m_cmdCount++]), &(cmds[icmd]), sizeof(struct cli_commandset));
	}
}

void cli_main_entry()
{
	char line[80];
	int linelen = 0;

	MSG("$ ");
	cli_add_commandset(default_cmdset);

	while (!m_exit) {
		char ch;
		ch = getc(stdin);
		switch (ch) {
		case 8:
		case 127:
			if (linelen > 0) {
				--linelen;
				MSG("\x08 \x08");
			}
			break;

		case '\r':
		case '\n':
			//MSG("\r\n");
			if (linelen > 0) {
				line[linelen] = 0;
				cli_interpret(line);
			}
			linelen = 0;
			MSG("$ ");
			break;

		case 21:
			while (linelen > 0) {
				--linelen;
				MSG("\x08 \x08");
			}
			break;

		default:
			if (ch >= ' ' && ch < 127 && linelen < sizeof(line) - 1) {
				line[linelen++] = ch;
				//MSG("%c", ch);
			}
		}
	}
}
