/*
 * General definitions
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

#ifndef __COMMON_H
#define __COMMON_H

#include <cstdbool>
#include <cstring>

typedef unsigned char byte;

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#define CLAMP(a, b, c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))

typedef struct sizebuf_s
{
	bool allowoverflow;	// if false, do a Sys_Error
	bool overflowed;		// set to true if the buffer size failed
	byte *data;
	int maxsize;
	int cursize;
} sizebuf_t;

void SZ_Alloc(sizebuf_t *buf, int startsize);
void SZ_Free(sizebuf_t *buf);
void SZ_Clear(sizebuf_t *buf);
void *SZ_GetSpace(sizebuf_t *buf, int length);
void SZ_Write(sizebuf_t *buf, const void *data, int length);
void SZ_Print(sizebuf_t *buf, const char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s *prev, *next;
} link_t;

void ClearLink(link_t *l);
void RemoveLink(link_t *l);
void InsertLinkBefore(link_t *l, link_t *before);
void InsertLinkAfter(link_t *l, link_t *after);

#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (size_t)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

//============================================================================

extern bool bigendien;

extern short (*BigShort)(short l);
extern short (*LittleShort)(short l);
extern int (*BigLong)(int l);
extern int (*LittleLong)(int l);
extern float (*BigFloat)(float l);
extern float (*LittleFloat)(float l);

//============================================================================

void MSG_WriteChar(sizebuf_t *sb, int c);
void MSG_WriteByte(sizebuf_t *sb, int c);
void MSG_WriteShort(sizebuf_t *sb, int c);
void MSG_WriteLong(sizebuf_t *sb, int c);
void MSG_WriteFloat(sizebuf_t *sb, float f);
void MSG_WriteString(sizebuf_t *sb, const char *s);
void MSG_WriteCoord(sizebuf_t *sb, float f);
void MSG_WriteAngle(sizebuf_t *sb, float f);
void MSG_WritePreciseAngle(sizebuf_t *sb, float f); // JPG - precise aim!!

extern int msg_readcount;
extern bool msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading(void);
int MSG_ReadChar(void);
int MSG_ReadByte(void);
int MSG_PeekByte(void); // JPG - need this to check for ProQuake messages
int MSG_ReadShort(void);
int MSG_ReadLong(void);
float MSG_ReadFloat(void);
char *MSG_ReadString(void);

float MSG_ReadCoord(void);
float MSG_ReadAngle(void);

float MSG_ReadPreciseAngle(void); // JPG - precise aim!!

//============================================================================

extern char com_token[1024];
extern bool com_eof;

const char *COM_Parse(const char *data);

extern int com_argc;
extern char **com_argv;

int COM_CheckParm(const char *parm);
void COM_Init(const char *path);
void COM_InitArgv(int argc, char **argv);

char *COM_SkipPath(char *pathname);
void COM_StripExtension(char *in, char *out);
char *COM_FileExtension(char *in);
void COM_FileBase (const char *in, char *out, size_t outsize);
void COM_DefaultExtension(char *path, const char *extension);

// does a varargs printf into a temp buffer
char *va(const char *format, ...);

char *CopyString(char *s);
void COM_SlashesForward_Like_Unix(char *WindowsStylePath);
void COM_Reduce_To_Parent_Path(char* myPath);
char *COM_NiceFloatString(float floatvalue);

//============================================================================

extern int com_filesize;
extern char com_basedir[MAX_OSPATH];

struct cache_user_s;

typedef struct pack_s pack_t;

extern char com_gamedir[MAX_OSPATH];

void COM_ForceExtension(char *path, const char *extension);	// by joe

FILE *COM_FileOpenWrite(const char *filename);

void COM_CreatePath(const char *path);
pack_t *COM_LoadPackFile(const char *packfile);
int COM_OpenFile(const char *filename, FILE **file);
void COM_CloseFile(FILE *h);

byte *COM_LoadTempFile(const char *path);
byte *COM_LoadHunkFile(const char *path);
void COM_LoadCacheFile(const char *path, struct cache_user_s *cu);
byte *COM_LoadMallocFile(const char *path);

// Misc
int COM_Minutes(int seconds);
int COM_Seconds(int seconds);
char *VersionString(void);
void Host_Version_f(void);

extern struct cvar_s registered;

extern bool standard_quake, rogue, hipnotic;

// strlcat and strlcpy, from OpenBSD
// Most (all?) BSDs already have them
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
# define HAVE_STRLCAT 1
# define HAVE_STRLCPY 1
#endif

#ifndef HAVE_STRLCAT
/*!
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t strlcat(char *dst, const char *src, size_t siz);
#endif  // #ifndef HAVE_STRLCAT

#ifndef HAVE_STRLCPY
/*!
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif  // #ifndef HAVE_STRLCPY

#endif /* __COMMON_H */
