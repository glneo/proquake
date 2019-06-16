/*
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

keydest_t key_dest;

char *keybindings[MAX_KEYS];
bool consolekeys[MAX_KEYS]; // if true, can't be rebound while in console
bool menubound[MAX_KEYS]; // if true, can't be rebound while in menu
bool keydown[MAX_KEYS];

typedef struct
{
	const char *name;
	int keynum;
} keyname_t;

keyname_t keynames[] = {
	{ "TAB", K_TAB },
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "BACKSPACE", K_BACKSPACE },
	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "ALT", K_ALT },
	{ "CTRL", K_CTRL },
	{ "SHIFT", K_SHIFT },

//	{"KP_NUMLOCK", K_KP_NUMLOCK},
	{ "KP_SLASH", K_KP_SLASH },
	{ "KP_STAR", K_KP_STAR },
	{ "KP_MINUS", K_KP_MINUS },
	{ "KP_HOME", K_KP_HOME },
	{ "KP_UPARROW", K_KP_UPARROW },
	{ "KP_PGUP", K_KP_PGUP },
	{ "KP_PLUS", K_KP_PLUS },
	{ "KP_LEFTARROW", K_KP_LEFTARROW },
	{ "KP_5", K_KP_5 },
	{ "KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "KP_END", K_KP_END },
	{ "KP_DOWNARROW", K_KP_DOWNARROW },
	{ "KP_PGDN", K_KP_PGDN },
	{ "KP_ENTER", K_KP_ENTER },
	{ "KP_INS", K_KP_INS },
	{ "KP_DEL", K_KP_DEL },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "COMMAND", K_COMMAND },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },

	{ "JOY1", K_JOY1 },
	{ "JOY2", K_JOY2 },
	{ "JOY3", K_JOY3 },
	{ "JOY4", K_JOY4 },

	{ "AUX1", K_AUX1 },
	{ "AUX2", K_AUX2 },
	{ "AUX3", K_AUX3 },
	{ "AUX4", K_AUX4 },
	{ "AUX5", K_AUX5 },
	{ "AUX6", K_AUX6 },
	{ "AUX7", K_AUX7 },
	{ "AUX8", K_AUX8 },
	{ "AUX9", K_AUX9 },
	{ "AUX10", K_AUX10 },
	{ "AUX11", K_AUX11 },
	{ "AUX12", K_AUX12 },
	{ "AUX13", K_AUX13 },
	{ "AUX14", K_AUX14 },
	{ "AUX15", K_AUX15 },
	{ "AUX16", K_AUX16 },
	{ "AUX17", K_AUX17 },
	{ "AUX18", K_AUX18 },
	{ "AUX19", K_AUX19 },
	{ "AUX20", K_AUX20 },
	{ "AUX21", K_AUX21 },
	{ "AUX22", K_AUX22 },
	{ "AUX23", K_AUX23 },
	{ "AUX24", K_AUX24 },
	{ "AUX25", K_AUX25 },
	{ "AUX26", K_AUX26 },
	{ "AUX27", K_AUX27 },
	{ "AUX28", K_AUX28 },
	{ "AUX29", K_AUX29 },
	{ "AUX30", K_AUX30 },
	{ "AUX31", K_AUX31 },
	{ "AUX32", K_AUX32 },

	{ "PAUSE", K_PAUSE },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "SEMICOLON", ';' }, // because a raw semicolon seperates commands

	{ "BACKQUOTE", '`' }, // because a raw backquote may toggle the console
	{ "TILDE", '~' }, // because a raw tilde may toggle the console

	{ "LTHUMB", K_LTHUMB },
	{ "RTHUMB", K_RTHUMB },
	{ "LSHOULDER", K_LSHOULDER },
	{ "RSHOULDER", K_RSHOULDER },
	{ "ABUTTON", K_ABUTTON },
	{ "BBUTTON", K_BBUTTON },
	{ "XBUTTON", K_XBUTTON },
	{ "YBUTTON", K_YBUTTON },
	{ "LTRIGGER", K_LTRIGGER },
	{ "RTRIGGER", K_RTRIGGER },

	{ NULL, 0 }
};

//============================================================================

bool chat_team = false;
static char chat_buffer[256];
static int chat_bufferlen = 0;

const char *Key_GetChatBuffer(void)
{
	return chat_buffer;
}

static void Key_EndChat(void)
{
	key_dest = key_game;
	chat_bufferlen = 0;
	chat_buffer[0] = 0;
}

static void Key_Message(int key)
{
	switch (key)
	{
	case K_ENTER:
	case K_KP_ENTER:
		if (chat_team)
			Cbuf_AddText("say_team \"");
		else
			Cbuf_AddText("say \"");
		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		Key_EndChat();
		return;

	case K_ESCAPE:
		Key_EndChat();
		return;

	case K_BACKSPACE:
		if (chat_bufferlen)
			chat_buffer[--chat_bufferlen] = 0;
		return;
	}
}

static void Char_Message(int key)
{
	if (chat_bufferlen == sizeof(chat_buffer) - 1)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================

/*
 * Returns a key number to be used to index keybindings[] by looking at
 * the given string.  Single ascii characters return themselves, while
 * the K_* names are matched up
 */
static int Key_StringToKeynum(const char *str)
{
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (keyname_t *kn = keynames; kn->name; kn++)
	{
		if (!strcasecmp(str, kn->name))
			return kn->keynum;
	}

	return -1;
}

/*
 * Returns a string (either a single ascii char, or a K_* name) for the given keynum.
 * FIXME: handle quote special (general escape sequence?)
 */
const char *Key_KeynumToString(int keynum)
{
	static char tinystr[2];

	if (keynum == -1)
		return "<KEY NOT FOUND>";

	// printable ascii
	if (keynum > 32 && keynum < 127)
	{
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (keyname_t *kn = keynames; kn->name; kn++)
	{
		if (keynum == kn->keynum)
			return kn->name;
	}

	return "<UNKNOWN KEYNUM>";
}

void Key_SetBinding(int keynum, const char *binding)
{
	if (keynum == -1)
		return;

	// free old bindings
	if (keybindings[keynum])
	{
		free(keybindings[keynum]);
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	if (binding)
		keybindings[keynum] = Q_strdup(binding);
}

static void Key_Bind_f(void)
{
	int c = Cmd_Argc();
	if (c != 2 && c != 3)
	{
		Con_Printf("bind <key> [command] : attach a command to a key\n");
		return;
	}

	int keynum = Key_StringToKeynum(Cmd_Argv(1));
	if (keynum == -1)
	{
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[keynum])
			Con_Printf("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[keynum]);
		else
			Con_Printf("\"%s\" is not bound\n", Cmd_Argv(1));
		return;
	}

	// copy the rest of the command line
	char cmd[1024];
	cmd[0] = 0;
	for (int i = 2; i < c; i++)
	{
		strlcat(cmd, Cmd_Argv(i), sizeof(cmd));
		if (i != (c - 1))
			strlcat(cmd, " ", sizeof(cmd));
	}

	Key_SetBinding(keynum, cmd);
}

static void Key_Unbind_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf("unbind <key> : remove commands from a key\n");
		return;
	}

	int keynum = Key_StringToKeynum(Cmd_Argv(1));
	if (keynum == -1)
	{
		Con_Printf("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding(keynum, NULL);
}

static void Key_Unbindall_f(void)
{
	for (size_t keynum = 0; keynum < MAX_KEYS; keynum++)
	{
		if (keybindings[keynum])
			Key_SetBinding(keynum, NULL);
	}
}

static void Key_Bindlist_f(void)
{
	int count = 0;
	for (size_t keynum = 0; keynum < MAX_KEYS; keynum++)
	{
		if (keybindings[keynum] && *keybindings[keynum])
		{
			Con_SafePrintf("   %s \"%s\"\n", Key_KeynumToString(keynum), keybindings[keynum]);
			count++;
		}
	}
	Con_SafePrintf("%i bindings\n", count);
}

/* Writes lines containing "bind key value" */
void Key_WriteBindings(FILE *f)
{
	// unbindall before loading stored bindings
	if (cfg_unbindall.value)
		fprintf(f, "unbindall\n");

	for (size_t i = 0; i < MAX_KEYS; i++)
	{
		if (keybindings[i] && *keybindings[i])
			fprintf(f, "bind \"%s\" \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
	}
}

static struct
{
	bool active;
	int lastkey;
	int lastchar;
} key_inputgrab =
{ false, -1, -1 };

static void Key_ClearStates(void)
{
	for (size_t i = 0; i < MAX_KEYS; i++)
		if (keydown[i])
			Key_Event(i, false);
}

void Key_BeginInputGrab(void)
{
	Key_ClearStates();

	key_inputgrab.active = true;
	key_inputgrab.lastkey = -1;
	key_inputgrab.lastchar = -1;

	IN_UpdateInputMode();
}

void Key_EndInputGrab(void)
{
	Key_ClearStates();

	key_inputgrab.active = false;

	IN_UpdateInputMode();
}

void Key_GetGrabbedInput(int *lastkey, int *lastchar)
{
	if (lastkey)
		*lastkey = key_inputgrab.lastkey;
	if (lastchar)
		*lastchar = key_inputgrab.lastchar;
}

/*
 * Called by the system between frames for both key up and key down events
 * Should NOT be called during an interrupt!
 */
void Key_Event(int key, bool down)
{
	char *kb;
	char cmd[1024];

	if (key < 0 || key >= MAX_KEYS)
		return;

	// handle autorepeats and stray key up events
	if (down)
	{
		if (keydown[key])
		{
			if (key_dest == key_game && !con_forcedup)
				return; // ignore autorepeats in game mode
		}
		else if (key >= 200 && !keybindings[key])
			Con_Printf("%s is unbound, hit F4 to set.\n", Key_KeynumToString(key));
	}
	else if (!keydown[key])
		return; // ignore stray key up events

	keydown[key] = down;

	if (key_inputgrab.active)
	{
		if (down)
			key_inputgrab.lastkey = key;
		return;
	}

	// handle escape specialy, so the user can never unbind it
	if (key == K_ESCAPE)
	{
		if (!down)
			return;

		if (keydown[K_SHIFT])
		{
			Con_ToggleConsole_f();
			return;
		}

		switch (key_dest)
		{
		case key_message:
			Key_Message(key);
			break;
		case key_menu:
			M_Keydown(key, 0, down);
			break;
		case key_game:
		case key_console:
			M_ToggleMenu_f();
			break;
		default:
			Sys_Error("Bad key_dest");
		}

		return;
	}

	/* key up events are sent even if in console mode */

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	if (!down)
	{
		kb = keybindings[key];
		if (kb && kb[0] == '+')
		{
			sprintf(cmd, "-%s %i\n", kb + 1, key);
			Cbuf_AddText(cmd);
		}
		return;
	}

	// during demo playback, most keys bring up the main menu
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game && key != K_TAB)
	{
		M_ToggleMenu_f();
		return;
	}

	// if not a consolekey, send to the interpreter no matter what mode is
	if ((key_dest == key_menu && menubound[key]) || (key_dest == key_console && !consolekeys[key])
			|| (key_dest == key_game && (!con_forcedup || !consolekeys[key])))
	{
		kb = keybindings[key];
		if (kb)
		{
			if (kb[0] == '+')
			{	// button commands add keynum as a parm
				sprintf(cmd, "%s %i\n", kb, key);
				Cbuf_AddText(cmd);
			}
			else
			{
				Cbuf_AddText(kb);
				Cbuf_AddText("\n");
			}
		}
		return;
	}

	if (!down)
		return; // other systems only care about key down events

	switch (key_dest)
	{
	case key_message:
		Key_Message(key);
		break;
	case key_menu:
		M_Keydown(key, 0, down);
		break;

	case key_game:
	case key_console:
		Key_Console(key);
		break;
	default:
		Sys_Error("Bad key_dest");
	}
}

/* Called by the backend when the user has input a character */
void Char_Event(int key)
{
	if (key < 32 || key > 126)
		return;

#if defined(PLATFORM_OSX) || defined(PLATFORM_MAC)
	if (keydown[K_COMMAND])
	return;
#endif
	if (keydown[K_CTRL])
		return;

	if (key_inputgrab.active)
	{
		key_inputgrab.lastchar = key;
		return;
	}

	switch (key_dest)
	{
	case key_message:
		Char_Message(key);
		break;
	case key_menu:
//		M_Charinput (key);
		break;
	case key_game:
		if (con_forcedup)
			Char_Console(key);
		break;
	case key_console:
		Char_Console(key);
		break;
	default:
		break;
	}
}

bool Key_TextEntry(void)
{
	if (key_inputgrab.active)
		return true;

	switch (key_dest)
	{
	case key_message:
		return true;
	case key_menu:
//		return M_TextEntry();
		return true;
	case key_game:
		if (!con_forcedup)
			return false;
		return true;
	case key_console:
		return true;
	default:
		return false;
	}
}

void Key_UpdateForDest(void)
{
	static bool forced = false;

	if (cls.state == ca_dedicated)
		return;

	switch (key_dest)
	{
	case key_console:
		if (forced && cls.state == ca_connected)
		{
			forced = false;
			IN_Activate();
			key_dest = key_game;
		}
		break;
	case key_game:
		if (cls.state != ca_connected)
		{
			forced = true;
			IN_Deactivate();
			key_dest = key_console;

		}
		else
			forced = false;
		break;
	default:
		forced = false;
		break;
	}
}

void Key_Init(void)
{
	// initialize consolekeys[]
	for (size_t i = 32; i < 127; i++) // ascii characters
		consolekeys[i] = true;
	consolekeys['`'] = false;
	consolekeys['~'] = false;
	consolekeys[K_TAB] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_ESCAPE] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_CTRL] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_INS] = true;
	consolekeys[K_DEL] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_HOME] = true;
	consolekeys[K_END] = true;
	consolekeys[K_KP_NUMLOCK] = true;
	consolekeys[K_KP_SLASH] = true;
	consolekeys[K_KP_STAR] = true;
	consolekeys[K_KP_MINUS] = true;
	consolekeys[K_KP_HOME] = true;
	consolekeys[K_KP_UPARROW] = true;
	consolekeys[K_KP_PGUP] = true;
	consolekeys[K_KP_PLUS] = true;
	consolekeys[K_KP_LEFTARROW] = true;
	consolekeys[K_KP_5] = true;
	consolekeys[K_KP_RIGHTARROW] = true;
	consolekeys[K_KP_END] = true;
	consolekeys[K_KP_DOWNARROW] = true;
	consolekeys[K_KP_PGDN] = true;
	consolekeys[K_KP_ENTER] = true;
	consolekeys[K_KP_INS] = true;
	consolekeys[K_KP_DEL] = true;
#if defined(PLATFORM_OSX) || defined(PLATFORM_MAC)
	consolekeys[K_COMMAND] = true;
#endif
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;

	// initialize menubound[]
	menubound[K_ESCAPE] = true;
	for (size_t i = 0; i < 12; i++)
		menubound[K_F1 + i] = true;

	Cmd_AddCommand("bind", Key_Bind_f);
	Cmd_AddCommand("unbind", Key_Unbind_f);
	Cmd_AddCommand("unbindall", Key_Unbindall_f);
	Cmd_AddCommand("bindlist", Key_Bindlist_f);
}
