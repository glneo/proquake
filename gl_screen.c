/*
 * Master for refresh, status bar, console, chat, notify, etc
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

/*

 background clear
 rendering
 turtle/net/ram icons
 sbar
 centerprint / slow centerprint
 notify lines
 intermission / finale overlay
 loading plaque
 console
 menu

 required background clears
 required update regions


 syncronous draw mode or async
 One off screen buffer, with updates either copied or xblited
 Need to double buffer?


 async draw will require the refresh area to be cleared, because it will be
 xblited, but sync draw can just ignore it.

 sync
 draw

 CenterPrint ()
 SlowPrint ()
 Screen_Update ();
 Con_Printf ();

 net
 turn off messages option

 the refresh is always rendered, unless the console is full screen


 console is:
 notify lines
 half
 full


 */

int glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged
//int			scr_copytop;
//int			scr_copyeverything;

float scr_con_current;
float scr_conlines;		// lines of console to display

float oldscreensize, oldfov;

cvar_t scr_viewsize = { "viewsize", "100", true };
cvar_t scr_fov = { "fov", "90", true };	// 10 - 170
cvar_t default_fov = { "default_fov", "0", true };	// Default_fov from FuhQuake
cvar_t scr_conspeed = { "scr_conspeed", "99999", true };  // Baker 3.60 - Save to config
cvar_t scr_centertime = { "scr_centertime", "2" };
cvar_t scr_showram = { "showram", "0" };
cvar_t scr_showturtle = { "showturtle", "0" };
cvar_t scr_showpause = { "showpause", "1" };
cvar_t scr_printspeed = { "scr_printspeed", "8" };

cvar_t vid_consize = { "vid_consize", "-1", true }; //Baker 3.97

cvar_t pq_drawfps = { "pq_drawfps", "0", true }; // JPG - draw frames per second
cvar_t show_speed = { "show_speed", "0", false }; // JPG - draw frames per second

bool scr_initialized;		// ready to draw

qpic_t *scr_ram;
qpic_t *scr_net;
qpic_t *scr_turtle;

int clearconsole;
int clearnotify;

viddef_t vid;				// global video state

vrect_t scr_vrect;

bool scr_disabled_for_loading;
bool scr_drawloading;
float scr_disabled_time;
bool scr_skipupdate;

void SCR_ScreenShot_f(void);

/*
 ===============================================================================

 CENTER PRINTING

 ===============================================================================
 */

char scr_centerstring[1024];
float scr_centertime_start;	// for slow victory printing
float scr_centertime_off;
int scr_center_lines;
int scr_erase_lines;
int scr_erase_center;

/*
 ==============
 SCR_CenterPrint

 Called for important messages that should stay in the center of the screen
 for a few moments
 ==============
 */
void SCR_CenterPrint(char *str)
{
	strncpy(scr_centerstring, str, sizeof(scr_centerstring) - 1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString(void)
{
	char *start;
	int l;
	int j;
	int x, y;
	int remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height * 0.35;
	else
		y = 48;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
		{
			Draw_Character(x, y, start[j]);
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

extern cvar_t cl_scoreboard_clean;
extern bool sb_showscores;
void SCR_CheckDrawCenterString(void)
{
//	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	if (sb_showscores && cl_scoreboard_clean.value)
		return;

	SCR_DrawCenterString();
}

//=============================================================================

/*
 ====================
 CalcFov
 ====================
 */
float CalcFov(float fov_x, float width, float height)
{
	float a, x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error("Bad fov: %f", fov_x);

	x = width / tanf(fov_x/360*M_PI);
	a = atanf(height / x);
	a = a * 360 / M_PI;
	return a;
}

/*
 =================
 SCR_CalcRefdef

 Must be called whenever vid changes
 Internal use only
 =================
 */
static void SCR_CalcRefdef(void)
{
	int h;
	float size;
	bool full = false;

//	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

// force the status bar to redraw
	Sbar_Changed();

// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set("viewsize", "30");
	if (scr_viewsize.value > 120)
		Cvar_Set("viewsize", "120");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set("fov", "10");
	if (scr_fov.value > 170)
		Cvar_Set("fov", "170");

// intermission is always full screen
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;		// no status bar at all
	else if (size >= 110)
		sb_lines = 24;		// no inventory
	else
		sb_lines = 24 + 16 + 8;

	if (scr_viewsize.value >= 100.0)
	{
		full = true;
		size = 100.0;
	}
	else
	{
		size = scr_viewsize.value;
	}

	if (cl.intermission)
	{
		full = true;
		size = 100;
		sb_lines = 0;
	}
	size /= 100.0;

	if (cl_sbar.value >= 1.0)
	{
		h = vid.height - sb_lines;
	}
	else
	{
		// LordHavoc: always fullscreen rendering
		h = vid.height;
	}

	r_refdef.vrect.width = (int) (vid.width * size);
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = (int) (vid.height * size);
	if (cl_sbar.value >= 1.0)
	{
		// Baker 3.97: Only if we are displaying the sbar
		if (r_refdef.vrect.height > vid.height - sb_lines)
			r_refdef.vrect.height = vid.height - sb_lines;
	}

	if (r_refdef.vrect.height > (int) vid.height)
		r_refdef.vrect.height = vid.height;
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	//r_refdef.fov_x = scr_fov.value;
	//r_refdef.fov_y = CalcFov (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	if ((glwidth / glheight) > 1.34)
	{
		r_refdef.fov_y = CalcFov(scr_fov.value, r_refdef.vrect.height * (320.0f / 240.0f), r_refdef.vrect.height);
		r_refdef.fov_x = CalcFov(r_refdef.fov_y, vid.height, r_refdef.vrect.width);
	}
	else
	{
		r_refdef.fov_x = scr_fov.value;
		r_refdef.fov_y = CalcFov(r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
	}
	scr_vrect = r_refdef.vrect;
}

/*
 =================
 SCR_SizeUp_f

 Keybinding command
 =================
 */
void SCR_SizeUp_f(void)
{
	Cvar_SetValue("viewsize", scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}

/*
 =================
 SCR_SizeDown_f

 Keybinding command
 =================
 */
void SCR_SizeDown_f(void)
{
	Cvar_SetValue("viewsize", scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
 ==================
 SCR_Init
 ==================
 */
void CL_Default_fov_f(void);
void CL_Fov_f(void);
void SCR_Init(void)
{
	Cvar_RegisterVariable(&default_fov, CL_Default_fov_f);
	Cvar_RegisterVariable(&scr_fov, CL_Fov_f);

	Cvar_RegisterVariable(&scr_viewsize, NULL);
	Cvar_RegisterVariable(&scr_conspeed, NULL);
	Cvar_RegisterVariable(&scr_showram, NULL);
	Cvar_RegisterVariable(&scr_showturtle, NULL);
	Cvar_RegisterVariable(&scr_showpause, NULL);
	Cvar_RegisterVariable(&scr_centertime, NULL);
	Cvar_RegisterVariable(&scr_printspeed, NULL);

	Cvar_RegisterVariable(&pq_drawfps, NULL); // JPG - draw frames per second
	Cvar_RegisterVariable(&show_speed, NULL); // Baker 3.67

	// register our commands
	Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	scr_ram = Draw_PicFromWad("ram");
	scr_net = Draw_PicFromWad("net");
	scr_turtle = Draw_PicFromWad("turtle");

	scr_initialized = true;
}

void SCR_DrawRam(void)
{
	if (!scr_showram.value)
		return;

	Draw_Pic(scr_vrect.x + 32, scr_vrect.y, scr_ram);
}

void SCR_DrawTurtle(void)
{
	static int count;

	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic(scr_vrect.x, scr_vrect.y, scr_turtle);
}

void SCR_DrawNet(void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;

	if (cls.demoplayback)
		return;

	Draw_Pic(scr_vrect.x + 64, scr_vrect.y, scr_net);
}

void SCR_DrawFPS(void)
{
	int x;
	static double last_realtime = 0.0;
	static int last_framecount = 0;
	static int fps = 0;
	char buff[10];
	char *ch;

	if (!pq_drawfps.value)
		return;

	if (realtime - last_realtime > 1.0)
	{
		fps = (host_framecount - last_framecount) / (realtime - last_realtime) + 0.5;
		last_framecount = host_framecount;
		last_realtime = realtime;
	}

	snprintf(buff, sizeof(buff), "%3d", fps);
	x = vid.width - 48;

	ch = buff;

	while (*ch)
	{
		Draw_Character(x, (!show_speed.value ? 8 : 16), *ch);
		x += 8;
		ch++;
	}
}

/*
 ==============
 SCR_DrawSpeed - Baker 3.67 from JoeQuake
 ==============
 */
void SCR_DrawSpeed(void)
{
	int x;
	char buff[10];
	char *ch;
	float speed, vspeed;
	vec3_t vel;
	static float maxspeed = 0, display_speed = -1;
	static double lastrealtime = 0;

	if (!show_speed.value)
		return;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_speed = -1;
		maxspeed = 0;
	}

	VectorCopy(cl.velocity, vel);
	vspeed = vel[2];
	vel[2] = 0;
	speed = VectorLength(vel);

	if (speed > maxspeed)
		maxspeed = speed;

	if (display_speed >= 0)
	{
		snprintf(buff, sizeof(buff), "%3d", (int) display_speed);
		ch = buff;
		x = vid.width - 48;
		while (*ch)
		{
			Draw_Character(x, 8, (*ch) + 128);
			x += 8;
			ch++;
		}
	}

	if (realtime - lastrealtime >= 0.1)
	{
		lastrealtime = realtime;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

/*
 ==============
 DrawPause
 ==============
 */
void SCR_DrawPause(void)
{
	qpic_t *pic;

	if (!cl.paused)
		return;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	pic = Draw_CachePic("gfx/pause.lmp");
	Draw_Pic((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

/*
 ==============
 SCR_DrawLoading
 ==============
 */
void SCR_DrawLoading(void)
{
	qpic_t *pic;

	if (!scr_drawloading)
		return;

	pic = Draw_CachePic("gfx/loading.lmp");
	Draw_Pic((vid.width - pic->width) / 2, (vid.height - 48 - pic->height) / 2, pic);
}

//=============================================================================

void SCR_SetUpToDrawConsole(void)
{
	//johnfitz -- let's hack away the problem of slow console when host_timescale is <0
	extern cvar_t host_timescale;
	float timescale;
	//johnfitz

	Con_CheckResize();

	if (scr_drawloading)
		return;		// never a console with loading plaque

	// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height / 2;	// half screen
	else
		scr_conlines = 0;				// none visible

	timescale = (host_timescale.value > 0) ? host_timescale.value : 1; //johnfitz -- timescale

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value * host_frametime / timescale; //johnfitz -- timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value * host_frametime / timescale; //johnfitz -- timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
	{
		Sbar_Changed();
	}
	else if (clearnotify++ < vid.numpages)
	{
	}
	else
	{
		con_notifylines = 0;
	}
}

/*
 ==================
 SCR_DrawConsole
 ==================
 */
void SCR_DrawConsole(void)
{
	if (scr_con_current)
	{
//		scr_copyeverything = 1;
		Con_DrawConsole(scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify();	// only draw notify in game
	}
}

/*
 ==============================================================================

 SCREEN SHOTS

 ==============================================================================
 */

typedef struct _TargaHeader
{
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

/*
 ==================
 SCR_ScreenShot_f
 ==================
 */
void SCR_ScreenShot_f(void)
{
	byte *buffer;
	char tganame[16];  //johnfitz -- was [80]
	char checkname[MAX_OSPATH];
	int i, c, temp;

	// find a file name to save it to
	for (i = 0; i < 10000; i++)
	{
		snprintf(tganame, sizeof(tganame), "quake%04i.tga", i);
		snprintf(checkname, sizeof(checkname), "%s/%s", com_gamedir, tganame);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	}

	if (i == 10000)
	{
		Con_Printf("Error: Cannot create more than 10000 screenshots\n");
		return;
	}

	buffer = Q_malloc(glwidth * glheight * 3 + 18);
	memset(buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = glwidth & 255;
	buffer[13] = glwidth >> 8;
	buffer[14] = glheight & 255;
	buffer[15] = glheight >> 8;
	buffer[16] = 24;	// pixel size

	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18);

	// swap rgb to bgr
	c = 18 + glwidth * glheight * 3;
	for (i = 18; i < c; i += 3)
	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
	COM_WriteFile(tganame, buffer, glwidth * glheight * 3 + 18);

	free(buffer);
	Con_Printf("Wrote %s\n", tganame);
}

//=============================================================================

void SCR_BeginLoadingPlaque(void)
{
	S_StopAllSounds(true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
//	scr_fullupdate = 0;
	Sbar_Changed();
	SCR_UpdateScreen();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
//	scr_fullupdate = 0;
}

/*
 ===============
 SCR_EndLoadingPlaque

 ================
 */
void SCR_EndLoadingPlaque(void)
{
	scr_disabled_for_loading = false;
//	scr_fullupdate = 0;
	Con_ClearNotify();
}

//=============================================================================

char *scr_notifystring;
bool scr_drawdialog;

void SCR_DrawNotifyString(void)
{
	char *start;
	int l;
	int j;
	int x, y;

	start = scr_notifystring;

	y = (int) (vid.height * 0.35f);

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l * 8) / 2;
		for (j = 0; j < l; j++, x += 8)
			Draw_Character(x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
 ==================
 SCR_ModalMessage

 Displays a text string in the center of the screen and waits for a Y or N
 keypress.
 ==================
 */
// Baker Note: Because this loop gets control a connection to a server can die here
int SCR_ModalMessage(char *text, float timeout) //johnfitz -- timeout
{
	double time1, time2; //johnfitz -- timeout

	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen();

	S_ClearBuffer();		// so dma doesn't loop current sound

	time1 = Sys_DoubleTime() + timeout; //johnfitz -- timeout
	time2 = 0.0f; //johnfitz -- timeout

	do
	{
		key_count = -1;		// wait for a key down and up
		Sys_SendKeyEvents();
		if (timeout)
			time2 = Sys_DoubleTime(); //johnfitz -- zero timeout means wait forever.
	} while (key_lastpress != 'y' && key_lastpress != 'n' && key_lastpress != K_ESCAPE && time2 <= time1);

	scr_drawdialog = false;

	// Baker: gl doesn't use scr_fullupdate, but has it defined
//	scr_fullupdate = 0;

	//johnfitz -- timeout
	if (time2 > time1)
		return false;
	//johnfitz

	return key_lastpress == 'y';
}

//=============================================================================

/*
 ===============
 SCR_BringDownConsole

 Brings the console down and fades the palettes back to normal
 ================
 */
void SCR_BringDownConsole(void)
{
	int i;

	scr_centertime_off = 0;

	for (i = 0; i < 20 && scr_conlines != scr_con_current; i++)
		SCR_UpdateScreen();

	cl.cshifts[0].percent = 0;		// no area contents palette on next frame
//	VID_SetPaletteOld(host_basepal);
}

void SCR_TileClear(void)
{
	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear(0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear(r_refdef.vrect.x + r_refdef.vrect.width, 0, vid.width - r_refdef.vrect.x + r_refdef.vrect.width, vid.height - sb_lines);
	}

	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear(r_refdef.vrect.x, 0, r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
		// bottom
		Draw_TileClear(r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height, r_refdef.vrect.width,
				vid.height - sb_lines - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

/*
 ==================
 SCR_UpdateScreen

 This is called every frame, and can also be called explicitly to flush
 text to the screen.
 ==================
 */
void SCR_UpdateScreen(void)
{
	if (cls.state == ca_dedicated)
		return; // stdout only

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return; // not initialized yet

	GL_BeginRendering(&glx, &gly, &glwidth, &glheight);

	// determine size of refresh window
	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef(); // something changed, so reorder the screen

	// do 3D refresh drawing, and then update the screen
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Set2D();

	// draw any areas not covered by the refresh
	if (cl_sbar.value >= 1.0 || scr_viewsize.value < 100.0)
		SCR_TileClear();

	if (scr_drawdialog) //new game confirm
	{
		Sbar_Draw();
		Draw_FadeScreen();
		SCR_DrawNotifyString();
	}
	else if (scr_drawloading) //loading
	{
		SCR_DrawLoading();
		Sbar_Draw();
		M_Draw(); // drawloading and draw menu can happen now
	}
	else if (cl.intermission == 1 && key_dest == key_game) //end of level
	{
		Sbar_IntermissionOverlay();
	}
	else if (cl.intermission == 2 && key_dest == key_game) //end of episode
	{
		Sbar_FinaleOverlay();
		SCR_CheckDrawCenterString();
	}
	else if (cl.intermission == 3 && key_dest == key_game) //cut-scene
	{
		SCR_CheckDrawCenterString();
	}
	else
	{
		SCR_DrawRam();
		SCR_DrawNet();
		SCR_DrawTurtle();
		SCR_DrawPause();

		if (cls.state == ca_connected)
		{
			Draw_Crosshair();
			SCR_DrawFPS();					// JPG - draw FPS
			SCR_DrawSpeed();				// Baker 3.67 - Drawspeed
			SCR_CheckDrawCenterString();
			SCR_DrawCoords();				// Baker: draw coords if developer 2 or higher
			SCR_DrawVolume();				// Baker 3.60 - JoeQuake 0.15
			Sbar_Draw();
		}

//		if (mod_conhide==false || (key_dest == key_console || key_dest == key_message))
		SCR_DrawConsole();

		M_Draw();
	}

	// R_BrightenScreen2();
	V_UpdatePalette_Static(false);

	GL_EndRendering();
}
