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

//
// file IO
//

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
int Sys_FileOpenRead (char *path, int *hndl);

int Sys_FileOpenWrite (char *path);
void Sys_FileClose (int handle);
void Sys_FileSeek (int handle, int position);
int Sys_FileRead (int handle, void *dest, int count);
int Sys_FileWrite (int handle, void *data, int count);
int	Sys_FileTime (char *path);
void Sys_mkdir (char *path);


//
// system IO
//
void Sys_DebugLog(char *file, char *fmt, ...);

void Sys_Error (char *error, ...);
// an error will cause the entire program to exit

void Sys_Printf (char *fmt, ...);
// send text to the console

void Sys_Quit (void);

double Sys_FloatTime (void);

char *Sys_ConsoleInput (void);

void Sys_Sleep (void);

// called to yield for a little bit so as
// not to hog cpu when paused or debugging

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

#ifdef _WIN32
void Sys_InfoInit(void);
char *Sys_GetClipboardData (void);
#endif


void Sys_CopyToClipboard(char *);
void Sys_Init (void);


#ifdef _WIN32
void Sys_GetLock (void);
void Sys_ReleaseLock (void);
#endif

void Sys_SetWindowCaption (char *newcaption);
void Sys_OpenFolder_f (void);
int Sys_GetHardDriveSerial (const char* mydir);
