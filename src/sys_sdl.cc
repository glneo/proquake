/*
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "quakedef.h"

#include <SDL2/SDL.h>

int nostdout = 0;

const char *basedir = ".";

void Sys_Printf(const char *fmt, ...)
{
	va_list argptr;
	char text[2048]; // JPG 3.30 - increased this from 1024 to 2048
	unsigned char *p;

	if (nostdout)
		return;

	va_start(argptr, fmt);
	vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	// JPG 1.05 - translate to plain text
	if (pq_dequake.value)
	{
		unsigned char *ch;
		for (ch = (unsigned char *)text; *ch; ch++)
			*ch = dequake[*ch];
	}

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	for (p = (unsigned char *) text; *p; p++)
	{
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}

	// JPG 3.00 - rcon (64 doesn't mean anything special, but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && ((unsigned)rcon_message.cursize < rcon_message.maxsize - strlen(text) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString(&rcon_message, text);
	}
}

void Sys_Quit(void)
{
	Host_Shutdown();
	fflush(stdout);
	exit(0);
}

double Sys_DoubleTime (void)
{
	return SDL_GetTicks() / 1000.0;
}

void Sys_mkdir(const char *path)
{
	mkdir(path, 0777);
}

/* returns -1 if not present */
bool Sys_FileExists(const char *path)
{
	bool exists = false;

	FILE *handle = fopen(path, "rb");
	if (handle)
	{
		exists = true;
		fclose(handle);
	}

	return exists;
}

FILE *Sys_FileOpenRead(const char *path)
{
	FILE *handle = fopen(path, "rb");
	if (!handle)
		Sys_Printf("Error opening %s: %s\n", path, strerror(errno));

	return handle;
}

FILE *Sys_FileOpenWrite(const char *path)
{
	FILE *handle = fopen(path, "wb");
	if (!handle)
		Sys_Printf("Error opening %s: %s\n", path, strerror(errno));

	return handle;
}

int Sys_FileWrite(FILE *handle, void *src, int count)
{
	return fwrite(src, 1, count, handle);
}

void Sys_FileSeek(FILE *handle, int position)
{
	fseek(handle, position, SEEK_SET);
}

int Sys_FileRead(FILE *handle, void *dest, int count)
{
	return fread(dest, 1, count, handle);
}

void Sys_FileClose(FILE *handle)
{
	fclose(handle);
}

char *Sys_GetClipboardData(void)
{
	return SDL_GetClipboardText();
}

void Sys_CopyToClipboard(char *text)
{
	SDL_SetClipboardText(text);
}

void Sys_Sleep(unsigned long msecs)
{
	SDL_Delay (msecs);
}

char *Sys_ConsoleInput(void)
{
//	static char text[256];
//	int len;
//	fd_set fdset;
//	struct timeval timeout;
//
//	if (cls.state == ca_dedicated)
//	{
////		FD_ZERO(&fdset);
////		FD_SET(0, &fdset); // stdin
//		timeout.tv_sec = 0;
//		timeout.tv_usec = 0;
//		if (select(1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
//			return NULL;
//
//		len = read(0, text, sizeof(text));
//		if (len < 1)
//			return NULL;
//		text[len - 1] = 0;    // rip off the /n and terminate
//
//		return text;
//	}

	return NULL;
}

int main(int argc, char **argv)
{
	COM_InitArgv(argc, argv);

	quakeparms_t parms;
	memset(&parms, 0, sizeof(parms));
	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 32 * 1024 * 1024; // 32MB default
	int j = COM_CheckParm("-mem");
	if (j)
		parms.memsize = (int) (atof(com_argv[j + 1]) * 1024 * 1024);
	parms.membase = malloc(parms.memsize);
	parms.basedir = basedir;

	isDedicated = COM_CheckParm("-dedicated");

	Host_Init(&parms);

	if (COM_CheckParm("-nostdout"))
		nostdout = 1;

	Sys_Printf("%s -- Version %s\n", ENGINE_NAME, ENGINE_VERSION);

	double oldtime = Sys_DoubleTime() - 0.1;

	if (isDedicated)
	{
		while (true)
		{
			double newtime = Sys_DoubleTime ();
			double time = newtime - oldtime;

			while (time < sys_ticrate.value)
			{
				SDL_Delay(1);
				newtime = Sys_DoubleTime();
				time = newtime - oldtime;
			}
			oldtime = newtime;
			Host_Frame(time);
//			Host_Frame(sys_ticrate.value);
		}
	}
	else
	{
		while (true)
		{
			double newtime = Sys_DoubleTime ();
			double time = newtime - oldtime;
			Host_Frame(time);
			oldtime = newtime;
		}
	}
}
