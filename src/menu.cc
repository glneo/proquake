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

#include "quakedef.h"
#include "glquake.h"

m_state_t m_state;

void (*vid_menucmdfn)(void);
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

void M_Menu_Main_f(void);
void M_Menu_SinglePlayer_f(void);
void M_Menu_Load_f(void);
void M_Menu_Save_f(void);
void M_Menu_MultiPlayer_f(void);
void M_Menu_Setup_f(void);
void M_Menu_Options_f(void);
void M_Menu_Keys_f(void);
void M_Menu_Preferences_f(void);
void M_Menu_Video_f(void);
void M_Menu_Help_f(void);
void M_Menu_Quit_f(void);
void M_Menu_LanConfig_f(void);
void M_Menu_GameOptions_f(void);
void M_Menu_Search_f(void);
void M_Menu_ServerList_f(void);

void M_Main_Draw(void);
void M_SinglePlayer_Draw(void);
void M_Load_Draw(void);
void M_Save_Draw(void);
void M_MultiPlayer_Draw(void);
void M_Setup_Draw(void);
void M_Options_Draw(void);
void M_Keys_Draw(void);
//void M_VideoModes_Draw (void);
void M_Video_Draw(void);
void M_Help_Draw(void);
void M_Quit_Draw(void);
void M_LanConfig_Draw(void);
void M_GameOptions_Draw(void);
void M_Search_Draw(void);
void M_ServerList_Draw(void);

bool m_entersound; // play after drawing a frame, so caching won't disrupt the sound

m_state_t m_return_state;
bool m_return_onerror;
char m_return_reason[64];

#define StartingGame (m_multiplayer_cursor == 1)
#define JoiningGame (m_multiplayer_cursor == 0)

void M_ConfigureNetSubsystem(void);

/*
 ================
 M_DrawCharacter

 Draws one solid graphics character
 ================
 */
void M_DrawCharacter(int cx, int line, int num)
{
	Draw_Character(cx, line, num);
}

void M_Print(int cx, int cy, const char *str)
{
	while (*str)
	{
		M_DrawCharacter(cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite(int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter(cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawTransPic(int x, int y, qpic_t *pic)
{
	Draw_TransPic(x, y, pic);
}

void M_DrawPic(int x, int y, qpic_t *pic)
{
	Draw_Pic(x, y, pic, 1.0f);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int j;
	byte *dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy(dest, source, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		memcpy(dest + TOP_RANGE, source + top, 16);
	else
		for (j = 0; j < 16; j++)
			dest[TOP_RANGE + j] = source[top + 15 - j];

	if (bottom < 128)
		memcpy(dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j = 0; j < 16; j++)
			dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
}

void M_DrawTransPicTranslate(int x, int y, qpic_t *pic)
{
//	Draw_TransPicTranslate(x + ((vid.width - 320) >> 1), y, pic, translationTable);
}

void M_DrawTextBox(int x, int y, int width, int lines)
{
	qpic_t *p;
	int cx, cy;
	int n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic("gfx/box_tl.lmp");
	M_DrawTransPic(cx, cy, p);
	p = Draw_CachePic("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic(cx, cy, p);
	}
	p = Draw_CachePic("gfx/box_bl.lmp");
	M_DrawTransPic(cx, cy + 8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic("gfx/box_tm.lmp");
		M_DrawTransPic(cx, cy, p);
		p = Draw_CachePic("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic("gfx/box_mm2.lmp");
			M_DrawTransPic(cx, cy, p);
		}
		p = Draw_CachePic("gfx/box_bm.lmp");
		M_DrawTransPic(cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic("gfx/box_tr.lmp");
	M_DrawTransPic(cx, cy, p);
	p = Draw_CachePic("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic(cx, cy, p);
	}
	p = Draw_CachePic("gfx/box_br.lmp");
	M_DrawTransPic(cx, cy + 8, p);
}

//=============================================================================

int m_save_demonum;

void M_ToggleMenu_f(void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}

	if (key_dest == key_console)
		Con_ToggleConsole_f();
	else
		M_Menu_Main_f();
}

//=============================================================================
/* MAIN MENU */

int m_main_cursor;
#define	MAIN_ITEMS	5

void M_Menu_Main_f(void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

void M_Main_Draw(void)
{
	int f;
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_main.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	M_DrawTransPic(72, 32, Draw_CachePic("gfx/mainmenu.lmp"));

	f = (int)(realtime * 10) % 6;

	M_DrawTransPic(54, 32 + m_main_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}

void M_Main_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
	case K_KP_ENTER:
		m_entersound = true;
		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f();
			break;

		case 1:
			M_Menu_MultiPlayer_f();
			break;

		case 2:
			M_Menu_Options_f();
			break;

		case 3:
			M_Menu_Help_f();
			break;

		case 4:
			M_Menu_Quit_f();
			break;
		}
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

int m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3

void M_Menu_SinglePlayer_f(void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}

void M_SinglePlayer_Draw(void)
{
	int f;
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/ttl_sgl.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	M_DrawTransPic(72, 32, Draw_CachePic("gfx/sp_menu.lmp"));

	f = (int) (realtime * 10) % 6;

	M_DrawTransPic(54, 32 + m_singleplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
}

void M_SinglePlayer_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f();
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				if (!SCR_ModalMessage("Are you sure you want to\nstart a new game?\n", 0.0f))
					break;
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText("disconnect\n");
			Cbuf_AddText("maxplayers 1\n");
			Cbuf_AddText("map start\n");
			break;

		case 1:
			M_Menu_Load_f();
			break;

		case 2:
			M_Menu_Save_f();
			break;
		}
	}
}

//=============================================================================
/* LOAD/SAVE MENU */

int m_load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		12
char m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
int loadable[MAX_SAVEGAMES];

void M_ScanSaves(void)
{
	int i, j;
	char name[MAX_OSPATH];
	FILE *f;
	int version;

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		strcpy(m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		snprintf(name, sizeof(name), "%s/s%i.sav", com_gamedir, i);

		f = fopen(name, "r");
		if (!f)
			continue;
		if (fscanf(f, "%i\n", &version) < 1)
			Con_Printf("ERROR: couldn't read from file.\n");
		if (fscanf(f, "%79s\n", name) < 1)
			Con_Printf("ERROR: couldn't read from file.\n");
		strncpy(m_filenames[i], name, sizeof(m_filenames[i]) - 1);

		// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose(f);
	}
}

void M_Menu_Load_f(void)
{
	m_entersound = true;
	m_state = m_load;
	key_dest = key_menu;
	M_ScanSaves();
}

void M_Menu_Save_f(void)
{
	if (!sv.active)
		return;
	if (cl.intermission)
		return;
	if (svs.maxclients != 1)
		return;
	m_entersound = true;
	m_state = m_save;
	key_dest = key_menu;
	M_ScanSaves();
}

void M_Load_Draw(void)
{
	int i;
	qpic_t *p;

	p = Draw_CachePic("gfx/p_load.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8 * i, m_filenames[i]);

// line cursor
	M_DrawCharacter(8, 32 + m_load_cursor * 8, 12 + ((int) (realtime * 4) & 1));
}

void M_Save_Draw(void)
{
	int i;
	qpic_t *p;

	p = Draw_CachePic("gfx/p_save.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print(16, 32 + 8 * i, m_filenames[i]);

// line cursor
	M_DrawCharacter(8, 32 + m_load_cursor * 8, 12 + ((int) (realtime * 4) & 1));
}

void M_Load_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f();
		break;

	case K_ENTER:
		S_LocalSound("misc/menu2.wav");
		if (!loadable[m_load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;

		// Host_Loadgame_f can't bring up the loading plaque because too much
		// stack space has been used, so do it now
		SCR_BeginLoadingPlaque();

		// issue the load command
		Cbuf_AddText(va("load s%i\n", m_load_cursor));

		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		m_load_cursor--;
		if (m_load_cursor < 0)
			m_load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		m_load_cursor++;
		if (m_load_cursor >= MAX_SAVEGAMES)
			m_load_cursor = 0;
		break;
	}
}

void M_Save_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_SinglePlayer_f();
		break;

	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		Cbuf_AddText(va("save s%i\n", m_load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		m_load_cursor--;
		if (m_load_cursor < 0)
			m_load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		m_load_cursor++;
		if (m_load_cursor >= MAX_SAVEGAMES)
			m_load_cursor = 0;
		break;
	}
}

//=============================================================================
/* MULTIPLAYER MENU */

int m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3

void M_Menu_MultiPlayer_f(void)
{
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}

void M_MultiPlayer_Draw(void)
{
	int f;
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));

	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_DrawTransPic(72, 32, Draw_CachePic("gfx/mp_menu.lmp"));

	f = (int) (realtime * 10) % 6;

	M_DrawTransPic(54, 32 + m_multiplayer_cursor * 20, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));

	if (!tcpipAvailable)
		M_PrintWhite((320 / 2) - ((27 * 8) / 2), 148, "No Communications Available");
}

void M_MultiPlayer_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f();
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			if (tcpipAvailable)
				M_Menu_LanConfig_f();
			break;

		case 1:
			if (tcpipAvailable)
				M_Menu_LanConfig_f();
			break;

		case 2:
			M_Menu_Setup_f();
			break;
		}
	}
}

//=============================================================================
/* SETUP MENU */

int setup_cursor = 5;
int setup_cursor_table[] = { 40, 56, 80, 104, 128, 152 };
char setup_hostname[16], setup_myname[16];
int setup_oldtop, setup_oldbottom, setup_top, setup_bottom;

#define	NUM_SETUP_CMDS	6

void M_Menu_Setup_f(void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;

	strlcpy(setup_hostname, hostname.string, sizeof(setup_hostname));

	if (!(strlen(setup_myname)))
		strlcpy(setup_myname, cl_name.string, sizeof(setup_myname)); //R00k

	setup_top = setup_oldtop = ((int) cl_color.value) >> 4;
	setup_bottom = setup_oldbottom = ((int) cl_color.value) & 15;
}

void M_Setup_Draw(void)
{
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_Print(64, 40, "Hostname");
	M_DrawTextBox(160, 32, 16, 1);
	M_Print(168, 40, setup_hostname);

	M_Print(64, 56, "Your name");
	M_DrawTextBox(160, 48, 16, 1);
	M_PrintWhite(168, 56, setup_myname); // Baker 3.83: Draw it correctly!

	M_Print(64, 80, "Name Maker");

	M_Print(64, 104, "Shirt color");
	M_Print(64, 128, "Pants color");

	M_DrawTextBox(64, 152 - 8, 14, 1);
	M_Print(72, 152, "Accept Changes");

	//p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTextBox(160, 64, 8, 8);
	p = Draw_CachePic("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top * 16, setup_bottom * 16);
	M_DrawTransPicTranslate(176, 76, p);

	M_DrawCharacter(56, setup_cursor_table[setup_cursor], 12 + ((int) (realtime * 4) & 1));

	if (setup_cursor == 0)
		M_DrawCharacter(168 + 8 * strlen(setup_hostname), setup_cursor_table[setup_cursor],
				10 + ((int) (realtime * 4) & 1));

	if (setup_cursor == 1)
		M_DrawCharacter(168 + 8 * strlen(setup_myname), setup_cursor_table[setup_cursor],
				10 + ((int) (realtime * 4) & 1));
}

void M_Setup_Key(int key, int ascii)
{
	int l;

	switch (key)
	{
	case K_ESCAPE:
		strlcpy(setup_myname, cl_name.string, sizeof(setup_myname)); //R00k
		M_Menu_MultiPlayer_f();
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound("misc/menu1.wav");
		setup_cursor = 0;
		break;

	case K_END:
		S_LocalSound("misc/menu1.wav");
		setup_cursor = NUM_SETUP_CMDS - 1;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound("misc/menu3.wav");
		if (setup_cursor == 3)
			setup_top = setup_top - 1;
		if (setup_cursor == 4)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
		forward: S_LocalSound("misc/menu3.wav");
		if (setup_cursor == 3)
			setup_top = setup_top + 1;
		if (setup_cursor == 4)
			setup_bottom = setup_bottom + 1;
		break;

	case K_SPACE:
	case K_ENTER:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 3 || setup_cursor == 4)
			goto forward;

		if (setup_cursor == 2)
		{
			m_entersound = true;
			break;
		}

		// setup_cursor == 5 (OK)
//		if (strcmp(cl_name.string, setup_myname) != 0)
		Cbuf_AddText(va("name \"%s\"\n", setup_myname));
//		if (strcmp(hostname.string, setup_hostname) != 0)
		Cvar_SetQuick(&hostname, setup_hostname);
//		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
		Cbuf_AddText(va("color %i %i\n", setup_top, setup_bottom));
		m_entersound = true;
		M_Menu_MultiPlayer_f();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname) - 1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname) - 1] = 0;
		}
		break;

	default:
		if (ascii < 32 || ascii > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l + 1] = 0;
				setup_hostname[l] = ascii;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l + 1] = 0;
				setup_myname[l] = ascii;
			}
		}
	}

	if (setup_top > 15)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 15;
	if (setup_bottom > 15)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 15;
}

//=============================================================================
/* OPTIONS MENU */

enum
{
	OPT_CUSTOMIZE = 0,
	OPT_CONSOLE,
	OPT_DEFAULTS,
	OPT_SCALE,
	OPT_SCRSIZE,
	OPT_GAMMA,
	OPT_CONTRAST,
	OPT_MOUSESPEED,
	OPT_SBALPHA,
	OPT_SNDVOL,
	OPT_MUSICVOL,
	OPT_MUSICEXT,
	OPT_ALWAYRUN,
	OPT_INVMOUSE,
	OPT_ALWAYSMLOOK,
	OPT_LOOKSPRING,
	OPT_LOOKSTRAFE,
	OPT_VIDEO,
	OPTIONS_ITEMS
};

#define SLIDER_RANGE 10

int options_cursor;

void M_Menu_Options_f (void)
{
//	IN_Deactivate(modestate == MODE_WINDOWED);
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}


void M_AdjustSliders (int dir)
{
	float f, l;

	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case OPT_SCALE:	// console and menu scale
		l = ((vid.width + 31) / 32) / 10.0;
		f = scr_conscale.value + dir * .1;
		if (f < 1)	f = 1;
		else if(f > l)	f = l;
		Cvar_SetValue ("scr_conscale", f);
		Cvar_SetValue ("scr_menuscale", f);
		Cvar_SetValue ("scr_sbarscale", f);
		break;
	case OPT_SCRSIZE:	// screen size
		f = scr_viewsize.value + dir * 10;
		if (f > 120)	f = 120;
		else if(f < 30)	f = 30;
		Cvar_SetValue ("viewsize", f);
		break;
	case OPT_GAMMA:	// gamma
		f = vid_gamma.value - dir * 0.05;
		if (f < 0.5)	f = 0.5;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("gamma", f);
		break;
	case OPT_CONTRAST:	// contrast
		f = vid_contrast.value + dir * 0.1;
		if (f < 1)	f = 1;
		else if (f > 2)	f = 2;
		Cvar_SetValue ("contrast", f);
		break;
	case OPT_MOUSESPEED:	// mouse speed
		f = sensitivity.value + dir * 0.5;
		if (f > 11)	f = 11;
		else if (f < 1)	f = 1;
		Cvar_SetValue ("sensitivity", f);
		break;
	case OPT_SBALPHA:	// statusbar alpha
		f = scr_sbaralpha.value - dir * 0.05;
		if (f < 0)	f = 0;
		else if (f > 1)	f = 1;
		Cvar_SetValueQuick(&scr_sbaralpha, f);
		break;
	case OPT_MUSICVOL:	// music volume
		f = bgmvolume.value + dir * 0.1;
		if (f < 0)	f = 0;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("bgmvolume", f);
		break;
	case OPT_MUSICEXT:	// enable external music vs cdaudio
//		Cvar_Set ("bgm_extmusic", bgm_extmusic.value ? "0" : "1");
		break;
	case OPT_SNDVOL:	// sfx volume
		f = sfxvolume.value + dir * 0.1;
		if (f < 0)	f = 0;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("volume", f);
		break;

	case OPT_ALWAYRUN:	// always run
		if (cl_movespeedkey.value <= 1)
			Cvar_Set ("cl_movespeedkey", "2.0");
		if (cl_forwardspeed.value > 200)
		{
			Cvar_Set ("cl_forwardspeed", "200");
			Cvar_Set ("cl_backspeed", "200");
		}
		else
		{
			Cvar_SetValue ("cl_forwardspeed", 200 * cl_movespeedkey.value);
			Cvar_SetValue ("cl_backspeed", 200 * cl_movespeedkey.value);
		}
		break;

	case OPT_INVMOUSE:	// invert mouse
		Cvar_SetValue ("m_pitch", -m_pitch.value);
		break;

	case OPT_ALWAYSMLOOK:
		if (in_mlook.state & 1)
			Cbuf_AddText("-mlook");
		else
			Cbuf_AddText("+mlook");
		break;

	case OPT_LOOKSPRING:	// lookspring
		Cvar_Set ("lookspring", lookspring.value ? "0" : "1");
		break;

	case OPT_LOOKSTRAFE:	// lookstrafe
		Cvar_Set ("lookstrafe", lookstrafe.value ? "0" : "1");
		break;
	}
}


void M_DrawSlider (int x, int y, float range)
{
	int	i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter (x-8, y, 128);
	for (i = 0; i < SLIDER_RANGE; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
	if (on)
		M_DrawCharacter (x, y, 131);
	else
		M_DrawCharacter (x, y, 129);
#endif
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	float r = 0, l;
	qpic_t *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// Draw the items in the order of the enum defined above:
	// OPT_CUSTOMIZE:
	M_Print (16, 32,			"              Controls");
	// OPT_CONSOLE:
	M_Print (16, 32 + 8*OPT_CONSOLE,	"          Goto console");
	// OPT_DEFAULTS:
	M_Print (16, 32 + 8*OPT_DEFAULTS,	"          Reset config");

	// OPT_SCALE:
	M_Print (16, 32 + 8*OPT_SCALE,		"                 Scale");
	l = (vid.width / 320.0) - 1;
	r = l > 0 ? (scr_conscale.value - 1) / l : 0;
	M_DrawSlider (220, 32 + 8*OPT_SCALE, r);

	// OPT_SCRSIZE:
	M_Print (16, 32 + 8*OPT_SCRSIZE,	"           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 32 + 8*OPT_SCRSIZE, r);

	// OPT_GAMMA:
	M_Print (16, 32 + 8*OPT_GAMMA,		"            Brightness");
	r = (1.0 - vid_gamma.value) / 0.5;
	M_DrawSlider (220, 32 + 8*OPT_GAMMA, r);

	// OPT_CONTRAST:
	M_Print (16, 32 + 8*OPT_CONTRAST,	"              Contrast");
	r = vid_contrast.value - 1.0;
	M_DrawSlider (220, 32 + 8*OPT_CONTRAST, r);

	// OPT_MOUSESPEED:
	M_Print (16, 32 + 8*OPT_MOUSESPEED,	"           Mouse Speed");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (220, 32 + 8*OPT_MOUSESPEED, r);

	// OPT_SBALPHA:
	M_Print (16, 32 + 8*OPT_SBALPHA,	"       Statusbar alpha");
	r = (1.0 - scr_sbaralpha.value) ; // scr_sbaralpha range is 1.0 to 0.0
	M_DrawSlider (220, 32 + 8*OPT_SBALPHA, r);

	// OPT_SNDVOL:
	M_Print (16, 32 + 8*OPT_SNDVOL,		"          Sound Volume");
	r = sfxvolume.value;
	M_DrawSlider (220, 32 + 8*OPT_SNDVOL, r);

	// OPT_MUSICVOL:
	M_Print (16, 32 + 8*OPT_MUSICVOL,	"          Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 32 + 8*OPT_MUSICVOL, r);

	// OPT_MUSICEXT:
	M_Print (16, 32 + 8*OPT_MUSICEXT,	"        External Music");
//	M_DrawCheckbox (220, 32 + 8*OPT_MUSICEXT, bgm_extmusic.value);

	// OPT_ALWAYRUN:
	M_Print (16, 32 + 8*OPT_ALWAYRUN,	"            Always Run");
	M_DrawCheckbox (220, 32 + 8*OPT_ALWAYRUN, cl_forwardspeed.value > 200);

	// OPT_INVMOUSE:
	M_Print (16, 32 + 8*OPT_INVMOUSE,	"          Invert Mouse");
	M_DrawCheckbox (220, 32 + 8*OPT_INVMOUSE, m_pitch.value < 0);

	// OPT_ALWAYSMLOOK:
	M_Print (16, 32 + 8*OPT_ALWAYSMLOOK,	"            Mouse Look");
	M_DrawCheckbox (220, 32 + 8*OPT_ALWAYSMLOOK, in_mlook.state & 1);

	// OPT_LOOKSPRING:
	M_Print (16, 32 + 8*OPT_LOOKSPRING,	"            Lookspring");
	M_DrawCheckbox (220, 32 + 8*OPT_LOOKSPRING, lookspring.value);

	// OPT_LOOKSTRAFE:
	M_Print (16, 32 + 8*OPT_LOOKSTRAFE,	"            Lookstrafe");
	M_DrawCheckbox (220, 32 + 8*OPT_LOOKSTRAFE, lookstrafe.value);

	// OPT_VIDEO:
	if (vid_menudrawfn)
		M_Print (16, 32 + 8*OPT_VIDEO,	"         Video Options");

// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Key (int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
//	case K_BBUTTON:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
//	case K_KP_ENTER:
//	case K_ABUTTON:
		m_entersound = true;
		switch (options_cursor)
		{
		case OPT_CUSTOMIZE:
			M_Menu_Keys_f ();
			break;
		case OPT_CONSOLE:
			m_state = m_none;
			Con_ToggleConsole_f ();
			break;
		case OPT_DEFAULTS:
			if (SCR_ModalMessage("This will reset all controls\n"
					"and stored cvars. Continue? (y/n)\n", 15.0f))
			{
				Cbuf_AddText ("resetcfg\n");
				Cbuf_AddText ("exec default.cfg\n");
			}
			break;
		case OPT_VIDEO:
			M_Menu_Video_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}

	if (options_cursor == OPTIONS_ITEMS - 1 && vid_menudrawfn == NULL)
	{
		if (key == K_UPARROW)
			options_cursor = OPTIONS_ITEMS - 2;
		else
			options_cursor = 0;
	}
}


//=============================================================================
/* KEYS MENU */

// Baker 3.60 - more sensible customize controls, same options just organized better
char *bindnames[][2] = {
		{ "+attack", "attack" },
		{ "+jump", "jump" },
		{ "+forward", "move forward" },
		{ "+back", "move back" },
		{ "+moveleft", "move left" },
		{ "+moveright", "move right" },
		{ "+moveup", "swim up" },
		{ "+movedown", "swim down" },
		{ "impulse 10", "change weapon" },
		{ "+speed", "run" },
		{ "+left", "turn left" },
		{ "+right", "turn right" },
		{ "+lookup", "look up" },
		{ "+lookdown", "look down" },
		{ "+mlook", "mouse look" },
		{ "+klook", "keyboard look" },
		{ "+strafe", "sidestep" },
		{ "centerview", "center view" }
};

/* Baker 3.60 -- Old menu
 {
 {"+attack", 		"attack"},
 {"impulse 10", 		"change weapon"},
 {"+jump", 			"jump / swim up"},
 {"+forward", 		"walk forward"},
 {"+back", 			"backpedal"},
 {"+left", 			"turn left"},
 {"+right", 			"turn right"},
 {"+speed", 			"run"},
 {"+moveleft", 		"step left"},
 {"+moveright", 		"step right"},
 {"+strafe", 		"sidestep"},
 {"+lookup", 		"look up"},
 {"+lookdown", 		"look down"},
 {"centerview", 		"center view"},
 {"+mlook", 			"mouse look"},
 {"+klook", 			"keyboard look"},
 {"+moveup",			"swim up"},
 {"+movedown",		"swim down"}
 }; */

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int keys_cursor;
int bind_grab;

void M_Menu_Keys_f(void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}

void M_FindKeysForCommand(char *command, int *twokeys)
{
	int count;
	int j;
	int l;
	char *b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp(b, command, l))
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand(char *command)
{
	int j;
	int l;
	char *b;

	l = strlen(command);

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp(b, command, l))
			Key_SetBinding(j, "");
	}
}

void M_Keys_Draw(void)
{
	int i;
//	int l;
	int keys[2];
	char *name;
	int x, y;
	qpic_t *p;

	p = Draw_CachePic("gfx/ttl_cstm.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	if (bind_grab)
		M_Print(12, 32, "Press a key or button for this action");
	else
		M_Print(18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i = 0; i < NUMCOMMANDS; i++)
	{
		y = 48 + 8 * i;

		M_Print(16, y, bindnames[i][1]);

//		l = strlen(bindnames[i][0]);

		M_FindKeysForCommand(bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print(140, y, "???");
		}
		else
		{
			name = Key_KeynumToString(keys[0]);
			M_Print(140, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print(140 + x + 8, y, "or");
				M_Print(140 + x + 32, y, Key_KeynumToString(keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter(130, 48 + keys_cursor * 8, '=');
	else
		M_DrawCharacter(130, 48 + keys_cursor * 8, 12 + ((int) (realtime * 4) & 1));
}

void M_Keys_Key(int key, int ascii)
{
	char cmd[80];
	int keys[2];

	if (bind_grab)
	{	// defining a key
		S_LocalSound("misc/menu1.wav");
		if (key == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (ascii != '`')
		{
			//Con_Printf("Trying to bind %d",key);
			//Con_Printf("name is  %s",Key_KeynumToString (key));
			snprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(key),
					bindnames[keys_cursor][0]);
			Cbuf_InsertText(cmd);
		}

		bind_grab = false;
		return;
	}

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Options_f();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand(bindnames[keys_cursor][0], keys);
		S_LocalSound("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand(bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound("misc/menu2.wav");
		M_UnbindCommand(bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* ADVANCED SETTINGS MENU */

// Baker 3.60 - Added Advanced Settings Menu
#define	PREFERENCES_ITEMS	20 // Baker 3.60 - New Menu
//#define	PREF_SLIDER_RANGE	10 // Baker 3.60 - Needed for pq_maxfps ???

int preferences_cursor = 2;

void M_Menu_Preferences_f(void)
{
	key_dest = key_menu;
	m_state = m_preferences;  // Baker 3.60 - we are in the preferences menu
	m_entersound = true;
}

void M_Pref_AdjustSliders(int dir)
{
	int newval;

	S_LocalSound("misc/menu3.wav");

	// Baker 3.61 - Made menu bi-directional
	switch (preferences_cursor)
	{
	case 2:	// crosshair  off | on | centered

		if (!crosshair.value)
			newval = (dir < 0) ? 3 : 2;
		else if (!scr_crosshaircentered.value)
			newval = (dir < 0) ? 1 : 3;
		else
			newval = (dir < 0) ? 2 : 1;

		switch (newval)
		{
		case 1:
			Cvar_SetQuick(&crosshair, "0");
			Cvar_SetQuick(&scr_crosshaircentered, "0");
			break;

		case 2:
			Cvar_SetQuick(&crosshair, "1");
			Cvar_SetQuick(&scr_crosshaircentered, "0");
			break;

		case 3:
			Cvar_SetQuick(&crosshair, "1");
			Cvar_SetQuick(&scr_crosshaircentered, "1");
			break;
		}

		break;

	case 3:	// draw weapon  on | off | always

//#ifdef SUPPORTS_ENTITY_ALPHA
		if (!r_drawviewmodel.value)
			newval = (dir < 0) ? 1 : 3;
		else if (r_ringalpha.value < 1)
			newval = (dir < 0) ? 2 : 1;
		else
			newval = (dir < 0) ? 3 : 2;

		switch (newval)
		{
		case 1:
			Cvar_SetQuick(&r_ringalpha, "1");
			Cvar_SetQuick(&r_drawviewmodel, "1");
			break;

		case 2:
			Cvar_SetQuick(&r_ringalpha, "1");
			Cvar_SetQuick(&r_drawviewmodel, "0");
			break;

		case 3:
			Cvar_SetQuick(&r_drawviewmodel, "1");
			Cvar_SetQuick(&r_ringalpha, "0.4");
			break;
		}

		break;

	case 4:

		Cvar_SetQuick(&r_truegunangle, r_truegunangle.value ? "0" : "1");
		break;

	case 5:

		// FULL | LITE | ONLY DAMAGE | ALL OFF
		if (pq_suitblend.value >= 1)
			newval = 2;
		else
			newval = 1;

		switch (newval)
		{
		case 1:
			// Must be off, set to Quakedefaults
			Cvar_SetQuick(&r_waterwarp, "1");
			Cvar_SetQuick(&pq_ringblend, "1");
			Cvar_SetQuick(&pq_quadblend, "1");
			Cvar_SetQuick(&pq_pentblend, "1");
			Cvar_SetQuick(&pq_suitblend, "1");
			Cvar_SetQuick(&pq_waterblend, "1");
			break;

		case 2:
			// FULL to LITE
			Cvar_SetQuick(&r_waterwarp, "0");
			Cvar_SetQuick(&pq_ringblend, "0");
			Cvar_SetQuick(&pq_quadblend, "0.3");
			Cvar_SetQuick(&pq_pentblend, "0.3");
			Cvar_SetQuick(&pq_suitblend, "0.3");
			Cvar_SetQuick(&pq_waterblend, "0.3");
			break;
		}

		break;

	case 6:

		if (!cl_rollangle.value)  // Using most obscure value to avoid confusing someone who sets some of this
		{
			// Ok, we will assume it's all off so we turn everything on
			Cvar_SetQuick(&v_kickpitch, "0.6");
			Cvar_SetQuick(&v_kickroll, "0.6");
			Cvar_SetQuick(&v_kicktime, "0.5");
			Cvar_SetQuick(&cl_bob, "0.02");
			Cvar_SetQuick(&cl_bobcycle, "0.6");
			Cvar_SetQuick(&cl_bobup, "0.5");
			Cvar_SetQuick(&cl_rollangle, "2");
		}
		else
		{
			Cvar_SetQuick(&v_kickpitch, "0");
			Cvar_SetQuick(&v_kickroll, "0");
			Cvar_SetQuick(&v_kicktime, "0");
			Cvar_SetQuick(&cl_bob, "0");
			Cvar_SetQuick(&cl_bobcycle, "0");
			Cvar_SetQuick(&cl_bobup, "0");
			Cvar_SetQuick(&cl_rollangle, "0");
		}

		break;

	case 10:

		Cvar_SetQuick(&cl_keypad, cl_keypad.value ? "0" : "1");
		break;

		/*		case 11:

		 Cvar_SetQuick(&scr_conspeed, (scr_conspeed.value >= 5000) ? "300" : "9999");
		 break; */

	case 11:

		Cvar_SetQuick(&pq_moveup, pq_moveup.value ? "0" : "1");
		break;

	case 12:

		Cvar_SetQuick(&ambient_level, ambient_level.value ? "0" : "0.3");
		break;

	case 13:

#ifdef SUPPORTS_VSYNC

		if (vid_vsync.value)
		Cvar_SetQuick(&vid_vsync, "0");
		else
		Cvar_SetQuick(&vid_vsync, "1");

#endif
		break;

	case 14:

		// 72 ON | 120 OFF | 200 OFF | 250 OFF
		if (pq_maxfps.value <= 72)
			newval = (dir < 0) ? 4 : 2;
		else if (pq_maxfps.value <= 120)
			newval = (dir < 0) ? 1 : 3;
		else if (pq_maxfps.value <= 200)
			newval = (dir < 0) ? 2 : 4;
		else
			newval = (dir < 0) ? 3 : 1;

		switch (newval)
		{
		case 1:
			Cvar_SetQuick(&pq_maxfps, "72");
			break;

		case 2:
			Cvar_SetQuick(&pq_maxfps, "120");
			break;

		case 3:
			Cvar_SetQuick(&pq_maxfps, "200");
			break;

		case 4:
			Cvar_SetQuick(&pq_maxfps, "250");
			break;
		}

		break;

	case 15:

		Cvar_SetQuick(&scr_showfps, scr_showfps.value ? "0" : "1");
		break;

	case 16:

		// 72 ON | 120 OFF | 200 OFF | 250 OFF
//		newval = CLAMP(-2, vid_consize.value + (dir < 0 ? -1 : 1), 3);
//		if (newval == 3) // We won't allow 320 selection except from console
//			newval = -1;
//		else if (newval == -2)
//			newval = 2;
//
//		Cvar_SetValueQuick(&vid_consize, newval);
		break;

	case 18:

		break;

	case 19:

#ifdef SUPPORTS_INTERNATIONAL_KEYBOARD
		Cvar_SetQuick(&in_keymap, in_keymap.value ? "0" : "1");
#endif
		break;

	}
}

void M_Pref_Options_Draw(void)
{
	int i = 32;
	char *title;
	qpic_t *p;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_option.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	title = "view setup";
	M_PrintWhite((320 - 8 * strlen(title)) / 2, i, title);
	i += 8;								  // 0
	i += 8;
	M_Print(16, i, "     crosshair      ");
	M_Print(220, i, crosshair.value ? (scr_crosshaircentered.value ? "centered" : "on") : "off");
	i += 8; 	  // 2
//#ifdef SUPPORTS_ENTITY_ALPHA
	M_Print(16, i, "     draw weapon    ");
	M_Print(220, i, r_drawviewmodel.value ? (r_ringalpha.value < 1 ? "always" : "on") : "off");
	i += 8;     // 3

	M_Print(16, i, "     weapon style   ");
	M_Print(220, i, r_truegunangle.value ? "darkplaces" : "classic");
	i += 8; 	  // 4
	M_Print(16, i, "     view blends    ");
	M_Print(220, i, pq_suitblend.value >= 1 ? "classic" : "deathmatch");
	i += 8; 	  // 5
	M_Print(16, i, "     disable bobbing");
	M_Print(220, i, cl_rollangle.value ? "off" : "on");
	i += 8; 	  // 6
	i += 8;													// 7
	title = "interface";
	M_PrintWhite((320 - 8 * strlen(title)) / 2, i, title);
	i += 8;							  // 8
	i += 8;													// 9
	M_Print(16, i, "     keypad binding ");
	M_Print(220, i, cl_keypad.value ? "on" : "off");
	i += 8; 	  // 10
	M_Print(16, i, "     jump is moveup ");
	M_Print(220, i, pq_moveup.value ? "on" : "off");
	i += 8; 	  // 11
	M_Print(16, i, "     ambient sound  ");
	M_Print(220, i, ambient_level.value ? "on" : "off");
	i += 8; 	  // 12
#ifdef SUPPORTS_VSYNC
			M_Print (16, i, "     vsync          "); M_Print (220, i, vid_vsync.value ? "on" : "off" ); i += 8; // 13
#else
	M_Print(16, i, "     vsync          ");
	M_Print(220, i, "[driver set]");
	i += 8; 	  // 13
#endif
	M_Print(16, i, "     max frames/sec ");
	M_Print(220, i,
			pq_maxfps.value == 72 ?
					"72 fps" :
					(pq_maxfps.value == 120 ?
							"120 fps" :
							(pq_maxfps.value == 200 ?
									"200 fps" :
									(pq_maxfps.value == 250 ? "250 fps" : "custom"))));
	i += 8; 	  // 14
	M_Print(16, i, "     show framerate ");
	M_Print(220, i, scr_showfps.value ? "on" : "off");
	i += 8;	  // 14
	M_Print(16, i, "     console width  ");
//	M_Print(220, i,
//			vid_consize.value == 0 ?
//					"100%" :
//					(vid_consize.value == 1 ?
//							"50%" :
//							(vid_consize.value == 2 ?
//									"640 width" :
//									(vid_consize.value == -1 ? "auto" : "custom"))));
	i += 16; 	  // 15
	M_Print(16, i, "     directinput mouse ");


	M_Print(220, i, "n/a");

	i += 8;

#ifdef SUPPORTS_INTERNATIONAL_KEYBOARD
	M_Print (16, i, "     keyboard automap"); M_Print (220, i, Key_InternationalON() ? "on" : "off" );
#else
	M_Print(16, i, "     keyboard automap");
	M_Print(220, i, "n/a");
#endif
	M_DrawCharacter(200, 32 + preferences_cursor * 8, 12 + ((int) (realtime * 4) & 1));
}

void M_Pref_Options_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Options_f();
		break;

	case K_ENTER:
		m_entersound = true;
		switch (preferences_cursor)
		{
		default:
			M_Pref_AdjustSliders(1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		preferences_cursor--;
		if (preferences_cursor < 0)
			preferences_cursor = PREFERENCES_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		preferences_cursor++;
		if (preferences_cursor >= PREFERENCES_ITEMS)
			preferences_cursor = 0;
		break;

	case K_LEFTARROW:
		M_Pref_AdjustSliders(-1);
		break;

	case K_RIGHTARROW:
		M_Pref_AdjustSliders(1);
		break;
	}

	if (preferences_cursor < 2) // the gray zone part 1!
	{
		if (key == K_UPARROW)
			preferences_cursor = PREFERENCES_ITEMS - 1;
		else
			preferences_cursor = 2;
	}

	if (preferences_cursor >= 7 && preferences_cursor <= 9)
	{
		if (key == K_UPARROW)
			preferences_cursor = 6;
		else
			preferences_cursor = 10;
	}

	if (preferences_cursor == 17)
	{
		if (key == K_UPARROW)
			preferences_cursor = 16;
		else
			preferences_cursor = 18;
	}

}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f(void)
{
//#ifdef SUPPORTS_GLVIDEO_MODESWITCH
	(*vid_menucmdfn)(); //johnfitz
//#else
//	key_dest = key_menu;
//	m_state = m_video;
//	m_entersound = true;
//#endif
}

void M_Video_Draw(void)
{
	(*vid_menudrawfn)();
}

void M_Video_Key(int key, int ascii)
{
	(*vid_menukeyfn)(key);
}

//=============================================================================
/* HELP MENU */

int help_page;
#define	NUM_HELP_PAGES	7  // JPG - was 6 (added ProQuake page)

void M_Menu_Help_f(void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}

// JPG - added ProQuake page
void M_Help_Draw(void)
{
	int f;

	if (help_page)
		M_DrawPic(0, 0, Draw_CachePic(va("gfx/help%i.lmp", help_page - 1)));
	else
	{
		M_DrawTextBox(16, 16, 34, 16);
		M_PrintWhite(32, 48, va("     %s version %s", ENGINE_NAME, ENGINE_VERSION));
		// Baker: fixme this isn't going to line up properly ^^^^^
		M_Print(32, 72, "          New Updates By Baker");
		M_PrintWhite(32, 80, "       http://www.quakeone.com");

		M_Print(32, 96, "   Programmed by J.P. Grossman    ");
		M_PrintWhite(36, 112, "                jpg@ai.mit.edu    ");
		M_Print(28, 120, " http://planetquake.com/proquake  ");
		M_Print(32, 136, "<-- Previous            Next -->");

		f = (int) (realtime * 8) % 6;
		M_DrawTransPic(48, 40, Draw_CachePic(va("gfx/menudot%i.lmp", f + 1)));
		M_DrawTransPic(248, 40, Draw_CachePic(va("gfx/menudot%i.lmp", f ? 7 - f : 1)));
	}
}

void M_Help_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES - 1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int msgNumber;
m_state_t m_quit_prevstate;
bool wasInMenus;

char *quitMessage[] = {
	"  Are you gonna quit    ", "  this game just like   ", "   everything else?     ", "                        ",
	" Milord, methinks that  ", "   thou art a lowly     ", " quitter. Is this true? ", "                        ",
	" Do I need to bust your ", "  face open for trying  ", "        to quit?        ", "                        ",
	" Man, I oughta smack you", "   for trying to quit!  ", "     Press Y to get     ", "      smacked out.      ",
	" Press Y to quit like a ", "   big loser in life.   ", "  Press N to stay proud ", "    and successful!     ",
	"   If you press Y to    ", "  quit, I will summon   ", "  Satan all over your   ", "      hard drive!       ",
	"  Um, Asmodeus dislikes ", " his children trying to ", " quit. Press Y to return", "   to your Tinkertoys.  ",
	"  If you quit now, I'll ", "  throw a blanket-party ", "   for you next time!   ", "                        "
};

void M_Menu_Quit_f(void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand() & 7;
}

void M_Quit_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit_f();
		break;

	default:
		break;
	}

}

void M_Quit_Draw(void)
{
	M_DrawTextBox(56, 76, 24, 4);
	M_Print(64, 84, quitMessage[msgNumber * 4 + 0]);
	M_Print(64, 92, quitMessage[msgNumber * 4 + 1]);
	M_Print(64, 100, quitMessage[msgNumber * 4 + 2]);
	M_Print(64, 108, quitMessage[msgNumber * 4 + 3]);
}

//=============================================================================
/* LAN CONFIG MENU */

int lanConfig_cursor = -1;
int lanConfig_cursor_table[] =
{ 72, 92, 124 };
#define NUM_LANCONFIG_CMDS	3

int lanConfig_port;
char lanConfig_portname[6];
char lanConfig_joinname[22];

void M_Menu_LanConfig_f(void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	snprintf(lanConfig_portname, sizeof(lanConfig_portname), "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}

void M_LanConfig_Draw(void)
{
	qpic_t *p;
	int basex;
	char *startJoin;
	char *protocol;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	basex = (320 - p->width) / 2;
	M_DrawPic(basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";

	protocol = "TCP/IP";
	M_Print(basex, 32, va("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print(basex, 52, "Address:");
	M_Print(basex + 9 * 8, 52, my_tcpip_address);

	M_Print(basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox(basex + 8 * 8, lanConfig_cursor_table[0] - 8, 6, 1);
	M_Print(basex + 9 * 8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print(basex, lanConfig_cursor_table[1], "Search for local games...");
		M_Print(basex, 108, "Join game at:");
		M_DrawTextBox(basex + 8, lanConfig_cursor_table[2] - 8, 22, 1);
		M_Print(basex + 16, lanConfig_cursor_table[2], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox(basex, lanConfig_cursor_table[1] - 8, 2, 1);
		M_Print(basex + 8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter(basex - 8, lanConfig_cursor_table[lanConfig_cursor], 12 + ((int) (realtime * 4) & 1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter(basex + 9 * 8 + 8 * strlen(lanConfig_portname), lanConfig_cursor_table[0],
				10 + ((int) (realtime * 4) & 1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter(basex + 16 + 8 * strlen(lanConfig_joinname), lanConfig_cursor_table[2],
				10 + ((int) (realtime * 4) & 1));

	if (*m_return_reason)
		M_PrintWhite(basex, 148, m_return_reason);
}

void M_LanConfig_Key(int key, int ascii)
{
	int l;

	switch (key)
	{
	case K_ESCAPE:
		M_Menu_MultiPlayer_f();
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f();
				break;
			}
			M_Menu_Search_f();
			break;
		}

		if (lanConfig_cursor == 2)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText(va("connect \"%s\"\n", lanConfig_joinname));
			break;
		}

		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname) - 1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname) - 1] = 0;
		}
		break;

	default:
		if (ascii < 32 || ascii > 127)
			break;

		if (lanConfig_cursor == 2)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l + 1] = 0;
				lanConfig_joinname[l] = key;
			}
		}

		if (ascii < '0' || ascii > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l + 1] = 0;
				lanConfig_portname[l] = key;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 2)
	{
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;
	}

	l = atoi(lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	snprintf(lanConfig_portname, sizeof(lanConfig_portname), "%u", lanConfig_port);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	char *name;
	char *description;
} level_t;

level_t levels[] = {
	{ "start", "Entrance" }, // 0

	{ "e1m1", "Slipgate Complex" }, // 1
	{ "e1m2", "Castle of the Damned" },
	{ "e1m3", "The Necropolis" },
	{ "e1m4", "The Grisly Grotto" },
	{ "e1m5", "Gloom Keep" },
	{ "e1m6", "The Door To Chthon" },
	{ "e1m7", "The House of Chthon" },
	{ "e1m8", "Ziggurat Vertigo" },

	{ "e2m1", "The Installation" }, // 9
	{ "e2m2", "Ogre Citadel" },
	{ "e2m3", "Crypt of Decay" },
	{ "e2m4", "The Ebon Fortress" },
	{ "e2m5", "The Wizard's Manse" },
	{ "e2m6", "The Dismal Oubliette" },
	{ "e2m7", "Underearth" },

	{ "e3m1", "Termination Central" }, // 16
	{ "e3m2", "The Vaults of Zin" },
	{ "e3m3", "The Tomb of Terror" },
	{ "e3m4", "Satan's Dark Delight" },
	{ "e3m5", "Wind Tunnels" },
	{ "e3m6", "Chambers of Torment" },
	{ "e3m7", "The Haunted Halls" },

	{ "e4m1", "The Sewage System" }, // 23
	{ "e4m2", "The Tower of Despair" },
	{ "e4m3", "The Elder God Shrine" },
	{ "e4m4", "The Palace of Hate" },
	{ "e4m5", "Hell's Atrium" },
	{ "e4m6", "The Pain Maze" },
	{ "e4m7", "Azure Agony" },
	{ "e4m8", "The Nameless City" },

	{ "end", "Shub-Niggurath's Pit" }, // 31

	{ "dm1", "Place of Two Deaths" }, // 32
	{ "dm2", "Claustrophobopolis" },
	{ "dm3", "The Abandoned Base" },
	{ "dm4", "The Bad Place" },
	{ "dm5", "The Cistern" },
	{ "dm6", "The Dark Zone" }
};

//MED 01/06/97 added hipnotic levels
level_t hipnoticlevels[] = {
	{ "start", "Command HQ" }, // 0

	{ "hip1m1", "The Pumping Station" }, // 1
	{ "hip1m2", "Storage Facility" },
	{ "hip1m3", "The Lost Mine" },
	{ "hip1m4", "Research Facility" },
	{ "hip1m5", "Military Complex" },

	{ "hip2m1", "Ancient Realms" }, // 6
	{ "hip2m2", "The Black Cathedral" },
	{ "hip2m3", "The Catacombs" },
	{ "hip2m4", "The Crypt" },
	{ "hip2m5", "Mortum's Keep" },
	{ "hip2m6", "The Gremlin's Domain" },

	{ "hip3m1", "Tur Torment" }, // 12
	{ "hip3m2", "Pandemonium" },
	{ "hip3m3", "Limbo" },
	{ "hip3m4", "The Gauntlet" },

	{ "hipend", "Armagon's Lair" }, // 16

	{ "hipdm1", "The Edge of Oblivion" } // 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t roguelevels[] ={
	{ "start", "Split Decision" },
	{ "r1m1", "Deviant's Domain" },
	{ "r1m2", "Dread Portal" },
	{ "r1m3", "Judgement Call" },
	{ "r1m4", "Cave of Death" },
	{ "r1m5", "Towers of Wrath" },
	{ "r1m6", "Temple of Pain" },
	{ "r1m7", "Tomb of the Overlord" },
	{ "r2m1", "Tempus Fugit" },
	{ "r2m2", "Elemental Fury I" },
	{ "r2m3", "Elemental Fury II" },
	{ "r2m4", "Curse of Osiris" },
	{ "r2m5", "Wizard's Keep" },
	{ "r2m6", "Blood Sacrifice" },
	{ "r2m7", "Last Bastion" },
	{ "r2m8", "Source of Evil" },
	{ "ctf1", "Division of Change" }
};

typedef struct
{
	char *description;
	int firstLevel;
	int levels;
} episode_t;

episode_t episodes[] = {
	{ "Welcome to Quake", 0, 1 },
	{ "Doomed Dimension", 1, 8 },
	{ "Realm of Black Magic", 9, 7 },
	{ "Netherworld", 16, 7 },
	{ "The Elder World", 23, 8 },
	{ "Final Level", 31, 1 },
	{ "Deathmatch Arena", 32, 6 }
};

//MED 01/06/97  added hipnotic episodes
episode_t hipnoticepisodes[] = {
	{ "Scourge of Armagon", 0, 1 },
	{ "Fortress of the Dead", 1, 5 },
	{ "Dominion of Darkness", 6, 6 },
	{ "The Rift", 12, 4 },
	{ "Final Level", 16, 1 },
	{ "Deathmatch Arena", 17, 1 }
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t rogueepisodes[] = {
	{ "Introduction", 0, 1 },
	{ "Hell's Fortress", 1, 7 },
	{ "Corridors of Time", 8, 8 },
	{ "Deathmatch Arena", 16, 1 }
};

int startepisode;
int startlevel;
int maxplayers;
bool m_serverInfoMessage = false;
double m_serverInfoMessageTime;

void M_Menu_GameOptions_f(void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}

int gameoptions_cursor_table[] =
{ 40, 56, 64, 72, 80, 88, 96, 112, 120 };
#define	NUM_GAMEOPTIONS	9
int gameoptions_cursor;

void M_GameOptions_Draw(void)
{
	qpic_t *p;
	int x;

	M_DrawTransPic(16, 4, Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);

	M_DrawTextBox(152, 32, 10, 1);
	M_Print(160, 40, "begin game");

	M_Print(0, 56, "      Max players");
	M_Print(160, 56, va("%i", maxplayers));

	M_Print(0, 64, "        Game Type");
	if (coop.value)
		M_Print(160, 64, "Cooperative");
	else
		M_Print(160, 64, "Deathmatch");

	M_Print(0, 72, "        Teamplay");
	if (rogue)
	{
		char *msg;

		switch ((int) teamplay.value)
		{
		case 1: msg = "No Friendly Fire"; break;
		case 2: msg = "Friendly Fire"; break;
		case 3: msg = "Tag"; break;
		case 4: msg = "Capture the Flag"; break;
		case 5: msg = "One Flag CTF"; break;
		case 6: msg = "Three Team CTF"; break;
		default: msg = "Off"; break;
		}
		M_Print(160, 72, msg);
	}
	else
	{
		char *msg;

		switch ((int) teamplay.value)
		{
		case 1: msg = "No Friendly Fire"; break;
		case 2: msg = "Friendly Fire"; break;
		default: msg = "Off"; break;
		}
		M_Print(160, 72, msg);
	}

	M_Print(0, 80, "            Skill");
	if (skill.value == 0)
		M_Print(160, 80, "Easy difficulty");
	else if (skill.value == 1)
		M_Print(160, 80, "Normal difficulty");
	else if (skill.value == 2)
		M_Print(160, 80, "Hard difficulty");
	else
		M_Print(160, 80, "Nightmare difficulty");

	M_Print(0, 88, "       Frag Limit");
	if (fraglimit.value == 0)
		M_Print(160, 88, "none");
	else
		M_Print(160, 88, va("%i frags", (int) fraglimit.value));

	M_Print(0, 96, "       Time Limit");
	if (timelimit.value == 0)
		M_Print(160, 96, "none");
	else
		M_Print(160, 96, va("%i minutes", (int) timelimit.value));

	M_Print(0, 112, "         Episode");
	//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
		M_Print(160, 112, hipnoticepisodes[startepisode].description);
	//PGM 01/07/97 added rogue episodes
	else if (rogue)
		M_Print(160, 112, rogueepisodes[startepisode].description);
	else
		M_Print(160, 112, episodes[startepisode].description);

	M_Print(0, 120, "           Level");
	//MED 01/06/97 added hipnotic episodes
	if (hipnotic)
	{
		M_Print(160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
		M_Print(160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
	}
	//PGM 01/07/97 added rogue episodes
	else if (rogue)
	{
		M_Print(160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
		M_Print(160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
	}
	else
	{
		M_Print(160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
		M_Print(160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
	}

// line cursor
	M_DrawCharacter(144, gameoptions_cursor_table[gameoptions_cursor], 12 + ((int) (realtime * 4) & 1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320 - 26 * 8) / 2;
			M_DrawTextBox(x, 138, 24, 4);
			x += 8;
			M_Print(x, 146, "  More than 4 players   ");
			M_Print(x, 154, " requires using command ");
			M_Print(x, 162, "line parameters; please ");
			M_Print(x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}

void M_NetStart_Change(int dir)
{
	int count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_SetValueQuick(&coop, coop.value ? 0 : 1);
		break;

	case 3:
		if (rogue)
			count = 6;
		else
			count = 2;

		Cvar_SetValueQuick(&teamplay, teamplay.value + dir);
		if (teamplay.value > count)
			Cvar_SetValueQuick(&teamplay, 0);
		else if (teamplay.value < 0)
			Cvar_SetValueQuick(&teamplay, count);
		break;

	case 4:
		Cvar_SetValueQuick(&skill, skill.value + dir);
		if (skill.value > 3)
			Cvar_SetValueQuick(&skill, 0);
		if (skill.value < 0)
			Cvar_SetValueQuick(&skill, 3);
		break;

	case 5:
		Cvar_SetValueQuick(&fraglimit, fraglimit.value + dir * 10);
		if (fraglimit.value > 100)
			Cvar_SetValueQuick(&fraglimit, 0);
		if (fraglimit.value < 0)
			Cvar_SetValueQuick(&fraglimit, 100);
		break;

	case 6:
		Cvar_SetValueQuick(&timelimit, timelimit.value + dir * 5);
		if (timelimit.value > 60)
			Cvar_SetValueQuick(&timelimit, 0);
		if (timelimit.value < 0)
			Cvar_SetValueQuick(&timelimit, 60);
		break;

	case 7:
		startepisode += dir;
		//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
		//PGM 01/07/97 added rogue count
		//PGM 03/02/97 added 1 for dmatch episode
		else if (rogue)
			count = 4;
		else if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		//MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
		//PGM 01/06/97 added hipnotic episodes
		else if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
			count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f();
		break;

	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound("misc/menu3.wav");
		M_NetStart_Change(-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound("misc/menu3.wav");
		M_NetStart_Change(1);
		break;

	case K_ENTER:
		S_LocalSound("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText("disconnect\n");
			Cbuf_AddText("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText(va("maxplayers %u\n", maxplayers));
			SCR_BeginLoadingPlaque();

			if (hipnotic)
				Cbuf_AddText(va("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name));
			else if (rogue)
				Cbuf_AddText(va("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name));
			else
				Cbuf_AddText(va("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name));

			return;
		}

		M_NetStart_Change(1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

bool searchComplete = false;
double searchCompleteTime;

void M_Menu_Search_f(void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f();

}

void M_Search_Draw(void)
{
	qpic_t *p;
	int x;

	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	x = (320 / 2) - ((12 * 8) / 2) + 4;
	M_DrawTextBox(x - 8, 32, 12, 1);
	M_Print(x, 40, "Searching...");

	if (slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (!searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f();
		return;
	}

	M_PrintWhite((320 / 2) - ((22 * 8) / 2), 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f();
}

void M_Search_Key(int key, int ascii)
{
}

//=============================================================================
/* SLIST MENU */

int slist_cursor;
bool slist_sorted;

void M_Menu_ServerList_f(void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}

void M_ServerList_Draw(void)
{
	int n;
	char string[64];
	qpic_t *p;

	if (!slist_sorted)
	{
		if (hostCacheCount > 1)
		{
			int i, j;
			hostcache_t temp;

			for (i = 0; i < hostCacheCount; i++)
				for (j = i + 1; j < hostCacheCount; j++)
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
						memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
					}
		}
		slist_sorted = true;
	}

	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320 - p->width) / 2, 4, p);
	for (n = 0; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			snprintf(string, sizeof(string), "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name,
					hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			snprintf(string, sizeof(string), "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print(16, 32 + 8 * n, string);
	}
	M_DrawCharacter(0, 32 + slist_cursor * 8, 12 + ((int) (realtime * 4) & 1));

	if (*m_return_reason)
		M_PrintWhite(16, 148, m_return_reason);
}

void M_ServerList_Key(int key, int ascii)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_LanConfig_f();
		break;

	case K_SPACE:
		M_Menu_Search_f();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
		S_LocalSound("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		slist_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText(va("connect \"%s\"\n", hostcache[slist_cursor].cname));
		break;

	default:
		break;
	}
}

//=============================================================================
/* Menu Subsystem */

void M_Init(void)
{
	Cmd_AddCommand("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand("menu_main", M_Menu_Main_f);
	Cmd_AddCommand("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand("menu_load", M_Menu_Load_f);
	Cmd_AddCommand("menu_save", M_Menu_Save_f);
	Cmd_AddCommand("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand("menu_options", M_Menu_Options_f);
	Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand("menu_preferences", M_Menu_Preferences_f);
	Cmd_AddCommand("menu_video", M_Menu_Video_f);
	Cmd_AddCommand("help", M_Menu_Help_f);
	Cmd_AddCommand("menu_quit", M_Menu_Quit_f);
}

void (*m_draw_callbacks[])() = {
	[m_none] = NULL,
	[m_main] = M_Main_Draw,
	[m_singleplayer] = M_SinglePlayer_Draw,
	[m_load] = M_Load_Draw,
	[m_save] = M_Save_Draw,
	[m_multiplayer] = M_MultiPlayer_Draw,
	[m_setup] = M_Setup_Draw,
	[m_options] = M_Options_Draw,
	[m_video] = M_Video_Draw,
	[m_keys] = M_Keys_Draw,
	[m_help] = M_Help_Draw,
	[m_quit] = M_Quit_Draw,
	[m_lanconfig] = M_LanConfig_Draw,
	[m_gameoptions] = M_GameOptions_Draw,
	[m_search] = M_Search_Draw,
	[m_slist] = M_ServerList_Draw,
	[m_preferences] = M_Pref_Options_Draw,
};

void M_Draw(void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (m_state > m_none && m_state <= m_preferences)
		m_draw_callbacks[m_state]();

	if (m_entersound)
	{
		S_LocalSound("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate();
}

void (*m_key_callbacks[])(int key, int ascii) = {
	[m_none] = NULL,
	[m_main] = M_Main_Key,
	[m_singleplayer] = M_SinglePlayer_Key,
	[m_load] = M_Load_Key,
	[m_save] = M_Save_Key,
	[m_multiplayer] = M_MultiPlayer_Key,
	[m_setup] = M_Setup_Key,
	[m_options] = M_Options_Key,
	[m_video] = M_Video_Key,
	[m_keys] = M_Keys_Key,
	[m_help] = M_Help_Key,
	[m_quit] = M_Quit_Key,
	[m_lanconfig] = M_LanConfig_Key,
	[m_gameoptions] = M_GameOptions_Key,
	[m_search] = M_Search_Key,
	[m_slist] = M_ServerList_Key,
	[m_preferences] = M_Pref_Options_Key,
};

void M_Keydown(int key, int ascii, bool down)
{
	if (m_state > m_none && m_state <= m_preferences)
		m_key_callbacks[m_state](key, ascii);
}

void M_ConfigureNetSubsystem(void)
{
	Cbuf_AddText("stopdemo\n");
	net_hostport = lanConfig_port;
}

void M_Charinput (int key)
{
	switch (m_state)
	{
	case m_setup:
		M_Setup_Key(key, 0);
		return;
	case m_quit:
		M_Quit_Key(key, 0);
		return;
	case m_lanconfig:
		M_LanConfig_Key(key, 0);
		return;
	default:
		return;
	}
}

//
//qboolean M_TextEntry (void)
//{
//	switch (m_state)
//	{
//	case m_setup:
//		return M_Setup_TextEntry ();
//	case m_quit:
//		return M_Quit_TextEntry ();
//	case m_lanconfig:
//		return M_LanConfig_TextEntry ();
//	default:
//		return false;
//	}
//}
