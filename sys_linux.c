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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#include "quakedef.h"

qboolean isDedicated = false;

int extmousex, extmousey;
cvar_t cl_keypad = { "cl_keypad", "0", true };

int nostdout = 0;

char *basedir = ".";
char *cachedir = "/tmp";

/*
 ===============================================================================

 SYNCHRONIZATION - JPG 3.30

 ===============================================================================
 */

int hlock;

/*
 ================
 Sys_GetLock
 ================
 */
void Sys_GetLock(void)
{
	int i;

	for (i = 0; i < 10; i++)
	{
		hlock = open(va("%s/lock.dat", com_gamedir), O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (hlock != -1)
			return;
		sleep(1);
	}

	Sys_Printf("Warning: could not open lock; using crowbar\n");
}

/*
 ================
 Sys_ReleaseLock
 ================
 */
void Sys_ReleaseLock(void)
{
	if (hlock != -1)
		close(hlock);
	unlink(va("%s/lock.dat", com_gamedir));
}

// =======================================================================
// General routines
// =======================================================================

void Sys_Printf(char *fmt, ...)
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
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - strlen(text) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString(&rcon_message, text);
	}
}

void Sys_Quit(void)
{
	Host_Shutdown();
	fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~FNDELAY);
	fflush(stdout);
	exit(0);
}

void Sys_Init(void)
{
}

void Sys_Error(char *error, ...)
{
	va_list argptr;
	char string[1024];

	// change stdin to non blocking
	fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~FNDELAY);

	va_start(argptr, error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);
	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();
	exit(1);
}

void Sys_Warn(char *warning, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, warning);
	vsnprintf(string, sizeof(string), warning, argptr);
	va_end(argptr);
	fprintf(stderr, "Warning: %s", string);
}

/*
 ============
 Sys_FileTime

 returns -1 if not present
 ============
 */
int Sys_FileTime(char *path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

void Sys_mkdir(char *path)
{
	mkdir(path, 0777);
}

int Sys_FileOpenRead(char *path, int *handle)
{
	int h;
	struct stat fileinfo;

	h = open(path, O_RDONLY, 0666);
	*handle = h;
	if (h == -1)
		return -1;

	if (fstat(h, &fileinfo) == -1)
		Sys_Error("Error fstating %s", path);

	return fileinfo.st_size;
}

int Sys_FileOpenWrite(char *path)
{
	int handle;

	umask(0);

	handle = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);

	if (handle == -1)
		Sys_Error("Error opening %s: %s", path, strerror(errno));

	return handle;
}

int Sys_FileWrite(int handle, void *src, int count)
{
	return write(handle, src, count);
}

void Sys_FileClose(int handle)
{
	close(handle);
}

void Sys_FileSeek(int handle, int position)
{
	lseek(handle, position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
	return read(handle, dest, count);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
	va_list argptr;
	static char data[1024];
	int fd;

	va_start(argptr, fmt);
	vsnprintf(data, sizeof(data), fmt, argptr);
	va_end(argptr);

	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (write(fd, data, strlen(data)) < 0)
		Con_Printf("debug log file write failed\n");
	close(fd);
}

void Sys_EditFile(char *filename)
{
	char cmd[256], *term, *editor;

	term = getenv("TERM");
	if (term && !strcmp(term, "xterm"))
	{
		editor = getenv("VISUAL");
		if (!editor)
			editor = getenv("EDITOR");
		if (!editor)
			editor = getenv("EDIT");
		if (!editor)
			editor = "vi";
		sprintf(cmd, "xterm -e %s %s", editor, filename);
		if (system(cmd) < 0)
			Con_Printf("Failed to start editor\n");
	}
}

double Sys_FloatTime(void)
{
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

void Sys_OpenFolder_f(void)
{
	Con_Printf("folder command not implemented in linux yet\n");
	return;
}

//TODO: gets text from clipboard
char *Sys_GetClipboardData(void)
{
	return "";
}

//TODO: copies given text to clipboard
void Sys_CopyToClipboard(char *text)
{
}

char *Sys_ConsoleInput(void)
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (cls.state == ca_dedicated)
	{
		FD_ZERO(&fdset);
		FD_SET(0, &fdset); // stdin
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if (select(1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
			return NULL;

		len = read(0, text, sizeof(text));
		if (len < 1)
			return NULL;
		text[len - 1] = 0;    // rip off the /n and terminate

		return text;
	}

	return NULL;
}

int main(int argc, char **argv)
{
	double time, oldtime, newtime;
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;
	int j;

	memset(&parms, 0, sizeof(parms));

	COM_InitArgv(argc, argv);
	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 32 * 1024 * 1024;

	j = COM_CheckParm("-mem");
	if (j)
		parms.memsize = (int) (atof(com_argv[j + 1]) * 1024 * 1024);
	parms.membase = malloc(parms.memsize);

	parms.basedir = basedir;
	// caching is disabled by default, use -cachedir to enable
	// parms.cachedir = cachedir;

	Host_Init(&parms);

	Sys_Init();

	if (COM_CheckParm("-nostdout"))
		nostdout = 1;

	Sys_Printf("Linux Quake -- Version %s\n", ENGINE_VERSION);

	oldtime = Sys_FloatTime() - 0.1;
	while (true)
	{
		// find time spent rendering last frame
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated)
		{
			// play vcrfiles at max speed
			if (time < sys_ticrate.value && (vcrFile == -1 || recording))
			{
				usleep(1);
				continue; // not time to run a server only tic yet
			}
			time = sys_ticrate.value;
		}

		if (time > sys_ticrate.value * 2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame(time);
	}
}
