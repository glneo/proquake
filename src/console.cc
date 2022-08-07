/*
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
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

#include <cstdarg>

#include "quakedef.h"
#include "glquake.h"

#define CON_TEXTSIZE 65536 // new default size
#define CON_MINSIZE  16384 // old default, now the minimum size

#define HISTORY_FILE_NAME "history.txt"


#define CMDLINES        64
#define	MAXCMDLINE      256

static const float con_cursorspeed = 4;

int con_linewidth;
int con_buffersize;

bool con_forcedup;		// because no entities to refresh

int con_totallines;		// total lines in console scrollback
unsigned int con_backscroll;		// lines up from bottom to display
int con_current;		// where next message will be printed
int con_x;				// offset in current line for next print
char *con_text = NULL;

cvar_t con_notifytime = { "con_notifytime", "3", CVAR_NONE }; //seconds
cvar_t con_logcenterprint = { "con_logcenterprint", "1", CVAR_NONE };
cvar_t _con_notifylines = { "con_notifylines", "4" };
int con_notifylines;		// scan lines to clear for

#define	CON_LASTCENTERSTRING_SIZE 1024
char con_lastcenterstring[CON_LASTCENTERSTRING_SIZE];

#define	NUM_CON_TIMES 4
float con_times[NUM_CON_TIMES];	// realtime time the line was generated
// for transparent notify lines

bool con_debuglog = false;

bool con_initialized;

char		key_lines[CMDLINES][MAXCMDLINE];

int		key_linepos;
int		key_insert;	//johnfitz -- insert key toggle (for editing)
double		key_blinktime; //johnfitz -- fudge cursor blinking to make it easier to spot in certain cases

int		edit_line = 0;
int		history_line = 0;

extern bool keydown[256];

void Con_DrawPic(int x, int y, qpic_t *pic)
{
	Draw_Pic(x, y, pic, scr_conalpha.value);
}

void Con_DrawCharacter(int x, int y, int num)
{
	Draw_Character(x, y, num, 1.0f);
}

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void History_Init (void)
{
	int i, c;
	FILE *hf;

	for (i = 0; i < CMDLINES; i++)
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	hf = fopen(va("%s/%s", host_parms.userdir, HISTORY_FILE_NAME), "rt");
	if (hf != NULL)
	{
		do
		{
			i = 1;
			do
			{
				c = fgetc(hf);
				key_lines[edit_line][i++] = c;
			} while (c != '\r' && c != '\n' && c != EOF && i < MAXCMDLINE);
			key_lines[edit_line][i - 1] = 0;
			edit_line = (edit_line + 1) & (CMDLINES - 1);
			/* for people using a windows-generated history file on unix: */
			if (c == '\r' || c == '\n')
			{
				do
					c = fgetc(hf);
				while (c == '\r' || c == '\n');
				if (c != EOF)
					ungetc(c, hf);
				else	c = 0; /* loop once more, otherwise last line is lost */
			}
		} while (c != EOF && edit_line < CMDLINES);
		fclose(hf);

		history_line = edit_line = (edit_line - 1) & (CMDLINES - 1);
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0;
	}
}

void History_Shutdown(void)
{
	int i;
	FILE *hf;

	hf = fopen(va("%s/%s", host_parms.userdir, HISTORY_FILE_NAME), "wt");
	if (hf != NULL)
	{
		i = edit_line;
		do
		{
			i = (i + 1) & (CMDLINES - 1);
		} while (i != edit_line && !key_lines[i][1]);

		while (i != edit_line && key_lines[i][1])
		{
			fprintf(hf, "%s\n", key_lines[i] + 1);
			i = (i + 1) & (CMDLINES - 1);
		}
		fclose(hf);
	}
}

static void PasteToConsole (void)
{
	char *cbd, *p, *workline;
	int mvlen, inslen;

	if (key_linepos == MAXCMDLINE - 1)
		return;

//	if ((cbd = PL_GetClipboardData()) == NULL)
	return;

	p = cbd;
	while (*p)
	{
		if (*p == '\n' || *p == '\r' || *p == '\b')
		{
			*p = 0;
			break;
		}
		p++;
	}

	inslen = (int) (p - cbd);
	if (inslen + key_linepos > MAXCMDLINE - 1)
		inslen = MAXCMDLINE - 1 - key_linepos;
	if (inslen <= 0) goto done;

	workline = key_lines[edit_line];
	workline += key_linepos;
	mvlen = (int) strlen(workline);
	if (mvlen + inslen + key_linepos > MAXCMDLINE - 1)
	{
		mvlen = MAXCMDLINE - 1 - key_linepos - inslen;
		if (mvlen < 0) mvlen = 0;
	}

	// insert the string
	if (mvlen != 0)
		memmove (workline + inslen, workline, mvlen);
	memcpy (workline, cbd, inslen);
	key_linepos += inslen;
	workline[mvlen + inslen] = '\0';
  done:
	free(cbd);
}

/*
====================
Key_Console -- johnfitz -- heavy revision

Interactive line editing and console scrollback
====================
*/
extern	char *con_text, key_tabpartial[MAXCMDLINE];
extern	int con_current, con_linewidth; //, con_vislines;

void Key_Console (int key)
{
	static	char current[MAXCMDLINE] = "";
	int	history_line_last;
	size_t		len;
	char *workline = key_lines[edit_line];

	switch (key)
	{
	case K_ENTER:
	case K_KP_ENTER:
		key_tabpartial[0] = 0;
		Cbuf_AddText (workline + 1);	// skip the prompt
		Cbuf_AddText ("\n");
		Con_Printf ("%s\n", workline);

		// If the last two lines are identical, skip storing this line in history
		// by not incrementing edit_line
		if (strcmp(workline, key_lines[(edit_line-1)&31]))
			edit_line = (edit_line + 1) & 31;

		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_lines[edit_line][1] = 0; //johnfitz -- otherwise old history items show up in the new edit line
		key_linepos = 1;
		if (cls.state == ca_disconnected)
			SCR_UpdateScreen (); // force an update, because the command may take some time
		return;

	case K_TAB:
		Con_TabComplete ();
		return;

	case K_BACKSPACE:
		key_tabpartial[0] = 0;
		if (key_linepos > 1)
		{
			workline += key_linepos - 1;
			if (workline[1])
			{
				len = strlen(workline);
				memmove (workline, workline + 1, len);
			}
			else	*workline = 0;
			key_linepos--;
		}
		return;

	case K_DEL:
		key_tabpartial[0] = 0;
		workline += key_linepos;
		if (*workline)
		{
			if (workline[1])
			{
				len = strlen(workline);
				memmove (workline, workline + 1, len);
			}
			else	*workline = 0;
		}
		return;

	case K_HOME:
		if (keydown[K_CTRL])
		{
			//skip initial empty lines
			int i, x;
			char *line;

			for (i = con_current - con_totallines + 1; i <= con_current; i++)
			{
				line = con_text + (i % con_totallines) * con_linewidth;
				for (x = 0; x < con_linewidth; x++)
				{
					if (line[x] != ' ')
						break;
				}
				if (x != con_linewidth)
					break;
			}
			con_backscroll = con_current - (i % con_totallines) - 2;
			con_backscroll = CLAMP((unsigned int)0, con_backscroll, con_totallines-(vid.height>>3)-1);
		}
		else	key_linepos = 1;
		return;

	case K_END:
		if (keydown[K_CTRL])
			con_backscroll = 0;
		else	key_linepos = strlen(workline);
		return;

	case K_PGUP:
	case K_MWHEELUP:
		con_backscroll += 2;
		if (con_backscroll > con_totallines - (vid.height>>3) - 1)
			con_backscroll = con_totallines - (vid.height>>3) - 1;
		return;

	case K_PGDN:
	case K_MWHEELDOWN:
		con_backscroll -= 2;
		if (con_backscroll < 0)
			con_backscroll = 0;
		return;

	case K_LEFTARROW:
		if (key_linepos > 1)
		{
			key_linepos--;
			key_blinktime = realtime;
		}
		return;

	case K_RIGHTARROW:
		len = strlen(workline);
		if ((int)len == key_linepos)
		{
			len = strlen(key_lines[(edit_line + 31) & 31]);
			if ((int)len <= key_linepos)
				return; // no character to get
			workline += key_linepos;
			*workline = key_lines[(edit_line + 31) & 31][key_linepos];
			workline[1] = 0;
			key_linepos++;
		}
		else
		{
			key_linepos++;
			key_blinktime = realtime;
		}
		return;

	case K_UPARROW:
		if (history_line == edit_line)
			strcpy(current, workline);

		history_line_last = history_line;
		do
		{
			history_line = (history_line - 1) & 31;
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
		{
			history_line = history_line_last;
			return;
		}

		key_tabpartial[0] = 0;
		strcpy(workline, key_lines[history_line]);
		key_linepos = strlen(workline);
		return;

	case K_DOWNARROW:
		if (history_line == edit_line)
			return;

		key_tabpartial[0] = 0;

		do
		{
			history_line = (history_line + 1) & 31;
		} while (history_line != edit_line && !key_lines[history_line][1]);

		if (history_line == edit_line)
			strcpy(workline, current);
		else	strcpy(workline, key_lines[history_line]);
		key_linepos = strlen(workline);
		return;

	case K_INS:
		if (keydown[K_SHIFT])		/* Shift-Ins paste */
			PasteToConsole();
		else	key_insert ^= 1;
		return;

	case 'v':
	case 'V':
#if defined(PLATFORM_OSX) || defined(PLATFORM_MAC)
		if (keydown[K_COMMAND]) {	/* Cmd+v paste (Mac-only) */
			PasteToConsole();
			return;
		}
#endif
		if (keydown[K_CTRL]) {		/* Ctrl+v paste */
			PasteToConsole();
			return;
		}
		break;

	case 'c':
	case 'C':
		if (keydown[K_CTRL]) {		/* Ctrl+C: abort the line -- S.A */
			Con_Printf ("%s\n", workline);
			workline[0] = ']';
			workline[1] = 0;
			key_linepos = 1;
			history_line= edit_line;
			return;
		}
		break;
	}
}

void Char_Console (int key)
{
	size_t		len;
	char *workline = key_lines[edit_line];

	if (key_linepos < MAXCMDLINE-1)
	{
		bool endpos = !workline[key_linepos];

		key_tabpartial[0] = 0; //johnfitz
		// if inserting, move the text to the right
		if (key_insert && !endpos)
		{
			workline[MAXCMDLINE - 2] = 0;
			workline += key_linepos;
			len = strlen(workline) + 1;
			memmove (workline + 1, workline, len);
			*workline = key;
		}
		else
		{
			workline += key_linepos;
			*workline = key;
			// null terminate if at the end
			if (endpos)
				workline[1] = 0;
		}
		key_linepos++;
	}
}

/*
 * Prints a bar of the desired length, but never wider than the console
 * includes a newline, unless len >= con_linewidth
 */
void Con_Quakebar(int len)
{
	char bar[42];

	len = min(len, (int )sizeof(bar) - 2);
	len = min(len, con_linewidth);

	bar[0] = '\35';
	for (int i = 1; i < len - 1; i++)
		bar[i] = '\36';
	bar[len - 1] = '\37';

	if (len < con_linewidth)
	{
		bar[len] = '\n';
		bar[len + 1] = 0;
	}
	else
		bar[len] = 0;

	Con_Printf("%s", bar);
}

extern int history_line; //johnfitz

void Con_ToggleConsole_f(void)
{
	if (key_dest == key_console/* || (key_dest == key_game && con_forcedup)*/)
	{
		key_lines[edit_line][1] = 0;	// clear any typing
		key_linepos = 1;
		con_backscroll = 0; //johnfitz -- toggleconsole should return you to the bottom of the scrollback
		history_line = edit_line; //johnfitz -- it should also return you to the bottom of the command history

		if (cls.state == ca_connected)
		{
			IN_Activate();
			key_dest = key_game;
		}
		else
		{
			M_Menu_Main_f();
		}
	}
	else
	{
		IN_Deactivate();
		key_dest = key_console;
	}

	SCR_EndLoadingPlaque();
	memset(con_times, 0, sizeof(con_times));
}

static void Con_Clear_f(void)
{
	if (con_text)
		memset(con_text, ' ', con_buffersize);
	con_backscroll = 0; // if console is empty, being scrolled up is confusing
}

/* adapted from quake2 source */
static void Con_Dump_f(void)
{
	int l, x;
	const char *line;
	char buffer[1024];

	FILE *f = COM_FileOpenWrite("condump.txt");
	if (!f)
	{
		Con_Printf("ERROR: couldn't open file condump.txt\n");
		return;
	}

	// skip initial empty lines
	for (l = con_current - con_totallines + 1; l <= con_current; l++)
	{
		line = con_text + (l % con_totallines) * con_linewidth;
		for (x = 0; x < con_linewidth; x++)
			if (line[x] != ' ')
				break;
		if (x != con_linewidth)
			break;
	}

	// write the remaining lines
	buffer[con_linewidth] = 0;
	for (; l <= con_current; l++)
	{
		line = con_text + (l % con_totallines) * con_linewidth;
		strncpy(buffer, line, con_linewidth);
		for (x = con_linewidth - 1; x >= 0; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x = 0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf(f, "%s\n", buffer);
	}

	fclose(f);
	Con_Printf("Dumped console text to condump.txt\n");
}

void Con_ClearNotify(void)
{
	int i;

	for (i = 0; i < NUM_CON_TIMES; i++)
		con_times[i] = 0;
}

/* If the line width has changed, reformat the buffer */
void Con_CheckResize(void)
{
	int i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char *tbuf; //johnfitz -- tbuf no longer a static array

	width = (vid.conwidth >> 3) - 2; //johnfitz -- use vid.conwidth instead of vid.width

	if (width == con_linewidth)
		return;

	oldwidth = con_linewidth;
	con_linewidth = width;
	oldtotallines = con_totallines;
	con_totallines = con_buffersize / con_linewidth; //johnfitz -- con_buffersize replaces CON_TEXTSIZE
	numlines = oldtotallines;

	if (con_totallines < numlines)
		numlines = con_totallines;

	numchars = oldwidth;

	if (con_linewidth < numchars)
		numchars = con_linewidth;

	tbuf = (char *)Q_malloc(con_buffersize); //johnfitz

	memcpy(tbuf, con_text, con_buffersize); //johnfitz -- con_buffersize replaces CON_TEXTSIZE
	memset(con_text, ' ', con_buffersize); //johnfitz -- con_buffersize replaces CON_TEXTSIZE

	for (i = 0; i < numlines; i++)
	{
		for (j = 0; j < numchars; j++)
		{
			con_text[(con_totallines - 1 - i) * con_linewidth + j] = tbuf[((con_current - i + oldtotallines) % oldtotallines) * oldwidth + j];
		}
	}

	free(tbuf);

	Con_ClearNotify();

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

static void Con_Linefeed(void)
{
	//johnfitz -- improved scrolling
	if (con_backscroll)
		con_backscroll++;
	if (con_backscroll > con_totallines - (vid.height >> 3) - 1)
		con_backscroll = con_totallines - (vid.height >> 3) - 1;
	//johnfitz

	con_x = 0;
	con_current++;
	memset(&con_text[(con_current % con_totallines) * con_linewidth], ' ', con_linewidth);
}

/* Handles cursor positioning, line wrapping, etc */
static void Con_Print(const char *txt)
{
	int y;
	int c, l;
	static int cr;
	int mask;
	bool boundary;

	//con_backscroll = 0; //johnfitz -- better console scrolling

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		S_LocalSound("misc/talk.wav");	// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;

	boundary = true;

	while ((c = *txt))
	{
		if (c <= ' ')
		{
			boundary = true;
		}
		else if (boundary)
		{
			// count word length
			for (l = 0; l < con_linewidth; l++)
				if (txt[l] <= ' ')
					break;

			// word wrap
			if (l != con_linewidth && (con_x + l > con_linewidth))
				con_x = 0;

			boundary = false;
		}

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}

		if (!con_x)
		{
			Con_Linefeed();
			// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y * con_linewidth + con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
	}
}

/* All console printing must go through this in order to be logged to disk */
void Con_Printf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];
	static bool inupdate = false;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	// also echo to debugging console
	Sys_Printf("%s", msg);

	if (!con_initialized)
		return;

	if (cls.state == ca_dedicated)
		return; // no graphics mode

	// write it to the scrollable buffer
	Con_Print(msg);

	// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading)
	{
		// protect against infinite loop if something in SCR_UpdateScreen calls this function
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen();
			inupdate = false;
		}
	}
}

/*
 * same as Con_Warning, but only prints if "developer" cvar is set, use for
 * "exceeds standard limit of" messages, which are only relevant for developers
 * targetting vanilla engines
 */
void Con_DWarning(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.value)
		return; // don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Con_SafePrintf("\x02Warning: ");
	Con_Printf("%s", msg);
}

/* prints a warning to the console */
void Con_Warning(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Con_SafePrintf("\x02Warning: ");
	Con_Printf("%s", msg);
}

/* A Con_Printf that only shows up if the "developer" cvar is set */
void Con_DPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.value)
		return; // don't confuse non-developers with techie stuff...

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Con_SafePrintf("%s", msg);
}

/* Okay to call even when the screen can't be updated */
void Con_SafePrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[1024];
	int temp;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf("%s", msg);
	scr_disabled_for_loading = temp;
}

void Con_CenterPrintf(int linewidth, const char *fmt, ...) __attribute__((__format__(__printf__,2,3)));
void Con_CenterPrintf(int linewidth, const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG]; //the original message
	char line[MAXPRINTMSG]; //one line from the message
	char spaces[21]; //buffer for spaces
	char *src, *dst;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	linewidth = min(linewidth, con_linewidth);
	for (src = msg; *src;)
	{
		dst = line;
		while (*src && *src != '\n')
			*dst++ = *src++;
		*dst = 0;
		if (*src == '\n')
			src++;

		int len = strlen(line);
		if (len < linewidth)
		{
			int s = (linewidth - len) / 2;
			memset(spaces, ' ', s);
			spaces[s] = 0;
			Con_Printf("%s%s\n", spaces, line);
		}
		else
			Con_Printf("%s\n", line);
	}
}

/* echo centerprint message to the console */
void Con_LogCenterPrint(const char *str)
{
	//ignore duplicates
	if (!strcmp(str, con_lastcenterstring))
		return;

	//don't log in deathmatch
	if (cl.gametype == GAME_DEATHMATCH && con_logcenterprint.value != 2)
		return;

	strcpy(con_lastcenterstring, str);

	if (con_logcenterprint.value)
	{
		Con_Quakebar(40);
		Con_CenterPrintf(40, "%s\n", str);
		Con_Quakebar(40);
		Con_ClearNotify();
	}
}

/*
 ==============================================================================

 TAB COMPLETION

 ==============================================================================
 */

//johnfitz -- tab completion stuff
//unique defs
char key_tabpartial[MAXCMDLINE];
typedef struct tab_s
{
	const char *name;
	const char *type;
	struct tab_s *next;
	struct tab_s *prev;
} tab_t;
tab_t *tablist;

//defs from elsewhere

typedef struct cmd_function_s
{
	struct cmd_function_s *next;
	const char *name;
	xcommand_t function;
} cmd_function_t;
extern cmd_function_t *cmd_functions;
#define	MAX_ALIAS_NAME	32
typedef struct cmdalias_s
{
	struct cmdalias_s *next;
	char name[MAX_ALIAS_NAME];
	char *value;
} cmdalias_t;
extern cmdalias_t *cmd_alias;

/*
 ============
 AddToTabList -- johnfitz

 tablist is a doubly-linked loop, alphabetized by name
 ============
 */

// bash_partial is the string that can be expanded,
// aka Linux Bash shell. -- S.A.
static char bash_partial[80];
static bool bash_singlematch;

void AddToTabList(const char *name, const char *type)
{
	tab_t *t, *insert;
	char *i_bash;
	const char *i_name;

	if (!*bash_partial)
	{
		strncpy(bash_partial, name, 79);
		bash_partial[79] = '\0';
	}
	else
	{
		bash_singlematch = 0;
		// find max common between bash_partial and name
		i_bash = bash_partial;
		i_name = name;
		while (*i_bash && (*i_bash == *i_name))
		{
			i_bash++;
			i_name++;
		}
		*i_bash = 0;
	}

	t = (tab_t *) Hunk_Alloc(sizeof(tab_t));
	t->name = name;
	t->type = type;

	if (!tablist) //create list
	{
		tablist = t;
		t->next = t;
		t->prev = t;
	}
	else if (strcmp(name, tablist->name) < 0) //insert at front
	{
		t->next = tablist;
		t->prev = tablist->prev;
		t->next->prev = t;
		t->prev->next = t;
		tablist = t;
	}
	else //insert later
	{
		insert = tablist;
		do
		{
			if (strcmp(name, insert->name) < 0)
				break;
			insert = insert->next;
		} while (insert != tablist);

		t->next = insert;
		t->prev = insert->prev;
		t->next->prev = t;
		t->prev->next = t;
	}
}

// This is redefined from host_cmd.c
typedef struct filelist_item_s
{
	char name[32];
	struct filelist_item_s *next;
} filelist_item_t;

//extern filelist_item_t *extralevels;
extern filelist_item_t *modlist;
//extern filelist_item_t *demolist;

typedef struct arg_completion_type_s
{
	const char *command;
	filelist_item_t **filelist;
} arg_completion_type_t;

static const arg_completion_type_t arg_completion_types[] = {
//	{ "map ", &extralevels },
//	{ "changelevel ", &extralevels },
		{ "game ", &modlist },
//	{ "record ", &demolist },
//	{ "playdemo ", &demolist },
//	{ "timedemo ", &demolist }
		};

static const int num_arg_completion_types = sizeof(arg_completion_types) / sizeof(arg_completion_types[0]);

/*
 ============
 FindCompletion -- stevenaaus
 ============
 */
const char *FindCompletion(const char *partial, filelist_item_t *filelist, int *nummatches_out)
{
	static char matched[32];
	char *i_matched, *i_name;
	filelist_item_t *file;
	int init, match, plen;

	memset(matched, 0, sizeof(matched));
	plen = strlen(partial);
	match = 0;

	for (file = filelist, init = 0; file; file = file->next)
	{
		if (!strncmp(file->name, partial, plen))
		{
			if (init == 0)
			{
				init = 1;
				strncpy(matched, file->name, sizeof(matched) - 1);
				matched[sizeof(matched) - 1] = '\0';
			}
			else
			{ // find max common
				i_matched = matched;
				i_name = file->name;
				while (*i_matched && (*i_matched == *i_name))
				{
					i_matched++;
					i_name++;
				}
				*i_matched = 0;
			}
			match++;
		}
	}

	*nummatches_out = match;

	if (match > 1)
	{
		for (file = filelist; file; file = file->next)
		{
			if (!strncmp(file->name, partial, plen))
				Con_SafePrintf("   %s\n", file->name);
		}
		Con_SafePrintf("\n");
	}

	return matched;
}

/*
 ============
 BuildTabList -- johnfitz
 ============
 */
void BuildTabList(const char *partial)
{
	cmdalias_t *alias;
	cvar_t *cvar;
	cmd_function_t *cmd;
	int len;

	tablist = NULL;
	len = strlen(partial);

	bash_partial[0] = 0;
	bash_singlematch = 1;

	cvar = Cvar_FindVarAfter("", CVAR_NONE);
	for (; cvar; cvar = cvar->next)
		if (!strncmp(partial, cvar->name, len))
			AddToTabList(cvar->name, "cvar");

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!strncmp(partial, cmd->name, len))
			AddToTabList(cmd->name, "command");

	for (alias = cmd_alias; alias; alias = alias->next)
		if (!strncmp(partial, alias->name, len))
			AddToTabList(alias->name, "alias");
}

void Con_TabComplete(void)
{
	char partial[MAXCMDLINE];
	const char *match;
	static char *c;
	tab_t *t;
	int mark, i;

	// if editline is empty, return
	if (key_lines[edit_line][1] == 0)
		return;

	// get partial string (space -> cursor)
	if (!key_tabpartial[0]) //first time through, find new insert point. (Otherwise, use previous.)
	{
		//work back from cursor until you find a space, quote, semicolon, or prompt
		c = key_lines[edit_line] + key_linepos - 1; //start one space left of cursor
		while (*c != ' ' && *c != '\"' && *c != ';' && c != key_lines[edit_line])
			c--;
		c++; //start 1 char after the separator we just found
	}
	strlcpy(partial, c, key_lines[edit_line] + key_linepos - c);

// Map autocomplete function -- S.A
// Since we don't have argument completion, this hack will do for now...
	for (i = 0; i < num_arg_completion_types; i++)
	{
		// arg_completion contains a command we can complete the arguments
		// for (like "map ") and a list of all the maps.
		arg_completion_type_t arg_completion = arg_completion_types[i];
		const char *command_name = arg_completion.command;

		if (!strncmp(key_lines[edit_line] + 1, command_name, strlen(command_name)))
		{
			int nummatches = 0;
			const char *matched_map = FindCompletion(partial, *arg_completion.filelist, &nummatches);
			if (!*matched_map)
				return;
			strlcpy(partial, matched_map, MAXCMDLINE);
			*c = '\0';
			strlcat(key_lines[edit_line], partial, MAXCMDLINE);
			key_linepos = c - key_lines[edit_line] + strlen(matched_map); //set new cursor position
			if (key_linepos >= MAXCMDLINE)
				key_linepos = MAXCMDLINE - 1;
			// if only one match, append a space
			if (key_linepos < MAXCMDLINE - 1 && key_lines[edit_line][key_linepos] == 0 && (nummatches == 1))
			{
				key_lines[edit_line][key_linepos] = ' ';
				key_linepos++;
				key_lines[edit_line][key_linepos] = 0;
			}
			c = key_lines[edit_line] + key_linepos;
			return;
		}
	}

//if partial is empty, return
	if (partial[0] == 0)
		return;

//trim trailing space becuase it screws up string comparisons
	if (i > 0 && partial[i - 1] == ' ')
		partial[i - 1] = 0;

// find a match
	mark = Hunk_LowMark();
	if (!key_tabpartial[0]) //first time through
	{
		strlcpy(key_tabpartial, partial, MAXCMDLINE);
		BuildTabList(key_tabpartial);

		if (!tablist)
			return;

		// print list if length > 1
		if (tablist->next != tablist)
		{
			t = tablist;
			Con_SafePrintf("\n");
			do
			{
				Con_SafePrintf("   %s (%s)\n", t->name, t->type);
				t = t->next;
			} while (t != tablist);
			Con_SafePrintf("\n");
		}

		//	match = tablist->name;
		// First time, just show maximum matching chars -- S.A.
		match = bash_partial;
	}
	else
	{
		BuildTabList(key_tabpartial);

		if (!tablist)
			return;

		//find current match -- can't save a pointer because the list will be rebuilt each time
		t = tablist;
		match = keydown[K_SHIFT] ? t->prev->name : t->name;
		do
		{
			if (!strcmp(t->name, partial))
			{
				match = keydown[K_SHIFT] ? t->prev->name : t->next->name;
				break;
			}
			t = t->next;
		} while (t != tablist);
	}
	Hunk_FreeToLowMark(mark); //it's okay to free it here because match is a pointer to persistent data

// insert new match into edit line
	strlcpy(partial, match, MAXCMDLINE); //first copy match string
	strlcat(partial, key_lines[edit_line] + key_linepos, MAXCMDLINE); //then add chars after cursor
	*c = '\0';	//now copy all of this into edit line
	strlcat(key_lines[edit_line], partial, MAXCMDLINE);
	key_linepos = c - key_lines[edit_line] + strlen(match); //set new cursor position
	if (key_linepos >= MAXCMDLINE)
		key_linepos = MAXCMDLINE - 1;

// if cursor is at end of string, let's append a space to make life easier
	if (key_linepos < MAXCMDLINE - 1 && key_lines[edit_line][key_linepos] == 0 && bash_singlematch)
	{
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
		// S.A.: the map argument completion (may be in combination with the bash-style
		// display behavior changes, causes weirdness when completing the arguments for
		// the changelevel command. the line below "fixes" it, although I'm not sure about
		// the reason, yet, neither do I know any possible side effects of it:
		c = key_lines[edit_line] + key_linepos;
	}
}

/*
 ==============================================================================

 DRAWING

 ==============================================================================
 */

/* Draws the last few lines of output transparently over the game top */
void Con_DrawNotify(void)
{
	int v = vid.conheight;
	int maxlines = CLAMP(0, (int)_con_notifylines.value, NUM_CON_TIMES);
	for (int i = con_current - maxlines + 1; i <= con_current; i++)
	{
		if (i < 0)
			continue;
		float time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime.value)
			continue;
		const char *text = con_text + (i % con_totallines) * con_linewidth;

		clearnotify = 0;

		for (int x = 0; x < con_linewidth; x++)
			Con_DrawCharacter((x + 1) << 3, v, text[x]);

		v += 8;
	}

	if (key_dest == key_message)
	{
		clearnotify = 0;

		int msg_start = 0;
//		if (team_message)
//		{
//			Draw_String(8, v, "(say team):");
//			msg_start = 12;
//		}
//		else
//		{
//			Draw_String(8, v, "say:");
//			msg_start = 5;
//		}

		int x = msg_start;
		const char *chat_buffer = Key_GetChatBuffer();
		for (int i = 0; chat_buffer[i]; i++)
		{
			Con_DrawCharacter(x << 3, v, chat_buffer[i]);
			x++;

			if (x > con_linewidth)
			{
				x = msg_start;
				v += 8;
			}
		}

		Con_DrawCharacter(x << 3, v, 10 + ((int) (realtime * con_cursorspeed) & 1));
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/* The input line scrolls horizontally if typing goes beyond the right edge */
static void Con_DrawInput(int con_vislines)
{
	if (key_dest != key_console && !con_forcedup)
		return; // don't draw anything

	// pre-step if horizontally scrolling
	int ofs = 0;
	if (key_linepos >= con_linewidth)
		ofs = 1 + key_linepos - con_linewidth;

	// draw input string
	int i;
	for (i = 0; key_lines[edit_line][i+ofs] && i < con_linewidth; i++)
		Con_DrawCharacter ((i+1)<<3, vid.conheight - 16, key_lines[edit_line][i+ofs]);

	// draw cursor
	if ((int)((realtime)*con_cursorspeed) & 1)
	{
		i = key_linepos - ofs;
		Con_DrawCharacter((i + 1) << 3, vid.conheight - 16, 11);
	}
}

void Con_DrawConsole(int lines, bool drawinput)
{
	if (lines <= 0)
		return;

	// draw the buffer text
	int con_vislines = lines * vid.conheight / vid.height;
	int rows = (con_vislines + 7) / 8;
	int y = vid.conheight - (rows * 8);
	rows -= 2; //for input and version lines
	int sb = (con_backscroll) ? 2 : 0;

	for (int i = con_current - rows + 1; i <= con_current - sb; i++)
	{
		int j = i - con_backscroll;
		if (j < 0)
			j = 0;

		const char *text = con_text + (j % con_totallines) * con_linewidth;
		for (int x = 0; x < con_linewidth; x++)
			Con_DrawCharacter((x + 1) << 3, y, text[x]);

		y += 8;
	}

	// draw scrollback arrows
	if (con_backscroll)
	{
		y += 8; // blank line
		for (int x = 0; x < con_linewidth; x += 4)
			Con_DrawCharacter((x + 1) << 3, y, '^');
		y += 8;
	}

	// draw the input prompt, user text, and cursor
	if (drawinput)
		Con_DrawInput(con_vislines);

	//draw version number in bottom right
//	y += 8;
	char ver[32];
	sprintf(ver, "QuickQuake %1.2f.%d", (float) 2, 3);
	for (int x = 0; x < (int) strlen(ver); x++)
		Con_DrawCharacter((con_linewidth - strlen(ver) + x + 2) << 3, y, ver[x] /*+ 128*/);
}

void Con_Init(void)
{
	int i = COM_CheckParm("-consize");
	if (i && i < com_argc - 1)
		con_buffersize = max(CON_MINSIZE, atoi(com_argv[i + 1]) * 1024);
	else
		con_buffersize = CON_TEXTSIZE;

	con_text = (char *)Hunk_AllocName(con_buffersize, "con_text");
	memset(con_text, ' ', con_buffersize);

	con_linewidth = 38;
	con_totallines = con_buffersize / con_linewidth;
	con_backscroll = 0;
	con_current = con_totallines - 1;

	Cvar_RegisterVariable(&con_notifytime);
	Cvar_RegisterVariable(&con_logcenterprint);
	Cvar_RegisterVariable(&_con_notifylines);

	Cmd_AddCommand("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand("clear", Con_Clear_f);
	Cmd_AddCommand("condump", Con_Dump_f);

	con_initialized = true;
	Con_Printf("Console initialized.\n");

	History_Init();

	key_blinktime = realtime;
}
