/*
 * Misc functions used in client and server
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

#include <string>
#include <vector>
#include <algorithm>

#include "quakedef.h"

#include <cassert>  // strltrim strrtrim

using namespace std;

vector<string> largv;

cvar_t registered = { (char *)"registered", (char *)"0" };
cvar_t cmdline = { (char *)"cmdline", (char *)"", false, true };

bool com_modified; // set true if using non-id files

int static_registered = 1;  // only for startup check, then set

// if a packfile directory differs from this, it is assumed to be modified
#define PAK0_COUNT 339
#define PAK0_CRC 32981

char com_token[1024];
char com_basedir[MAX_OSPATH];	// c:/quake

int com_argc;
char **com_argv;

#define CMDLINE_LENGTH 256
char com_cmdline[CMDLINE_LENGTH];

bool standard_quake = true, rogue, hipnotic;

// Special command line options

//bool		mod_conhide  = false;		// Conceal the console more
//bool		mod_nosoundwarn = false;	// Don't warn about missing sounds

// this graphic needs to be in the pak file to use registered features
static unsigned short pop[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x6600, 0x0000, 0x0000, 0x0000, 0x6600, 0x0000,
	0x0000, 0x0066, 0x0000, 0x0000, 0x0000, 0x0000, 0x0067, 0x0000,
	0x0000, 0x6665, 0x0000, 0x0000, 0x0000, 0x0000, 0x0065, 0x6600,
	0x0063, 0x6561, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6563,
	0x0064, 0x6561, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6564,
	0x0064, 0x6564, 0x0000, 0x6469, 0x6969, 0x6400, 0x0064, 0x6564,
	0x0063, 0x6568, 0x6200, 0x0064, 0x6864, 0x0000, 0x6268, 0x6563,
	0x0000, 0x6567, 0x6963, 0x0064, 0x6764, 0x0063, 0x6967, 0x6500,
	0x0000, 0x6266, 0x6769, 0x6a68, 0x6768, 0x6a69, 0x6766, 0x6200,
	0x0000, 0x0062, 0x6566, 0x6666, 0x6666, 0x6666, 0x6562, 0x0000,
	0x0000, 0x0000, 0x0062, 0x6364, 0x6664, 0x6362, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0062, 0x6662, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0061, 0x6661, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x6500, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x6400, 0x0000, 0x0000, 0x0000
};

/*

 All of Quake's data access is through a hierchal file system, but the contents
 of the file system can be transparently merged from several sources.

 The "base directory" is the path to the directory holding the quake.exe and all
 game directories. The sys_* files pass this to host_init in quakeparms_t->basedir.
 This can be overridden with the "-basedir" command line parm to allow code
 debugging in a different directory. The base directory is only used during
 filesystem initialization.

 The "game directory" is the first tree on the search path and directory that all
 generated files (savegames, screenshots, demos, config files) will be saved to.
 This can be overridden with the "-game" command line parameter.  The game
 directory can never be changed while quake is executing.  This is a precacution
 against having a malicious server instruct clients to write files over areas they
 shouldn't.

 The "cache directory" is only used during development to save network bandwidth,
 especially over ISDN / T1 lines.  If there is a cache directory specified, when
 a file is found by the normal search path, it will be mirrored into the cache
 directory, then opened there.


 FIXME:
 The file "parms.txt" will be read out of the game directory and appended to the
 current command line arguments to allow different games to initialize startup
 parms differently. This could be used to add a "-sspeed 22050" for the high
 quality sound edition.  Because they are added at the end, they will not
 override an explicit setting on the original command line.

 */

//============================================================================
// ClearLink is used for new headnodes
void ClearLink(link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink(link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore(link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter(link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
 ============================================================================

 BYTE ORDER FUNCTIONS

 ============================================================================
 */

bool bigendien;

short (*BigShort)(short l);
short (*LittleShort)(short l);
int (*BigLong)(int l);
int (*LittleLong)(int l);
float (*BigFloat)(float l);
float (*LittleFloat)(float l);

short ShortSwap(short l)
{
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short ShortNoSwap(short l)
{
	return l;
}

int LongSwap(int l)
{
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

int LongNoSwap(int l)
{
	return l;
}

float FloatSwap(float f)
{
	union
	{
		float f;
		byte b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap(float f)
{
	return f;
}

/*
 ==============================================================================

 MESSAGE IO FUNCTIONS

 Handles byte ordering and avoids alignment errors
 ==============================================================================
 */

//
// writing functions
//
void MSG_WriteChar(sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("range error");
#endif

	buf = (byte *)SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteByte(sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("range error");
#endif

	buf = (byte *)SZ_GetSpace(sb, 1);
	buf[0] = c;
}

void MSG_WriteShort(sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Sys_Error ("range error");
#endif

	buf = (byte *)SZ_GetSpace(sb, 2);
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

void MSG_WriteLong(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = (byte *)SZ_GetSpace(sb, 4);
	buf[0] = c & 0xff;
	buf[1] = (c >> 8) & 0xff;
	buf[2] = (c >> 16) & 0xff;
	buf[3] = c >> 24;
}

void MSG_WriteFloat(sizebuf_t *sb, float f)
{
	union
	{
		float f;
		int l;
	} dat;

	dat.f = f;
	dat.l = LittleLong(dat.l);

	SZ_Write(sb, &dat.l, 4);
}

void MSG_WriteString(sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write(sb, (void *)"", 1);
	else
		SZ_Write(sb, (void *)s, strlen(s) + 1);
}

void MSG_WriteCoord(sizebuf_t *sb, float f)
{
	MSG_WriteShort(sb, (int) (f * 8));
}

void MSG_WriteAngle(sizebuf_t *sb, float f)
{
	MSG_WriteByte(sb, ((int) f * 256 / 360) & 255);
}

// JPG - precise aim for ProQuake!
void MSG_WritePreciseAngle(sizebuf_t *sb, float f)
{

	int val = (int) f * 65536 / 360;
	MSG_WriteShort(sb, val & 65535);

}

//
// reading functions
//
int msg_readcount;
bool msg_badread;

void MSG_BeginReading(void)
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar(void)
{
	int c;

	if (msg_readcount + 1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (signed char) net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte(void)
{
	int c;

	if (msg_readcount + 1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (unsigned char) net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

// JPG - need this to check for ProQuake messages
int MSG_PeekByte(void)
{
	if (msg_readcount + 1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	return (unsigned char) net_message.data[msg_readcount];
}

int MSG_ReadShort(void)
{
	int c;

	if (msg_readcount + 2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (short) (net_message.data[msg_readcount] + (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong(void)
{
	int c;

	if (msg_readcount + 4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount] + (net_message.data[msg_readcount + 1] << 8) + (net_message.data[msg_readcount + 2] << 16)
			+ (net_message.data[msg_readcount + 3] << 24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat(void)
{
	union
	{
		byte b[4];
		float f;
		int l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong(dat.l);

	return dat.f;
}

char *MSG_ReadString(void)
{
	static char string[2048];
	unsigned int l;
	int c;

	l = 0;
	do
	{
		c = MSG_ReadChar();
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord(void)
{
	return MSG_ReadShort() * (1.0 / 8);
}

float MSG_ReadAngle(void)
{
	return MSG_ReadChar() * (360.0 / 256);
}

// JPG - exact aim for proquake!
float MSG_ReadPreciseAngle(void)
{
	return MSG_ReadShort() * (360.0 / 65536);
}

//===========================================================================

void SZ_Alloc(sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = (byte *)Hunk_AllocName(startsize, (char *)"sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}

void SZ_Free(sizebuf_t *buf)
{
	free(buf->data);
	buf->data = NULL;
	buf->maxsize = 0;
	buf->cursize = 0;
}

void SZ_Clear(sizebuf_t *buf)
{
	buf->cursize = 0;
}

void *SZ_GetSpace(sizebuf_t *buf, int length)
{
	void *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error("overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error("%i is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf("SZ_GetSpace: overflow");
		SZ_Clear(buf);
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write(sizebuf_t *buf, const void *data, int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t *buf, const char *data)
{
	int len;

	len = strlen(data) + 1;

// byte * cast to keep VC++ happy
	if (buf->data[buf->cursize - 1])
		memcpy((byte *) SZ_GetSpace(buf, len), data, len); // no trailing 0
	else
		memcpy((byte *) SZ_GetSpace(buf, len - 1) - 1, data, len); // write over trailing 0
}

//============================================================================

/*
 ============
 COM_SkipPath
 ============
 */
char *COM_SkipPath(char *pathname)
{
	char *last;

	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/*
 ============
 COM_StripExtension
 ============
 */
void COM_StripExtension(char *in, char *out)
{
	char *dot;

	if (!(dot = strrchr(in, '.')))
	{
		strlcpy(out, in, strlen(in) + 1);
		return;
	}

	while (*in && in != dot)
		*out++ = *in++;

	*out = 0;
}

/*
 ============
 COM_FileExtension

 ============
 */
char *COM_FileExtension(char *in)
{
	static char exten[8];
	int i;

	in = strrchr(in, '.');
	if (!in)
		return (char *)"";
	in++;
	for (i = 0; i < 7 && *in; i++, in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
take 'somedir/otherdir/filename.ext',
write only 'filename' to the output
============
*/
void COM_FileBase (const char *in, char *out, size_t outsize)
{
	const char	*dot, *slash, *s;

	s = in;
	slash = in;
	dot = NULL;
	while (*s)
	{
		if (*s == '/')
			slash = s + 1;
		if (*s == '.')
			dot = s;
		s++;
	}
	if (dot == NULL)
		dot = s;

	if (dot - slash < 2)
		strlcpy (out, "?model?", outsize);
	else
	{
		size_t	len = dot - slash;
		if (len >= outsize)
			len = outsize - 1;
		memcpy (out, slash, len);
		out[len] = '\0';
	}
}

/*
 ==================
 COM_ForceExtension

 If path doesn't have an extension or has a different extension, append(!) specified extension
 Extension should include the .
 ==================
 */
void COM_ForceExtension(char *path, const char *extension)
{
	char *src;

	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src-- == '.')
		{
			COM_StripExtension(path, path);
			strlcat(path, extension, sizeof(path));
			return;
		}
	}

	strlcat(path, extension, MAX_OSPATH);
}

/*
 ==================
 COM_DefaultExtension
 ==================
 */
void COM_DefaultExtension(char *path, const char *extension)
{
	char *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strlcat(path, extension, MAX_OSPATH);
}

/* Parse a token out of a string */
const char *COM_Parse(const char *data)
{
	int c;
	int len = 0;

	com_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL; // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' /* || c==':' */)                    // JPG 3.20 - so that ip:port works
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

/*
 ================
 COM_CheckParm

 Returns the position (1 to argc-1) in the program's argument list
 where the given parameter apears, or 0 if not present
 ================
 */
int COM_CheckParm(const char *parm)
{
	unsigned int pos = find(largv.begin(), largv.end(), string(parm))  - largv.begin();
	if (pos == largv.size())
		return 0;

	return pos;
}

/*
 ================
 COM_CheckRegistered

 Looks for the pop.txt file and verifies it.
 Sets the "registered" cvar.
 Immediately exits out if an alternate game was attempted to be started without
 being registered.
 ================
 */
void COM_CheckRegistered(void)
{
	int h;
	unsigned short check[128];
	int i;

	COM_OpenFile((char *)"gfx/pop.lmp", &h);
	static_registered = 0;

	if (h == -1)
	{
		//Sys_Error ("This dedicated server requires a full registered copy of Quake");
		Con_Printf("Playing shareware version.\n");
		//if (com_modified)
		//	Sys_Error ("You must have the registered version to use modified games");
		return;
	}

	Sys_FileRead(h, check, sizeof(check));
	COM_CloseFile(h);

	for (i = 0; i < 128; i++)
		if (pop[i] != (unsigned short) BigShort(check[i]))
			Sys_Error("Corrupted data file.");

	Cvar_SetQuick(&cmdline, com_cmdline);
	Cvar_SetQuick(&registered, "1");
	static_registered = 1;
	Con_Printf("Playing registered version.\n");
}

void COM_Path_f(void);

/*
 ================
 COM_InitArgv
 ================
 */
void COM_InitArgv(int argc, char **argv)
{
	int i, j, n;

	// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for (j = 0; (j < MAX_NUM_ARGVS) && (j < argc); j++)
	{
		i = 0;

		while ((n < (CMDLINE_LENGTH - 1)) && argv[j][i])
		{
			com_cmdline[n++] = argv[j][i++];
		}

		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}

	com_cmdline[n] = 0;

	for (com_argc = 0; com_argc < argc; com_argc++)
		largv.push_back(argv[com_argc]);

	com_argv = argv;

	if (COM_CheckParm((char *)"-rogue"))
	{
		rogue = true;
		standard_quake = false;
	}

	if (COM_CheckParm((char *)"-hipnotic"))
	{
		hipnotic = true;
		standard_quake = false;
	}

	//ProQuake 4.10 new extras
	//if (COM_CheckParm ("-conhide"))  		mod_conhide = true;
	//if (COM_CheckParm ("-nosoundwarn"))  	mod_nosoundwarn = true;

}

void COM_InitFilesystem();

/*
 ================
 COM_Init
 ================
 */
void COM_Init(const char *basedir)
{
	byte swaptest[2] = { 1, 0 };

// set the byte swapping variables in a portable manner
	if (*(short *) swaptest == 1)
	{
		bigendien = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cvar_RegisterVariable(&registered);
	Cvar_RegisterVariable(&cmdline);  // Baker 3.99c: needed for test2 command
	Cmd_AddCommand("path", COM_Path_f);

	COM_InitFilesystem();
	COM_CheckRegistered();
}

/*
 =============================================================================

 QUAKE FILESYSTEM

 =============================================================================
 */

int com_filesize;

//
// in memory
//

typedef struct
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char filename[MAX_OSPATH];
	int handle;
	int numfiles;
	packfile_t *files;
} pack_t;

//
// on disk
//
typedef struct
{
	char name[56];
	int filepos, filelen;
} dpackfile_t;

typedef struct
{
	char id[4];
	int dirofs;
	int dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK       2048

char com_gamedir[MAX_OSPATH];	// JPG 3.20 - added initialization

typedef struct searchpath_s
{
	char filename[MAX_OSPATH];
	pack_t *pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t *com_searchpaths = NULL;	// JPG 3.20 - added NULL
searchpath_t *com_verifypaths = NULL;	// JPG 3.20 - use original game directory for verify path

/*
 ============
 COM_Path_f
 ============
 */
void COM_Path_f(void)
{
	searchpath_t *s;

	Con_Printf("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next)
	{
		if (s->pack)
		{
			Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else
			Con_Printf("%s\n", s->filename);
	}
}

/*
 ============
 COM_WriteFile

 The filename will be prefixed by the current game directory
 ============
 */
void COM_WriteFile(const char *filename, void *data, int len)
{
	int handle;
	char name[MAX_OSPATH];

	Sys_mkdir(com_gamedir); //johnfitz -- if we've switched to a nonexistant gamedir, create it now so we don't crash

	snprintf(name, sizeof(name), "%s/%s", com_gamedir, filename);

	handle = Sys_FileOpenWrite(name);
	if (handle == -1)
	{
		Sys_Printf((char *)"COM_WriteFile: failed on %s\n", name);
		return;
	}

	Sys_Printf((char *)"COM_WriteFile: %s\n", name);
	Sys_FileWrite(handle, data, len);
	Sys_FileClose(handle);
}

/*
 ============
 COM_CreatePath
 ============
 */
void COM_CreatePath(const char *path)
{
	char *ofs;
	for(ofs = Q_strdup(path) + 1; *ofs; ofs++)
	{
		if (*ofs == '/')
		{       // create the directory
			*ofs = 0;
			Sys_mkdir(path);
			*ofs = '/';
		}
	}
	free(ofs);
}

/*
 ===========
 COM_CopyFile

 Copies a file over from the net to the local cache, creating any directories
 needed.  This is for the convenience of developers using ISDN from home.
 ===========
 */
void COM_CopyFile(char *netpath, char *cachepath)
{
	int in, out;
	unsigned int remaining, count;
	char buf[4096];

	remaining = Sys_FileOpenRead(netpath, &in);
	COM_CreatePath(cachepath);     // create directories up to the cache file
	out = Sys_FileOpenWrite(cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		Sys_FileRead(in, buf, count);
		Sys_FileWrite(out, buf, count);
		remaining -= count;
	}

	Sys_FileClose(in);
	Sys_FileClose(out);
}

long COM_filelength (FILE *f)
{
	long		pos, end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/*
 ===========
 COM_FindFile

 Finds the file in the search path.
 Sets com_filesize and one of handle or file
 ===========
 */
int COM_FindFile(const char *filename, int *handle, FILE **file)
{
	searchpath_t *search;
	char netpath[MAX_OSPATH];
	pack_t *pak;
	int i;
	int findtime;

	if (file && handle)
		Sys_Error("both handle and file set");
	if (!file && !handle)
		Sys_Error("neither handle or file set");

//
// search through the path, one element at a time
//
	search = com_searchpaths;

	for (; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;
			for (i = 0; i < pak->numfiles; i++)
				if (!strcmp(pak->files[i].name, filename))
				{       // found it!
					//Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
					if (handle)
					{
						*handle = pak->handle;
						Sys_FileSeek(pak->handle, pak->files[i].filepos);
					}
					else
					{       // open a new file on the pakfile
						*file = fopen(pak->filename, "rb");
						if (*file)
							fseek(*file, pak->files[i].filepos, SEEK_SET);
					}
					com_filesize = pak->files[i].filelen;
					return com_filesize;
				}
		}
		else	/* check a file in the directory tree */
		{
			if (!registered.value)
			{ /* if not a registered version, don't ever go beyond base */
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}

			snprintf(netpath, sizeof(netpath), "%s/%s",search->filename, filename);
			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;

			if (handle)
			{
				com_filesize = Sys_FileOpenRead (netpath, &i);
				*handle = i;
				return com_filesize;
			}
			else if (file)
			{
				*file = fopen (netpath, "rb");
				com_filesize = (*file == NULL) ? -1 : COM_filelength (*file);
				return com_filesize;
			}
			else
			{
				return 0; /* dummy valid value for COM_FileExists() */
			}
		}
	}

	Sys_Printf((char *)"FindFile: can't find %s\n", filename);

	if (handle)
		*handle = -1;
	else
		*file = NULL;
	com_filesize = -1;
	return -1;
}

/*
 ===========
 COM_OpenFile

 filename never has a leading slash, but may contain directory walks
 returns a handle and a length
 it may actually be inside a pak file
 ===========
 */
int COM_OpenFile(const char *filename, int *handle)
{
	return COM_FindFile(filename, handle, NULL);
}

/*
 ===========
 COM_FOpenFile

 If the requested file is inside a packfile, a new FILE * will be opened
 into the file.
 ===========
 */
int COM_FOpenFile(const char *filename, FILE **file)
{
	return COM_FindFile(filename, NULL, file);
}

/*
 ============
 COM_CloseFile

 If it is a pak file handle, don't really close it
 ============
 */
void COM_CloseFile(int h)
{
	searchpath_t *s;

	for (s = com_searchpaths; s; s = s->next)
		if (s->pack && s->pack->handle == h)
			return;

	Sys_FileClose(h);
}

/*
 ============
 COM_LoadFile

 Filename are reletive to the quake directory.
 Allways appends a 0 byte.
 ============
 */
cache_user_t *loadcache;
byte *loadbuf;
int loadsize;
byte *COM_LoadFile(const char *path, int usehunk)
{
	int h;
	byte *buf;
	char base[32];
	int len;

	buf = NULL;     // quiet compiler warning

// look for it in the filesystem or pack files
	len = COM_OpenFile(path, &h);
	if (h == -1)
		return NULL;

// extract the filename base name for hunk tag
	COM_FileBase(path, base, 32);

	if (usehunk == 1)
		buf = (byte *)Hunk_AllocName(len + 1, base);
	else if (usehunk == 2)
		buf = (byte *)Hunk_TempAlloc(len + 1);
	else if (usehunk == 0)
		buf = (byte *)Q_malloc(len + 1);
	else if (usehunk == 3)
		buf = (byte *)Cache_Alloc(loadcache, len + 1, base);
	else if (usehunk == 4)
	{
		if (len + 1 > loadsize)
			buf = (byte *)Hunk_TempAlloc(len + 1);
		else
			buf = loadbuf;
	}
	else if (usehunk == 5)
		buf = (byte *) Q_malloc(len + 1);
	else
		Sys_Error("bad usehunk");

	if (!buf)
		Sys_Error("not enough space for %s", path);

	((byte *) buf)[len] = 0;

	Sys_FileRead(h, buf, len);
	COM_CloseFile(h);

	return buf;
}

byte *COM_LoadHunkFile(const char *path)
{
	return COM_LoadFile(path, 1);
}

byte *COM_LoadTempFile(const char *path)
{
	return COM_LoadFile(path, 2);
}

void COM_LoadCacheFile(const char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile(path, 3);
}

// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile(const char *path, void *buffer, int bufsize)
{
	byte *buf;

	loadbuf = (byte *) buffer;
	loadsize = bufsize;
	buf = COM_LoadFile(path, 4);

	return buf;
}

// returns malloc'd memory
byte *COM_LoadMallocFile(const char *path)
{
	return COM_LoadFile(path, 5);
}

/*
 =================
 COM_LoadPackFile -- johnfitz -- modified based on topaz's tutorial


 Takes an explicit (not game tree related) path to a pak file.

 Loads the header and directory, adding the files at the beginning
 of the list so they override previous pack files.
 =================
 */
pack_t *COM_LoadPackFile(const char *packfile)
{
	dpackheader_t header;
	int i;
	packfile_t *newfiles;
	int numpackfiles;
	pack_t *pack;
	int packhandle;
	dpackfile_t info[MAX_FILES_IN_PACK];
	unsigned short crc;

	if (Sys_FileOpenRead(packfile, &packhandle) == -1)
	{
//              Con_Printf ("Couldn't open %s\n", packfile);
		return NULL;
	}
	Sys_FileRead(packhandle, (void *) &header, sizeof(header));
	if (header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error("%s has %i files", packfile, numpackfiles);

	if (numpackfiles != PAK0_COUNT)
		com_modified = true;    // not the original file

	//johnfitz -- dynamic gamedir loading

	//Hunk_AllocName (numpackfiles * sizeof(packfile_t), "packfile");

	newfiles = (packfile_t *)Q_malloc(numpackfiles * sizeof(packfile_t));

	//johnfitz

	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, (void *) info, header.dirlen);

// crc the directory to check for modifications
	CRC_Init(&crc);
	for (i = 0; i < header.dirlen; i++)
		CRC_ProcessByte(&crc, ((byte *) info)[i]);
	if (crc != PAK0_CRC)
		com_modified = true;

// parse the directory
	for (i = 0; i < numpackfiles; i++)
	{
		strcpy(newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	//johnfitz -- dynamic gamedir loading

	//pack = Hunk_Alloc (sizeof (pack_t));

	pack = 	(pack_t *)Q_malloc(sizeof(pack_t));

	//johnfitz

	strcpy(pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	// FitzQuake has this commented out

	Con_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}

/*
 ================
 COM_AddGameDirectory -- johnfitz -- modified based on topaz's tutorial


 Sets com_gamedir, adds the directory to the head of the path,
 then loads and adds pak1.pak pak2.pak ...
 ================
 */
void COM_AddGameDirectory(char *dir)
{
	int i;
	searchpath_t *search;
	pack_t *pak;
	char pakfile[MAX_OSPATH];

	strcpy(com_gamedir, dir);

// add the directory to the search path
	search = (searchpath_t *)Q_malloc(sizeof(searchpath_t));

	strcpy(search->filename, dir);
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

// add any pak files in the format pak0.pak pak1.pak, ...
	for (i = 0;; i++)
	{
		snprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile(pakfile);
		if (!pak)
			break;
		search = (searchpath_t *)Q_malloc(sizeof(searchpath_t));

		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

/*
 =================

 COM_InitFilesystem
 =================

 */
void COM_InitFilesystem() //johnfitz -- modified based on topaz's tutorial

{
	int i, j;
	char basedir[MAX_OSPATH];
	searchpath_t *search;

//
// -basedir <path>
// Overrides the system supplied base directory (under GAMENAME)
//
	i = COM_CheckParm((char *)"-basedir");
	if (i && i < com_argc - 1)
		strcpy(basedir, com_argv[i + 1]);
	else
		strcpy(basedir, host_parms.basedir);

	j = strlen(basedir);

	if (j > 0)
	{
		if ((basedir[j - 1] == '\\') || (basedir[j - 1] == '/'))
			basedir[j - 1] = 0;
	}
	strlcpy(com_basedir, basedir, sizeof(com_basedir));

// start up with GAMENAME by default (id1)
	COM_AddGameDirectory(va("%s/" GAMENAME, basedir));
	strcpy(com_gamedir, va("%s/" GAMENAME, basedir));   // Baker 3.60 - From FitzQuake

	if (COM_CheckParm((char *)"-rogue"))
		COM_AddGameDirectory(va("%s/rogue", basedir));
	if (COM_CheckParm((char *)"-hipnotic"))
		COM_AddGameDirectory(va("%s/hipnotic", basedir));

	com_verifypaths = com_searchpaths;	// JPG 3.20

//
// -game <gamedir>
// Adds basedir/gamedir as an override game
//
	i = COM_CheckParm((char *)"-game");
	if (i && i < com_argc - 1)
	{
		com_modified = true;
		COM_AddGameDirectory(va("%s/%s", basedir, com_argv[i + 1]));
	}

//
// -path <dir or packfile> [<dir or packfile>] ...
// Fully specifies the exact serach path, overriding the generated one
//
	i = COM_CheckParm((char *)"-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;
			search = (searchpath_t *)Hunk_Alloc(sizeof(searchpath_t));
			if (!strcmp(COM_FileExtension(com_argv[i]), "pak"))
			{
				search->pack = COM_LoadPackFile(com_argv[i]);
				if (!search->pack)
					Sys_Error("Couldn't load packfile: %s", com_argv[i]);
			}
			else
				strcpy(search->filename, com_argv[i]);
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}
}

void COM_SlashesForward_Like_Unix(char *WindowsStylePath)
{
	size_t i;
	// Translate "\" to "/"
	for (i = 0; i < strlen(WindowsStylePath); i++)
		if (WindowsStylePath[i] == '\\')
			WindowsStylePath[i] = '/';
}

void COM_Reduce_To_Parent_Path(char* myPath)
{
	char* terminatePoint = strrchr(myPath, '/');

	if (terminatePoint)
		*terminatePoint = '\0';

}

char *COM_NiceFloatString(float floatvalue)
{
	static char buildstring[32];
	int i;

	snprintf(buildstring, sizeof(buildstring), "%f", floatvalue);

	// Strip off ending zeros
	for (i = strlen(buildstring) - 1; i > 0 && buildstring[i] == '0'; i--)
		buildstring[i] = 0;

	// Strip off ending period
	if (buildstring[i] == '.')
		buildstring[i] = 0;

	return buildstring;
}

int COM_Minutes(int seconds)
{
	return seconds / 60;
}

int COM_Seconds(int seconds)
{
	return seconds % 60;
}

#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return (dlen + strlen(s));
	while (*s != '\0')
	{
		if (n != 1)
		{
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return (dlen + (s - src)); /* count does not include NUL */
}
#endif // #ifndef HAVE_STRLCAT

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0)
	{
		do
		{
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0)
	{
		if (siz != 0)
			*d = '\0'; /* NUL-terminate dst */
		while (*s++)
			;
	}

	return (s - src - 1); /* count does not include NUL */
}
#endif // #ifndef HAVE_STRLCPY

///////////////////////////////////////////////////////////////////////////////
//
//
//	va:  does a varargs printf into a temp buffer
//
//
///////////////////////////////////////////////////////////////////////////////

char *va(const char *format, ...)
{
	static char buffers[8][1024];
	// Let this malarky self-calculate and just do it once per session
	// There is still a single point of control
	// and the rest of the code adapts.
	static size_t sizeof_a_buffer = sizeof(buffers[0]);
	static size_t num_buffers = sizeof(buffers) / sizeof(buffers[0]);
	static size_t cycle = 0;

	char *buffer_to_use = buffers[cycle];
	va_list args;

	va_start(args, format);
	vsnprintf(buffer_to_use, sizeof_a_buffer, format, args);
	va_end(args);

	// Cycle through to next buffer for next time function is called
	if (++cycle >= num_buffers)
		cycle = 0;

	return buffer_to_use;
}
