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
#include <cstdarg>
#include <cassert> // strltrim strrtrim
#include <cerrno>
#include <climits>

#include "quakedef.h"

std::vector<std::string> largv;

cvar_t registered = { "registered", "0" };

bool com_modified; // set true if using non-id files

int static_registered = 1; // only for startup check, then set

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
 * All of Quake's data access is through a hierchal file system, but the contents
 * of the file system can be transparently merged from several sources.
 *
 * The "base directory" is the path to the directory holding the quake.exe and all
 * game directories. The sys_* files pass this to host_init in quakeparms_t->basedir.
 * This can be overridden with the "-basedir" command line parm to allow code
 * debugging in a different directory. The base directory is only used during
 * filesystem initialization.
 *
 * The "game directory" is the first tree on the search path and directory that all
 * generated files (savegames, screenshots, demos, config files) will be saved to.
 * This can be overridden with the "-game" command line parameter.  The game
 * directory can never be changed while quake is executing.  This is a precaution
 * against having a malicious server instruct clients to write files over areas they
 * shouldn't.
 */

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

short (*BigShort)(uint16_t s);
short (*LittleShort)(uint16_t s);
int (*BigLong)(uint32_t l);
int (*LittleLong)(uint32_t l);
float (*BigFloat)(float l);
float (*LittleFloat)(float l);

short ShortSwap(uint16_t s)
{
	uint16_t b1 = (s >> 0) & 0xff;
	uint16_t b2 = (s >> 8) & 0xff;

	return (short)((b1 << 8) |
	               (b2 << 0));
}

short ShortNoSwap(uint16_t s)
{
	return (short)s;
}

int LongSwap(uint32_t l)
{
	uint32_t b1 = (l >>  0) & 0xff;
	uint32_t b2 = (l >>  8) & 0xff;
	uint32_t b3 = (l >> 16) & 0xff;
	uint32_t b4 = (l >> 24) & 0xff;

	return (int)((b1 << 24) |
	             (b2 << 16) |
		     (b3 <<  8) |
		     (b4 <<  0));
}

int LongNoSwap(uint32_t l)
{
	return (int)l;
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
	char *buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		Sys_Error ("range error");
#endif

	buf = (char *)SZ_GetSpace(sb, 1);
	buf[0] = (char)c;
}

void MSG_WriteByte(sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		Sys_Error ("range error");
#endif

	buf = (byte *)SZ_GetSpace(sb, 1);
	buf[0] = (byte)c;
}

void MSG_WriteShort(sizebuf_t *sb, int c)
{
	byte *buf;

#ifdef PARANOID
	if (c < SHRT_MIN || c > SHRT_MAX)
		Sys_Error ("range error");
#endif

	buf = (byte *)SZ_GetSpace(sb, 2);
	buf[0] = (byte)((c >> 0) & 0xff);
	buf[1] = (byte)((c >> 8) & 0xff);
}

void MSG_WriteLong(sizebuf_t *sb, int c)
{
	byte *buf;

	buf = (byte *)SZ_GetSpace(sb, 4);
	buf[0] = (byte)((c >>  0) & 0xff);
	buf[1] = (byte)((c >>  8) & 0xff);
	buf[2] = (byte)((c >> 16) & 0xff);
	buf[3] = (byte)((c >> 24) & 0xff);
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

void MSG_WritePreciseAngle(sizebuf_t *sb, float f)
{
	MSG_WriteFloat(sb, f);
}

//
// reading functions
//
size_t msg_readcount;
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

	c = (unsigned char)net_message.data[msg_readcount];
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

	return (unsigned char)net_message.data[msg_readcount];
}

int MSG_ReadShort(void)
{
	int c;

	if (msg_readcount + 2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (short)((uint16_t)(net_message.data[msg_readcount + 0] << 0) |
	            (uint16_t)(net_message.data[msg_readcount + 1] << 8));

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

	c = (int)((uint16_t)(net_message.data[msg_readcount + 0] << 0) |
	          (uint16_t)(net_message.data[msg_readcount + 1] << 8) |
		  (uint16_t)(net_message.data[msg_readcount + 2] << 16) |
		  (uint16_t)(net_message.data[msg_readcount + 3] << 24));

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
		string[l] = (char)c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord(void)
{
	return (float)MSG_ReadShort() / 8.0f;
}

float MSG_ReadAngle(void)
{
	return (float)MSG_ReadChar() * (360.0f / 256);
}

float MSG_ReadPreciseAngle(void)
{
	return MSG_ReadFloat();
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

void *SZ_GetSpace(sizebuf_t *buf, size_t length)
{
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error("overflow without allowoverflow set");

		if (length > buf->maxsize)
			Sys_Error("%zu is > full buffer size", length);

		buf->overflowed = true;
		Con_Printf("SZ_GetSpace: overflow");
		SZ_Clear(buf);
	}

	void *data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write(sizebuf_t *buf, const void *data, size_t length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

void SZ_Print(sizebuf_t *buf, const char *data)
{
	size_t len = strlen(data) + 1;

	if (buf->data[buf->cursize - 1])
		memcpy((byte *) SZ_GetSpace(buf, len), data, len); // no trailing 0
	else
		memcpy((byte *) SZ_GetSpace(buf, len - 1) - 1, data, len); // write over trailing 0
}

//============================================================================

char *COM_SkipPath(char *pathname)
{
	char *last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

void COM_StripExtension(char *in, char *out)
{
	char *dot = strrchr(in, '.');
	if (!dot)
	{
		strlcpy(out, in, strlen(in) + 1);
		return;
	}

	while (*in && in != dot)
		*out++ = *in++;

	*out = 0;
}

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
 * If path doesn't have an extension or has a different extension, append(!)
 * specified extension Extension should include the .
 */
void COM_ForceExtension(char *path, const char *extension)
{
	char *src = path + strlen(path) - 1;
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
 * if path doesn't have a .EXT, append extension
 * (extension should include the .)
 */
void COM_DefaultExtension(char *path, const char *extension)
{
	char *src = path + strlen(path) - 1;

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
	char c;
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
 * Returns the position (1 to argc-1) in the program's argument list
 * where the given parameter apears, or 0 if not present
 */
int COM_CheckParm(const char *parm)
{
	size_t pos = std::find(largv.begin(), largv.end(), std::string(parm)) - largv.begin();
	if (pos == largv.size())
		return 0;

	return pos;
}

/*
 * Looks for the pop.txt file and verifies it.
 * Sets the "registered" cvar.
 * Immediately exits out if an alternate game was attempted to be started without
 * being registered.
 */
void COM_CheckRegistered(void)
{
	FILE *h;
	unsigned short check[128];
	int i;

	COM_OpenFile((char *)"gfx/pop.lmp", &h);
	static_registered = 0;

	if (!h)
	{
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

	Cvar_SetQuick(&registered, "1");
	static_registered = 1;
	Con_Printf("Playing registered version.\n");
}

void COM_InitArgv(int argc, char **argv)
{
	// reconstitute the command line for the cmdline externally visible cvar
	int n = 0;
	for (int j = 0; (j < MAX_NUM_ARGVS) && (j < argc); j++)
	{
		int i = 0;
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
}

/*
 =============================================================================

 QUAKE FILESYSTEM

 =============================================================================
 */

int com_filesize;

// in memory
typedef struct
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char filename[MAX_OSPATH];
	FILE *handle;
	size_t numfiles;
	packfile_t *files;
} pack_t;

// on disk
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

char com_gamedir[MAX_OSPATH];

typedef struct searchpath_s
{
	char pathname[MAX_OSPATH-3];
	pack_t *pack; // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t *com_searchpaths;

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
			Con_Printf("%s\n", s->pathname);
	}
}

/* The filename will be prefixed by the current game directory */
FILE *COM_FileOpenWrite(const char *filename)
{
	char name[MAX_OSPATH];

	Sys_mkdir(com_gamedir); // if we've switched to a nonexistant gamedir, create it now so we don't crash
	snprintf(name, sizeof(name), "%s/%s", com_gamedir, filename);

	return Sys_FileOpenWrite(name);
}

long COM_filelength(FILE *f)
{
	long pos = ftell(f);
	fseek(f, 0, SEEK_END);
	long end = ftell (f);
	fseek(f, pos, SEEK_SET);

	return end;
}

/*
 * Finds the file in the search path.
 * Sets com_filesize and one of handle or file
 * If the requested file is inside a packfile, a new FILE * will be opened
 * into the file
 */
long int COM_OpenFile(const char *filename, FILE **file)
{
	if (!file)
		Sys_Error("file pointer not set");

	// search through the path, one element at a time
	for (searchpath_t *search = com_searchpaths; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pack_t *pak = search->pack;
			for (size_t i = 0; i < pak->numfiles; i++)
				if (!strcmp(pak->files[i].name, filename))
				{
					// found it!
					Con_DPrintf("PackFile: %s : %s\n", pak->filename, filename);
					// open a new file on the pakfile
					*file = fopen(pak->filename, "rb");
					if (*file)
						fseek(*file, pak->files[i].filepos, SEEK_SET);
					com_filesize = pak->files[i].filelen;
					return com_filesize;
				}
		}
		else /* check a file in the directory tree */
		{
			if (!registered.value)
			{ /* if not a registered version, don't ever go beyond base */
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}

			char netpath[MAX_OSPATH];
			snprintf(netpath, sizeof(netpath), "%s/%s", search->pathname, filename);
			if (!Sys_FileExists(netpath))
				continue;

			*file = fopen (netpath, "rb");
			com_filesize = (*file == NULL) ? -1 : COM_filelength (*file);
			return com_filesize;
		}
	}

	Sys_Printf((char *)"FindFile: can't find %s\n", filename);

	*file = NULL;
	com_filesize = -1;
	return -1;
}

/* If it is a pak file handle, don't really close it */
void COM_CloseFile(FILE *h)
{
	for (searchpath_t *s = com_searchpaths; s; s = s->next)
		if (s->pack && s->pack->handle == h)
			return;

	Sys_FileClose(h);
}

/* Filename are relative to the quake directory */
byte *COM_LoadFile(const char *path, int usehunk)
{
	FILE *h;
	byte *buf;

	// look for it in the filesystem or pack files
	int len = COM_OpenFile(path, &h);
	if (!h)
		return NULL;

	if (usehunk == 1)
		buf = (byte *)Hunk_AllocName(len + 1, path);
	else if (usehunk == 5)
		buf = (byte *) Q_malloc(len + 1);
	else
		Sys_Error("bad usehunk");

	if (!buf)
		Sys_Error("not enough space for %s", path);

	/* Always appends a 0 byte */
	((byte *) buf)[len] = 0;

	Sys_FileRead(h, buf, len);
	COM_CloseFile(h);

	return buf;
}

byte *COM_LoadHunkFile(const char *path)
{
	return COM_LoadFile(path, 1);
}

// returns malloc'd memory
byte *COM_LoadMallocFile(const char *path)
{
	return COM_LoadFile(path, 5);
}

/*
 * Takes an explicit (not game tree related) path to a pak file.
 *
 * Loads the header and directory, adding the files at the beginning
 * of the list so they override previous pack files.
 */
pack_t *COM_LoadPackFile(const char *packfile)
{
	FILE *packhandle = Sys_FileOpenRead(packfile);
	if (!packhandle)
	{
		Con_Printf ("Couldn't open %s\n", packfile);
		return NULL;
	}

	dpackheader_t header;
	Sys_FileRead(packhandle, (void *) &header, sizeof(header));
	if (header.id[0] != 'P' ||
	    header.id[1] != 'A' ||
	    header.id[2] != 'C' ||
	    header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);

	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	size_t numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error("%s has %zu files", packfile, numpackfiles);

	if (numpackfiles != PAK0_COUNT)
		com_modified = true; // not the original file

	packfile_t *newfiles = (packfile_t *)Q_malloc(numpackfiles * sizeof(packfile_t));

	dpackfile_t info[MAX_FILES_IN_PACK];
	Sys_FileSeek(packhandle, header.dirofs);
	Sys_FileRead(packhandle, (void *)info, header.dirlen);

	// crc the directory to check for modifications
	unsigned short crc;
	CRC_Init(&crc);
	for (int i = 0; i < header.dirlen; i++)
		CRC_ProcessByte(&crc, ((byte *) info)[i]);
	if (crc != PAK0_CRC)
		com_modified = true;

	// parse the directory
	for (size_t i = 0; i < numpackfiles; i++)
	{
		strcpy(newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	pack_t *pack = (pack_t *)Q_malloc(sizeof(pack_t));

	strcpy(pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf("Added packfile %s (%i files)\n", packfile, numpackfiles);

	return pack;
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

/*
 * Sets com_gamedir, adds the directory to the head of the path,
 * then loads and adds pak1.pak pak2.pak ...
 */
static void COM_AddGameDirectory(char *dir)
{
	strcpy(com_gamedir, dir);

	// add the directory to the search path
	searchpath_t *search = (searchpath_t *)Q_malloc(sizeof(searchpath_t));

	strncpy(search->pathname, dir, sizeof(search->pathname));
	search->pack = NULL;
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	for (int i = 0; ;i++)
	{
		char pakfile[MAX_OSPATH];
		snprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);

		pack_t *pak = COM_LoadPackFile(pakfile);
		if (!pak)
			break;

		search = (searchpath_t *)Q_malloc(sizeof(searchpath_t));

		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

static void COM_InitFilesystem()
{
	char basedir[MAX_OSPATH];

	// -basedir <path>
	// Overrides the system supplied base directory (under GAMENAME)
	int i = COM_CheckParm((char *)"-basedir");
	if (i && i < com_argc - 1)
		strcpy(basedir, com_argv[i + 1]);
	else
		strcpy(basedir, host_parms.basedir);

	size_t j = strlen(basedir);
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

	// -game <gamedir>
	// Adds basedir/gamedir as an override game
	i = COM_CheckParm((char *)"-game");
	if (i && i < com_argc - 1)
	{
		com_modified = true;
		char *file = com_argv[i + 1];
		if (file[0] == '/') /* Absolute path */
			COM_AddGameDirectory(file);
		else
			COM_AddGameDirectory(va("%s/%s", basedir, com_argv[i + 1]));
	}

	// -path <dir or packfile> [<dir or packfile>] ...
	// Fully specifies the exact serach path, overriding the generated one
	i = COM_CheckParm((char *)"-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;
		while (++i < com_argc)
		{
			if (!com_argv[i] ||
			     com_argv[i][0] == '+' ||
			     com_argv[i][0] == '-')
				break;

			searchpath_t *search = (searchpath_t *)Q_malloc(sizeof(searchpath_t));
			if (!strcmp(COM_FileExtension(com_argv[i]), "pak"))
			{
				search->pack = COM_LoadPackFile(com_argv[i]);
				if (!search->pack)
					Sys_Error("Couldn't load packfile: %s", com_argv[i]);
			}
			else
				strncpy(search->pathname, com_argv[i], sizeof(search->pathname));
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}
}

static void COM_SwapInit()
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
}

void COM_Init(const char *basedir)
{
	COM_SwapInit();

	Cvar_RegisterVariable(&registered);
	Cmd_AddCommand("path", COM_Path_f);

	COM_InitFilesystem();
	COM_CheckRegistered();
}
