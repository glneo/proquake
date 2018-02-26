/*
 * Non-portable functions
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

#ifndef __SYS_H
#define __SYS_H

// file IO
void Sys_mkdir(const char *path);
bool Sys_FileExists(const char *path);
FILE *Sys_FileOpenRead(const char *path);
FILE *Sys_FileOpenWrite(const char *path);
int Sys_FileWrite(FILE *handle, void *src, int count);
int Sys_FileRead(FILE *handle, void *dest, int count);
void Sys_FileSeek(FILE *handle, int position);
void Sys_FileClose(FILE *handle);

// system IO
#define Sys_Error(...) do { \
	fprintf(stderr, "Error: %s: ", __func__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	Host_Shutdown(); \
	exit(1); \
} while ( 0 )

void Sys_Printf(const char *fmt, ...); // send text to the console
char *Sys_GetClipboardData(void);
void Sys_CopyToClipboard(char *text);
char *Sys_ConsoleInput(void);

double Sys_DoubleTime (void);
void Sys_Sleep(unsigned long msecs);

void Sys_Quit(void);

#endif /* __SYS_H */
