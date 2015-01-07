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

extern int con_totallines;
extern unsigned int con_backscroll;
extern	bool con_forcedup;	// because no entities to refresh
extern bool con_initialized;
extern byte *con_chars;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Key_Console (int key);
void Char_Console (int key);

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines, bool drawinput);
void Con_Printf (const char *fmt, ...);

void Con_Success (char *fmt, ...); //johnfitz
void Con_Warning (const char *fmt, ...); //johnfitz
void Con_DPrintf (const char *fmt, ...);
void Con_SafePrintf (const char *fmt, ...);
void Con_LogCenterPrint(const char *str);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);

void Con_TabComplete(void);

void Con_Quakebar(int len);

void History_Shutdown(void);
