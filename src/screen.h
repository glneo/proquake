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

#ifndef __SCREEN_H
#define __SCREEN_H

extern vrect_t scr_vrect;

extern float scr_con_current;
extern float scr_conlines; // lines of console to display

extern int sb_lines;
extern int clearnotify; // set to 0 whenever notify text is drawn

extern bool scr_disabled_for_loading;

extern cvar_t scr_menuscale;

extern cvar_t scr_sbarscale;
extern cvar_t scr_sbaralpha;

extern cvar_t scr_conalpha;
extern cvar_t scr_conscale;
extern cvar_t scr_conwidth;
extern cvar_t scr_conspeed;

extern cvar_t crosshair;
extern cvar_t scr_crosshairalpha;
extern cvar_t scr_crosshairscale;
extern cvar_t scr_crosshaircentered;

extern cvar_t scr_fadealpha;

extern cvar_t scr_showfps;
extern cvar_t scr_showram;
extern cvar_t scr_showturtle;
extern cvar_t scr_showpause;
extern cvar_t scr_showclock;

extern cvar_t scr_fov;
extern cvar_t scr_fov_adapt;
extern cvar_t scr_viewsize;
extern cvar_t scr_centertime;
extern cvar_t scr_printspeed;

void SCR_CenterPrint(const char *str);
void SCR_BeginLoadingPlaque(void);
void SCR_EndLoadingPlaque(void);
int SCR_ModalMessage(const char *text, float timeout);
void SRC_DrawTileClear(int x, int y, int w, int h);
void SCR_TileClear(void);
void SCR_ConsoleBackground(void);
void SCR_FadeScreen(void);
void SCR_UpdateScreen(void);
void SCR_Init(void);

#endif	/* __SCREEN_H */
