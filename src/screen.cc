/*
 * Master for refresh, status bar, console, chat, notify, etc
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2007-2008 Kristian Duske
 * Copyright (C) 2010-2014 QuakeSpasm developers
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
#include "glquake.h"

#define NUMCROSSHAIRS 5
qpic_t *scr_crosshairs[NUMCROSSHAIRS];

static byte crosshairdata[NUMCROSSHAIRS][64] =
{
	{
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	}, {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	}, {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	}, {
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	}, {
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},
};

cvar_t scr_menuscale = { "scr_menuscale", "1", CVAR_ARCHIVE };
cvar_t scr_menualpha = { "scr_menualpha", "1", CVAR_ARCHIVE };

cvar_t scr_sbarscale = { "scr_sbarscale", "1", CVAR_ARCHIVE };
cvar_t scr_sbaralpha = { "scr_sbaralpha", "0.75", CVAR_ARCHIVE };

cvar_t scr_conscale = { "scr_conscale", "1", CVAR_ARCHIVE };
cvar_t scr_conalpha = { "scr_conalpha", "0.85", CVAR_ARCHIVE };
cvar_t scr_conwidth = { "scr_conwidth", "0", CVAR_ARCHIVE };
cvar_t scr_conspeed = { "scr_conspeed", "500", CVAR_ARCHIVE };

cvar_t crosshair = { "crosshair", "1", CVAR_ARCHIVE };
cvar_t scr_crosshairalpha = { "scr_crosshairalpha", "1", CVAR_ARCHIVE };
cvar_t scr_crosshairscale = { "scr_crosshairscale", "1", CVAR_ARCHIVE };
cvar_t scr_crosshaircentered = { "scr_crosshaircentered", "1", CVAR_ARCHIVE };

cvar_t scr_fadealpha = { "scr_fadealpha", "0.5", CVAR_ARCHIVE };

cvar_t scr_showfps = { "showfps", "0", CVAR_NONE };
cvar_t scr_showturtle = { "showturtle", "0", CVAR_NONE };
cvar_t scr_showpause = { "showpause", "1", CVAR_NONE };
cvar_t scr_showclock = { "showclock", "0", CVAR_ARCHIVE };
cvar_t scr_devstats = {"showdevstats","0", CVAR_NONE};

cvar_t scr_fov = { "fov", "90", CVAR_NONE }; // 10 - 170
cvar_t scr_fov_adapt = { "fov_adapt", "1", CVAR_ARCHIVE };
cvar_t scr_viewsize = { "viewsize", "100", CVAR_ARCHIVE };
cvar_t scr_centertime = { "scr_centertime", "2", CVAR_NONE };
cvar_t scr_printspeed = { "scr_printspeed", "8", CVAR_NONE };

bool scr_initialized; // ready to draw

float scr_con_current;
float scr_conlines; // lines of console to display

qpic_t *scr_ram;
qpic_t *scr_net;
qpic_t *scr_turtle;
qpic_t *scr_backtile;

int clearconsole;
int clearnotify;

bool scr_disabled_for_loading;
bool scr_drawloading;
float scr_disabled_time;

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
void SCR_CenterPrint(const char *str) //update centerprint data
{
	strncpy(scr_centerstring, str, sizeof(scr_centerstring) - 1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	str = scr_centerstring;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString(void) //actually do the drawing
{
	char *start;
	int l;
	int j;
	int x, y;
	int remaining;

	Draw_SetCanvas (CANVAS_MENU); //johnfitz

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = 200 * 0.35;	//johnfitz -- 320x200 coordinate system
	else
		y = 48;
	if (crosshair.value)
		y -= 8;

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l * 8) / 2; //johnfitz -- 320x200 coordinate system
		for (j = 0; j < l; j++, x += 8)
		{
			Draw_Character(x, y, start[j], 1.0f);
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

void SCR_CheckDrawCenterString(void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;
	if (cl.paused) //johnfitz -- don't show centerprint during a pause
		return;

	SCR_DrawCenterString();
}

//=============================================================================

/*
 ====================
 AdaptFovx
 Adapt a 4:3 horizontal FOV to the current screen size using the "Hor+" scaling:
 2.0 * atan(width / height * 3.0 / 4.0 * tan(fov_x / 2.0))
 ====================
 */
float AdaptFovx(float fov_x, float width, float height)
{
	float a, x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error("Bad fov: %f", fov_x);

	if (!scr_fov_adapt.value)
		return fov_x;
	if ((x = height / width) == 0.75)
		return fov_x;
	a = atan(0.75 / x * tan(fov_x / 360 * M_PI));
	a = a * 360 / M_PI;
	return a;
}

/*
 ====================
 CalcFovy
 ====================
 */
float CalcFovy(float fov_x, float width, float height)
{
	float a, x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error("Bad fov: %f", fov_x);

	x = width / tan(fov_x / 360 * M_PI);
	a = atan(height / x);
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
	float size, scale; //johnfitz -- scale

	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_SetQuick(&scr_viewsize, "30");
	if (scr_viewsize.value > 120)
		Cvar_SetQuick(&scr_viewsize, "120");

	// bound fov
	if (scr_fov.value < 10)
		Cvar_SetQuick(&scr_fov, "10");
	if (scr_fov.value > 170)
		Cvar_SetQuick(&scr_fov, "170");

	size = scr_viewsize.value;
	scale = CLAMP(1.0f, (float)scr_sbarscale.value, (float)vid.width / 320.0f);

	if (size >= 120 || cl.intermission || scr_sbaralpha.value < 1.0f)
		sb_lines = 0;
	else if (size >= 110)
		sb_lines = 24 * scale;
	else
		sb_lines = 48 * scale;

	size = min(scr_viewsize.value, 100.0f) / 100.0f;

	r_refdef.vrect.width = max((unsigned int)(vid.width * size), 96u); //no smaller than 96, for icons
	r_refdef.vrect.height = min((unsigned int)(vid.height * size), (vid.height - sb_lines)); //make room for sbar
	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width) / 2;
	r_refdef.vrect.y = ((vid.height - sb_lines) / 2) - (r_refdef.vrect.height / 2);

//	Con_Printf("x: %d, y: %d, width: %d, height: %d\n", r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height);

	r_refdef.original_fov_x = AdaptFovx(scr_fov.value, r_refdef.vrect.width, r_refdef.vrect.height);
	r_refdef.original_fov_y = CalcFovy(r_refdef.original_fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
}

static void SCR_SizeUp_f(void)
{
	Cvar_SetValueQuick(&scr_viewsize, scr_viewsize.value + 10);
}

static void SCR_SizeDown_f(void)
{
	Cvar_SetValueQuick(&scr_viewsize, scr_viewsize.value - 10);
}

static void SCR_Callback_refdef(cvar_t *var)
{
	vid.recalc_refdef = 1;
}

/*
 ==================
 SCR_Conwidth_f -- johnfitz -- called when scr_conwidth or scr_conscale changes
 ==================
 */
void SCR_Conwidth_f(cvar_t *var)
{
	vid.recalc_refdef = 1;
	vid.conwidth = (scr_conwidth.value > 0) ? (int) scr_conwidth.value : (scr_conscale.value > 0) ? (int) (vid.width / scr_conscale.value) : vid.width;
	vid.conwidth = CLAMP(320u, vid.conwidth, vid.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
}

//============================================================================

void SCR_DrawFPS(void)
{
	static double oldtime = 0;
	static double lastfps = 0;
	static int oldframecount = 0;
	double elapsed_time;
	int frames;

	elapsed_time = realtime - oldtime;
	frames = r_framecount - oldframecount;

	if (elapsed_time < 0 || frames < 0)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		return;
	}
	// update value every 3/4 second
	if (elapsed_time > 0.75)
	{
		lastfps = frames / elapsed_time;
		oldtime = realtime;
		oldframecount = r_framecount;
	}

	if (scr_showfps.value)
	{
		char st[16];
		int x, y;
		sprintf(st, "%4.0f fps", lastfps);
		x = 320 - (strlen(st) << 3);
		y = 200 - 8;
		if (scr_showclock.value)
			y -= 8; //make room for clock
		Draw_SetCanvas (CANVAS_BOTTOMRIGHT);
		Draw_String(x, y, st, 1.0f);
	}
}

void SCR_DrawClock(void)
{
	if (!scr_showclock.value)
		return;

	int minutes, seconds;

	minutes = cl.time / 60;
	seconds = ((int) cl.time) % 60;

	char str[12];
	sprintf(str, "%i:%i%i", minutes, seconds / 10, seconds % 10);

	//draw it
	Draw_SetCanvas (CANVAS_BOTTOMRIGHT);
	Draw_String(320 - (strlen(str) << 3), 200 - 8, str, 1.0f);
}

void SCR_DrawDevStats(void)
{
	char	str[40];
	int		y = 25-9; //9=number of lines to print
	int		x = 0; //margin

	if (!scr_devstats.value)
		return;

	Draw_SetCanvas (CANVAS_BOTTOMLEFT);

	Draw_Fill (x, y*8, 19*8, 9*8, 0, 0.5); //dark rectangle

	sprintf (str, "devstats |Curr Max");
	Draw_String (x, (y++)*8-x, str, 1.0f);

	sprintf (str, "---------+---------");
	Draw_String (x, (y++)*8-x, str, 1.0f);

	sprintf (str, "Packet   |%4i %4i", 0, 0);
	Draw_String (x, (y++)*8-x, str, 1.0f);

	sprintf (str, "Visedicts|%4i %4i", cl_numvisedicts, MAX_VISEDICTS);
	Draw_String (x, (y++)*8-x, str, 1.0f);

	sprintf (str, "Dlights  |%4i %4i", 0, 0);
	Draw_String (x, (y++)*8-x, str, 1.0f);
}

void SCR_DrawCrosshair(void)
{
	Draw_SetCanvas(CANVAS_CROSSHAIR);

	if (crosshair.value >= 2 && (crosshair.value <= NUMCROSSHAIRS + 1))
		Draw_Pic(0, 0, scr_crosshairs[(int) crosshair.value - 2], scr_crosshairalpha.value);
	else if (crosshair.value)
	{
		if (scr_crosshaircentered.value) // Centered crosshair
			Draw_Character(-4, -4, '+', scr_crosshairalpha.value);
		else // Standard off-center Quake crosshair
			Draw_Character(0, 0, '+', scr_crosshairalpha.value);
	}
}

/*
 ==============
 SCR_DrawTurtle
 ==============
 */
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

	Draw_SetCanvas(CANVAS_DEFAULT);

	Draw_Pic(0, 0, scr_turtle, 1.0f);
}

/*
 ==============
 SCR_DrawNet
 ==============
 */
void SCR_DrawNet(void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_SetCanvas(CANVAS_DEFAULT);

	Draw_Pic(64, 0, scr_net, 1.0f);
}

/*
 ==============
 DrawPause
 ==============
 */
void SCR_DrawPause(void)
{
	qpic_t *pic;

	//FIXME: Pause is broken, try it
	if (!cl.paused)
		return;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	Draw_SetCanvas (CANVAS_MENU); //johnfitz

	pic = Draw_CachePic("gfx/pause.lmp");
	Draw_Pic((320 - pic->width) / 2, (240 - 48 - pic->height) / 2, pic, 1.0f); //johnfitz -- stretched menus
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

	Draw_SetCanvas (CANVAS_MENU); //johnfitz

	pic = Draw_CachePic("gfx/loading.lmp");
	Draw_Pic((320 - pic->width) / 2, (240 - 48 - pic->height) / 2, pic, 1.0f); //johnfitz -- stretched menus
}


//=============================================================================

/*
 ==================
 SCR_SetUpToDrawConsole
 ==================
 */
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
		scr_conlines = vid.height; //full screen //johnfitz -- vid.height instead of vid.height
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height / 2; //half screen //johnfitz -- vid.height instead of vid.height
	else
		scr_conlines = 0; //none visible

	timescale = (host_timescale.value > 0) ? (float)host_timescale.value : 1.0f; //johnfitz -- timescale

	if (scr_conlines < scr_con_current)
	{
		// ericw -- (vid.height/600.0) factor makes conspeed resolution independent, using 800x600 as a baseline
		scr_con_current -= scr_conspeed.value * (vid.height / 600.0) * host_frametime / timescale; //johnfitz -- timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;
	}
	else if (scr_conlines > scr_con_current)
	{
		// ericw -- (vid.height/600.0)
		scr_con_current += scr_conspeed.value * (vid.height / 600.0) * host_frametime / timescale; //johnfitz -- timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}
}

static void SCR_DrawConsole(void)
{
	Draw_SetCanvas(CANVAS_CONSOLE);

	if (scr_con_current)
	{
		// draw the background
		SCR_ConsoleBackground();
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

/*
 ==================
 SCR_ScreenShot_f -- johnfitz -- rewritten to use Image_WriteTGA
 ==================
 */
static void SCR_ScreenShot_f(void)
{
	byte *buffer;
	char tganame[16];  //johnfitz -- was [80]
	char checkname[MAX_OSPATH];
	int i;

// find a file name to save it to
	for (i = 0; i < 10000; i++)
	{
		snprintf(tganame, sizeof(tganame), "spasm%04i.tga", i);	// "fitz%04i.tga"
		snprintf(checkname, sizeof(checkname), "%s/%s", com_gamedir, tganame);
		if (!Sys_FileExists(checkname))
			break;	// file doesn't exist
	}
	if (i == 10000)
	{
		Con_Printf("SCR_ScreenShot_f: Couldn't find an unused filename\n");
		return;
	}

//get data
	if (!(buffer = (byte *) malloc(vid.width * vid.height * 3)))
	{
		Con_Printf("SCR_ScreenShot_f: Couldn't allocate memory\n");
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);/* for widths that aren't a multiple of 4 */
	glReadPixels(vid.x, vid.y, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

// now write the file
//	if (Image_WriteTGA(tganame, buffer, vid.width, vid.height, 24, false))
//		Con_Printf("Wrote %s\n", tganame);
//	else
		Con_Printf("SCR_ScreenShot_f: Couldn't create a TGA file\n");

	free(buffer);
}

//=============================================================================

/*
 ===============
 SCR_BeginLoadingPlaque

 ================
 */
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
	SCR_UpdateScreen();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
}

/*
 ===============
 SCR_EndLoadingPlaque

 ================
 */
void SCR_EndLoadingPlaque(void)
{
	scr_disabled_for_loading = false;
	Con_ClearNotify();
}

//=============================================================================

const char *scr_notifystring;
bool scr_drawdialog;

static void SCR_DrawNotifyString(void)
{
	const char *start;
	int l;
	int j;
	int x, y;

	Draw_SetCanvas (CANVAS_MENU); //johnfitz

	start = scr_notifystring;

	y = 200 * 0.35; //johnfitz -- stretched overlays

	do
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l * 8) / 2; //johnfitz -- stretched overlays
		for (j = 0; j < l; j++, x += 8)
			Draw_Character(x, y, start[j], 1.0f);

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
int SCR_ModalMessage (const char *text, float timeout) //johnfitz -- timeout
{
	double time1, time2; //johnfitz -- timeout
	int lastkey, lastchar;

	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;

	S_ClearBuffer ();		// so dma doesn't loop current sound

	time1 = Sys_DoubleTime () + timeout; //johnfitz -- timeout
	time2 = 0.0f; //johnfitz -- timeout

	Key_BeginInputGrab ();
	do
	{
		IN_SendKeyEvents ();
		Key_GetGrabbedInput (&lastkey, &lastchar);
//		Sys_Sleep (16);
		if (timeout) time2 = Sys_DoubleTime (); //johnfitz -- zero timeout means wait forever.
	} while (lastchar != 'y' && lastchar != 'Y' &&
		 lastchar != 'n' && lastchar != 'N' &&
		 lastkey != K_ESCAPE &&
		 lastkey != K_ABUTTON &&
		 lastkey != K_BBUTTON &&
		 time2 <= time1);
	Key_EndInputGrab ();

//	SCR_UpdateScreen (); //johnfitz -- commented out

	//johnfitz -- timeout
	if (time2 > time1)
		return false;
	//johnfitz

	return (lastchar == 'y' || lastchar == 'Y' || lastkey == K_ABUTTON);
}

void SRC_DrawTileClear(int x, int y, int w, int h)
{
	Draw_PicTile(x, y, w, h, scr_backtile, 1.0f);
}

void SCR_TileClear(void)
{
	// left
	if (r_refdef.vrect.x > 0)
		SRC_DrawTileClear(0, 0, r_refdef.vrect.x, vid.height - sb_lines);

	// right
	if ((r_refdef.vrect.x + r_refdef.vrect.width) < vid.width)
		SRC_DrawTileClear(r_refdef.vrect.x + r_refdef.vrect.width, 0, vid.width - (r_refdef.vrect.x + r_refdef.vrect.width), vid.height - sb_lines);

	// top
	if (r_refdef.vrect.y > 0)
		SRC_DrawTileClear(r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y);

	// bottom
	if ((r_refdef.vrect.y + r_refdef.vrect.height) < (vid.height - sb_lines))
		SRC_DrawTileClear(r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height, r_refdef.vrect.width,
				(vid.height - sb_lines) - (r_refdef.vrect.y + r_refdef.vrect.height));
}

void SCR_ConsoleBackground()
{
	qpic_t *pic = Draw_CachePic("gfx/conback.lmp");
	pic->width = vid.conwidth;
	pic->height = vid.conheight;

	float alpha = (con_forcedup) ? 1.0f : (float)scr_conalpha.value;

	Draw_SetCanvas(CANVAS_CONSOLE); //in case this is called from weird places

	Draw_Pic(0, 0, pic, alpha);
}

void SCR_FadeScreen(void)
{
	Draw_SetCanvas(CANVAS_DEFAULT);

	Draw_Fill(0, 0, vid.width, vid.height, 0, scr_fadealpha.value);
}

static void SCR_DrawSbar(void)
{
	Draw_SetCanvas(CANVAS_SBAR);

	Sbar_Draw();
}

static void SCR_DrawMenu(void)
{
	Draw_SetCanvas(CANVAS_MENU);

	M_Draw();
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

	GL_BeginRendering();

	//
	// determine size of refresh window
	//
	if (vid.recalc_refdef)
	{
		SCR_CalcRefdef();
		vid.recalc_refdef = 0;
	}

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole();

	V_RenderView();

	GL_Begin2D();

	SCR_TileClear();

	if (scr_drawdialog) //new game confirm
	{
		if (con_forcedup)
			SCR_ConsoleBackground();
		else
			SCR_DrawSbar();
		SCR_FadeScreen();
		SCR_DrawNotifyString();
	}
	else if (scr_drawloading) //loading
	{
		SCR_DrawLoading();
		SCR_DrawSbar();
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
	else
	{
		SCR_DrawCrosshair();
		SCR_DrawNet();
		SCR_DrawTurtle();
		SCR_DrawPause();
		SCR_CheckDrawCenterString();
		SCR_DrawSbar();
		SCR_DrawDevStats();
		SCR_DrawFPS();
		SCR_DrawClock();
		SCR_DrawConsole();
		SCR_DrawMenu();
	}

	Draw_PolyBlend();

	V_UpdatePalette_Static(false);

	GL_End2D();

	GL_EndRendering();
}

void SCR_Init(void)
{
	Cvar_RegisterVariable(&scr_menuscale);
	Cvar_RegisterVariable(&scr_menualpha);

	Cvar_RegisterVariable(&scr_sbaralpha);
	Cvar_RegisterVariable(&scr_sbarscale);

	Cvar_RegisterVariable(&scr_conscale);
	Cvar_RegisterVariable(&scr_conalpha);
	Cvar_RegisterVariable(&scr_conwidth);
	Cvar_RegisterVariable(&scr_conspeed);

	Cvar_RegisterVariable(&crosshair);
	Cvar_RegisterVariable(&scr_crosshairalpha);
	Cvar_RegisterVariable(&scr_crosshairscale);
	Cvar_RegisterVariable(&scr_crosshaircentered);

	Cvar_RegisterVariable(&scr_fadealpha);

	Cvar_RegisterVariable(&scr_showfps);
	Cvar_RegisterVariable(&scr_showturtle);
	Cvar_RegisterVariable(&scr_showpause);
	Cvar_RegisterVariable(&scr_showclock);
	Cvar_RegisterVariable(&scr_devstats);

	Cvar_RegisterVariable(&scr_fov);
	Cvar_RegisterVariable(&scr_fov_adapt);
	Cvar_RegisterVariable(&scr_viewsize);
	Cvar_RegisterVariable(&scr_centertime);
	Cvar_RegisterVariable(&scr_printspeed);

	Cvar_SetCallback(&scr_sbaralpha, SCR_Callback_refdef);
	Cvar_SetCallback(&scr_conwidth, &SCR_Conwidth_f);
	Cvar_SetCallback(&scr_conscale, &SCR_Conwidth_f);
	Cvar_SetCallback(&scr_fov, SCR_Callback_refdef);
	Cvar_SetCallback(&scr_fov_adapt, SCR_Callback_refdef);
	Cvar_SetCallback(&scr_viewsize, SCR_Callback_refdef);

	Cmd_AddCommand("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand("sizedown", SCR_SizeDown_f);

	GL_Init();
	TexMgr_Init();
	R_Init();
	Draw_Init();

	// load game pics
	scr_net = Draw_PicFromWad("net");
	scr_turtle = Draw_PicFromWad("turtle");
	scr_backtile = Draw_PicFromWad("backtile");
	// Draw_PicFromWad can dirty the scrap texture
	Scrap_Upload();

	// Load the crosshair pics
	for (int i = 0; i < NUMCROSSHAIRS; i++)
	{
		char name[11];
		sprintf(name, "crosshair%i", i);
		scr_crosshairs[i] = Draw_MakePic(name, 8, 8, crosshairdata[i]);
	}

	scr_initialized = true;

	Sbar_Init();
}
