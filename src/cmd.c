/*
 * Quake script command processing module
 *
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include "quakedef.h"

#define MAX_ALIAS_NAME 32
typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;
cmdalias_t *cmd_alias;

/*
 =============================================================================

 COMMAND BUFFER

 =============================================================================
 */

sizebuf_t cmd_text;
bool cmd_wait = false;

void Cbuf_Init(void)
{
	// space for commands and script files
	SZ_Alloc(&cmd_text, 8192);
}

/* Adds command text at the end of the buffer */
void Cbuf_AddText(char *text)
{
	int l;

	l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Printf("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write(&cmd_text, text, strlen(text));
}

/*
 * Adds command text immediately after the current command
 * Adds a \n to the text
 */
void Cbuf_InsertText(char *text)
{
	char *temp = NULL;
	int templen = cmd_text.cursize;

	// copy off any commands still remaining in the exec buffer
	if (templen)
	{
		temp = Q_malloc(templen);
		memcpy(temp, cmd_text.data, templen);
		SZ_Clear(&cmd_text);
	}

	// add the entire text of the file
	Cbuf_AddText(text);

	// add the copied off data
	if (templen)
	{
		SZ_Write(&cmd_text, temp, templen);
		free(temp);
	}
}

void Cbuf_Execute(void)
{
	int i;
	char *text;
	char line[1024];
	int quotes;
	int notcmd; // JPG - so that the ENTIRE line can be forwarded

	while (cmd_text.cursize)
	{
		// find a \n or ; line break
		text = (char *) cmd_text.data;

		quotes = 0;
		notcmd = strncmp(text, "cmd ", 4);  // JPG - so that the ENTIRE line can be forwarded
		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"')
				quotes++;
			if (!(quotes & 1) && text[i] == ';' && notcmd) // JPG - added && cmd so that the ENTIRE line can be forwareded
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}

		memmove(line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer
		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString(line, src_command);

		if (cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
 =============================================================================

 COMMAND EXECUTION

 =============================================================================
 */

typedef struct cmd_function_s
{
	struct cmd_function_s *next;
	char *name;
	xcommand_t function;
} cmd_function_t;
cmd_function_t *cmd_functions = NULL; // possible commands to execute

#define	MAX_ARGS		80

static int cmd_argc;
static char *cmd_argv[MAX_ARGS];
static char *cmd_null_string = "";
static char *cmd_args = NULL;

cmd_source_t cmd_source;

int Cmd_Argc(void)
{
	return cmd_argc;
}

char *Cmd_Argv(int arg)
{
	if ((unsigned) arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

char *Cmd_Args(void)
{
	return cmd_args;
}

bool Cmd_Exists(char *cmd_name)
{
	cmd_function_t *cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcmp(cmd_name, cmd->name))
			return true;
	}

	return false;
}

/* Parses the given string into command line tokens */
void Cmd_TokenizeString(char *text)
{
	int i;

// clear the args from the last string
	for (i = 0; i < cmd_argc; i++)
		Z_Free(cmd_argv[i]);

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			cmd_args = text;

		text = COM_Parse(text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = Z_Malloc(strlen(com_token) + 1);
			strcpy(cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}

/* A complete command line has been parsed, so try to execute it */
void Cmd_ExecuteString(char *text, cmd_source_t src)
{
	cmd_function_t *cmd;
	cmdalias_t *a;

	Con_DPrintf("Cmd_ExecuteString: %s \n", text);
	cmd_source = src;
	Cmd_TokenizeString(text);

	// execute the command line
	if (!Cmd_Argc())
		return; // no tokens

	// check functions
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (!strcasecmp(cmd_argv[0], cmd->name))
		{
			cmd->function();
			return;
		}
	}

	// check alias
	for (a = cmd_alias; a; a = a->next)
	{
		if (!strcasecmp(cmd_argv[0], a->name))
		{
			Cbuf_InsertText(a->value);
			return;
		}
	}

	// check cvars
	if (!Cvar_Command())
		Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));
}

/*
 =============================================================================

 COMMANDS

 =============================================================================
 */

/*
 * Adds command line parameters as script statements
 * Commands lead with a +, and continue until a - or another +
 * quake +prog jctest.qp +cmd amlev1
 * quake -nosound +cmd amlev1
 */
void Cmd_StuffCmds_f(void)
{
	if (Cmd_Argc() != 1)
	{
		Con_Printf("stuffcmds : execute command line parameters\n");
		return;
	}

	// build the combined string to parse from
	int s = 0;
	for (int i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			continue; // NEXTSTEP nulls out -NXHost
		s += strlen(com_argv[i]) + 1;
	}
	if (!s)
		return;

	char *text = Q_malloc(s + 1);
	text[0] = 0;
	for (int i = 1; i < com_argc; i++)
	{
		if (!com_argv[i])
			continue; // NEXTSTEP nulls out -NXHost
		strcat(text, com_argv[i]); // Dynamic string: no strlcat required
		if (i != com_argc - 1)
			strcat(text, " "); // Dynamic string: no strlcat required
	}

	// pull out the commands
	char *build = Q_malloc(s + 1);
	build[0] = 0;

	for (int i = 0; i < (s - 1); i++)
	{
		if (text[i] == '+')
		{
			i++;
			int j;
			for (j = i; (text[j] != '+') && (text[j] != '-') && (text[j] != 0); j++)
				;

			char c = text[j];
			text[j] = 0;
			strcat(build, text + i); // Dynamic string: no strlcat required
			strcat(build, "\n"); // Dynamic string: no strlcat required
			text[j] = c;
			i = j - 1;
		}
	}

	if (build[0])
		Cbuf_InsertText(build);

	free(text);
	free(build);
}

void Cmd_Exec_f(void)
{
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf("exec <filename> : execute a script file\n");
		return;
	}

	strlcpy(name, Cmd_Argv(1), sizeof(name));
	char *f = (char *)COM_LoadMallocFile(name);
	if (!f)
	{
		char *p;

		p = COM_SkipPath(name);
		if (!strchr(p, '.'))
		{	// no extension, so try the default (.cfg)
			strlcat(name, ".cfg", sizeof(name));
			f = (char *) COM_LoadMallocFile(name);
		}

		if (!f)
		{
			Con_Printf("couldn't exec %s\n", name);
			return;
		}
	}

	Con_Printf("exec'ing %s\n", name);

	Cbuf_InsertText(f);
	free(f);
}

/* Just prints the rest of the line to the console */
void Cmd_Echo_f(void)
{
	for (int i = 1; i < Cmd_Argc(); i++)
		Con_Printf("%s ", Cmd_Argv(i));
	Con_Printf("\n");
}

/*
 * Causes execution of the remainder of the command buffer to be delayed until
 * next frame.  This allows commands like:
 * bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
 */
void Cmd_Wait_f(void)
{
	cmd_wait = true;
}

/* Creates a new command that executes a command string (possibly ; separated) */
void Cmd_Alias_f(void)
{
	cmdalias_t *a;
	char cmd[1024];
	int i, c;
	char *s;

	switch (Cmd_Argc())
	{
	case 1: //list all aliases
		for (a = cmd_alias, i = 0; a; a = a->next, i++)
			Con_SafePrintf("   %s: %s", a->name, a->value);
		if (i)
			Con_SafePrintf("%i alias command(s)\n", i);
		else
			Con_SafePrintf("no alias commands found\n");
		break;

	case 2: //output current alias string
		for (a = cmd_alias; a; a = a->next)
			if (!strcmp(Cmd_Argv(1), a->name))
				Con_Printf("   %s: %s", a->name, a->value);
		break;

	default: //set alias string
		s = Cmd_Argv(1);
		if (strlen(s) >= MAX_ALIAS_NAME)
		{
			Con_Printf("Alias name is too long\n");
			return;
		}

		// if the alias already exists, reuse it
		for (a = cmd_alias; a; a = a->next)
		{
			if (!strcmp(s, a->name))
			{
				free(a->value);
				break;
			}
		}

		if (!a)
		{
			a = Q_malloc(sizeof(cmdalias_t));
			a->next = cmd_alias;
			cmd_alias = a;
		}
		strcpy(a->name, s);

		// copy the rest of the command line
		cmd[0] = 0; // start out with a null string
		c = Cmd_Argc();
		for (i = 2; i < c; i++)
		{
			strlcat(cmd, Cmd_Argv(i), sizeof(cmd));
			if (i != c)
				strlcat(cmd, " ", sizeof(cmd));
		}
		strlcat(cmd, "\n", sizeof(cmd));

		a->value = Q_strdup(cmd);
		break;
	}
}

void Cmd_Unalias_f(void)
{
	cmdalias_t *a, *prev;

	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("unalias <name> : delete alias\n");
		break;
	case 2:
		for (prev = a = cmd_alias; a; a = a->next)
		{
			if (!strcmp(Cmd_Argv(1), a->name))
			{
				prev->next = a->next;
				free(a->value);
				free(a);
				prev = a;
				return;
			}
			prev = a;
		}
		break;
	}
}

void Cmd_Unaliasall_f(void)
{
	while (cmd_alias)
	{
		cmdalias_t *temp = cmd_alias->next;
		free(cmd_alias->value);
		free(cmd_alias);
		cmd_alias = temp;
	}
}

/* Sends the entire command line over to the server */
void Cmd_ForwardToServer_f(void)
{
	//from ProQuake --start
	char *src, *dst, buff[128];			// JPG - used for say/say_team formatting
	int minutes, seconds, match_time;	// JPG - used for %t
	//from ProQuake --end

	if (cls.state != ca_connected)
	{
		Con_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	MSG_WriteByte(&cls.message, clc_stringcmd);

	//----------------------------------------------------------------------
	// JPG - handle say separately for formatting--start
	if ((!strcasecmp(Cmd_Argv(0), "say") || !strcasecmp(Cmd_Argv(0), "say_team")) && Cmd_Argc() > 1)
	{
		SZ_Print(&cls.message, Cmd_Argv(0));
		SZ_Print(&cls.message, " ");

		src = Cmd_Args();
		dst = buff;
		while (*src && dst - buff < 100)
		{
			if (*src == '%')
			{
				switch (*++src)
				{
				case 'h':
					dst += sprintf(dst, "%d", cl.stats[STAT_HEALTH]);
					break;

				case 'a':
					dst += sprintf(dst, "%d", cl.stats[STAT_ARMOR]);
					break;

				case 'c':
					dst += sprintf(dst, "%d", cl.stats[STAT_CELLS]);
					break;

				case 'x':
					dst += sprintf(dst, "%d", cl.stats[STAT_ROCKETS]);
					break;

				case '%':
					*dst++ = '%';
					break;

				case 't':
					if ((cl.minutes || cl.seconds) && cl.seconds < 128)
					{
						if (cl.match_pause_time)
							match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
						else
							match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.time - cl.last_match_time));
						minutes = match_time / 60;
						seconds = match_time - 60 * minutes;
					}
					else
					{
						minutes = cl.time / 60;
						seconds = cl.time - 60 * minutes;
						minutes &= 511;
					}
					dst += sprintf(dst, "%d:%02d", minutes, seconds);
					break;

				default:
					*dst++ = '%';
					*dst++ = *src;
					break;
				}
				if (*src)
					src++;
			}
			else
				*dst++ = *src++;
		}
		*dst = 0;

		SZ_Print(&cls.message, buff);
		return;
	}
	// JPG - handle say separately for formatting--end
	//----------------------------------------------------------------------

	if (strcasecmp(Cmd_Argv(0), "cmd"))
	{
		SZ_Print(&cls.message, Cmd_Argv(0));
		SZ_Print(&cls.message, " ");
	}
	if (Cmd_Argc() > 1)
		SZ_Print(&cls.message, Cmd_Args());
	else
		SZ_Print(&cls.message, "\n");
}

/* List console commands */
void Cmd_CmdList_f(void)
{
	char *partial = NULL;

	if (Cmd_Argc() > 1)
		partial = Cmd_Argv(1);

	Con_Printf("\n");

	int count = 0;
	for (cmd_function_t *cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if (partial && strncmp(partial, cmd->name, strlen(partial)))
			continue;

		Con_Printf("%s\n", cmd->name);

		count++;
	}

	Con_Printf("\n%i command(s)", count);

	if (partial)
		Con_Printf(" beginning with \"%s\"", partial);

	Con_Printf("\n\n");
}

void Cmd_AddCommand(char *cmd_name, xcommand_t function)
{
	if (host_initialized) // because hunk allocation would get stomped
		Sys_Error("Cmd_AddCommand after host_initialized");

	// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	// fail if the command already exists
	if (Cmd_Exists(cmd_name))
	{
		Con_Printf("Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	cmd_function_t *cmd = Q_malloc(sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;

	// insert each entry in alphabetical order
	if (cmd_functions == NULL || strcmp(cmd->name, cmd_functions->name) < 0) //insert at front
	{
		cmd->next = cmd_functions;
		cmd_functions = cmd;
	}
	else //insert later
	{
		cmd_function_t *prev = cmd_functions;
		cmd_function_t *cursor = cmd_functions->next;
		while ((cursor != NULL) && (strcmp(cmd->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		cmd->next = prev->next;
		prev->next = cmd;
	}
}

void Cmd_Init(void)
{
	// register our commands
	Cmd_AddCommand("stuffcmds", Cmd_StuffCmds_f);

	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("wait", Cmd_Wait_f);

	Cmd_AddCommand("alias", Cmd_Alias_f);
	Cmd_AddCommand("unalias", Cmd_Unalias_f);

	Cmd_AddCommand("cmd", Cmd_ForwardToServer_f);

	Cmd_AddCommand("cmdlist", Cmd_CmdList_f);
}
