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

#ifndef __HOST_H
#define __HOST_H

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
	const char *basedir;
	const char *userdir;	// user's directory on UNIX platforms.
				// if user directories are enabled, basedir
				// and userdir will point to different
				// memory locations, otherwise to the same.
	int argc;
	char **argv;
	void *membase;
	int memsize;
} quakeparms_t;

// host
extern quakeparms_t host_parms;

extern cvar_t sys_ticrate;
extern cvar_t sys_nostdout;
extern cvar_t developer;

extern cvar_t pq_spam_rate;
extern cvar_t pq_spam_grace;
extern cvar_t pq_connectmute;
extern cvar_t pq_tempmute;
extern cvar_t pq_logbinds;
extern cvar_t pq_showedict;
extern cvar_t pq_dequake;
extern cvar_t pq_maxfps;

extern bool isDedicated;

extern bool host_initialized;		// true if into command execution
extern double host_frametime;
extern byte *host_basepal;
extern byte *host_colormap;
extern int host_framecount;		// incremented every frame, never reset
extern double realtime;			// not bounded in any way, changed at
// start of every frame, never reset

extern char host_worldname[MAX_QPATH];

extern byte *host_colormap;

extern char dequake[256];	// JPG 1.05 - dedicated console translation
extern cvar_t pq_dequake;	// JPG 1.05 - dedicated console translation

// skill level for currently loaded level (in case
// the user changes the cvar while the level is
// running, this reflects the level actually in use)
extern int current_skill;

void Host_ClearMemory(void);
void Host_ServerFrame(void);
void Host_InitCommands(void);
void Host_Init(quakeparms_t *parms);
void Host_Shutdown(void);
[[noreturn]] void Host_Error(const char *error, ...);
void Host_EndGame(const char *message, ...);
void Host_Frame(double time);
void Host_Quit_f(void);
void Host_ClientCommands(const char *fmt, ...);
void Host_ShutdownServer(bool crash);
void Host_WriteConfiguration(void);

void Host_Stopdemo_f(void);
void Host_Quit(void); // Get out, no questions asked

#endif /* __HOST_H */
