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

extern int glx, gly, glwidth, glheight;

void SCR_Init (void);

void SCR_UpdateScreen (void);

extern vrect_t scr_vrect;

void SCR_SizeUp (void);
void SCR_SizeDown (void);
void SCR_BringDownConsole (void);
void SCR_CenterPrint (const char *str);

void SCR_BeginLoadingPlaque (void);
void SCR_EndLoadingPlaque (void);

int SCR_ModalMessage (const char *text, float timeout); //johnfitz -- added timeout

extern	float		scr_con_current;
extern	float		scr_conlines;		// lines of console to display

//extern	int			scr_fullupdate;	// set to 0 to force full redraw
extern	int			sb_lines;

extern	int			clearnotify;	// set to 0 whenever notify text is drawn
extern	bool	scr_disabled_for_loading;
extern	bool	scr_skipupdate;

extern	cvar_t		scr_viewsize;
extern  cvar_t		pq_drawfps;

extern	cvar_t		scr_menuscale;
extern	cvar_t		scr_sbarscale;
extern	cvar_t		scr_conwidth;
extern	cvar_t		scr_conscale;
extern	cvar_t		scr_scale;
extern	cvar_t		scr_crosshairscale;

typedef enum {
	CANVAS_NONE,
	CANVAS_DEFAULT,
	CANVAS_CONSOLE,
	CANVAS_MENU,
	CANVAS_SBAR,
	CANVAS_WARPIMAGE,
	CANVAS_CROSSHAIR,
	CANVAS_BOTTOMLEFT,
	CANVAS_BOTTOMRIGHT,
	CANVAS_TOPRIGHT,
	CANVAS_INVALID = -1
} canvastype;

void GL_SetCanvas (canvastype newcanvas);

extern	cvar_t scr_menuscale;
extern	cvar_t scr_sbarscale;
extern	cvar_t scr_sbaralpha;
extern	cvar_t scr_conwidth;
extern	cvar_t scr_conscale;
extern	cvar_t scr_crosshairscale;
extern	cvar_t scr_showfps;
extern	cvar_t scr_clock;
//johnfitz

extern	cvar_t scr_viewsize;
extern	cvar_t scr_fov;
extern	cvar_t scr_fov_adapt;
extern	cvar_t scr_conspeed;
extern	cvar_t scr_centertime;
extern	cvar_t scr_showram;
extern	cvar_t scr_showturtle;
extern	cvar_t scr_showpause;
extern	cvar_t scr_printspeed;
extern	cvar_t gl_triplebuffer;
