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

/*
 * Use this instead of malloc so that if memory allocation fails,
 * the program exits with a message saying there's not enough memory
 * instead of crashing after trying to use a NULL pointer
 */
void *Q_malloc(size_t size)
{
	void *p;

	if (!(p = malloc(size)))
		Sys_Error("Not enough free memory");

	return p;
}

void *Q_calloc(size_t n, size_t size)
{
	void *p;

	if (!(p = calloc(n, size)))
		Sys_Error("Not enough free memory");

	return p;
}

void *Q_realloc(void *ptr, size_t size)
{
	void *p;

	if (!(p = realloc(ptr, size)))
		Sys_Error("Not enough free memory");

	return p;
}

char *Q_strdup(const char *str)
{
	char *p;

	if (!(p = strdup(str)))
		Sys_Error("Not enough free memory");

	return p;
}

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

typedef struct
{
	int sentinal;
	int size;		// including sizeof(hunk_t), -1 = not allocated
	char name[8];
} hunk_t;

byte *hunk_base;
int hunk_size;

int hunk_low_used;
int hunk_high_used;

bool hunk_tempactive;
int hunk_tempmark;

/*
 ==============
 Hunk_Check

 Run consistency and sentinal trashing checks
 ==============
 */
void Hunk_Check(void)
{
	hunk_t *h;

	for (h = (hunk_t *) hunk_base; (byte *) h != hunk_base + hunk_low_used;)
	{
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("trashed sentinal");
		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error("bad size");
		h = (hunk_t *) ((byte *) h + h->size);
	}
}

void *Hunk_AllocName(int size, const char *name)
{
	hunk_t *h;

#ifdef PARANOID
	Hunk_Check ();
#endif

	if (size < 0)
		Sys_Error("bad size: %i", size);

	size = sizeof(hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error("Not enough RAM allocated.  Try using \"-mem 64\" on the command line.");
//		Sys_Error ("failed on %i bytes",size);

	h = (hunk_t *) (hunk_base + hunk_low_used);
	hunk_low_used += size;

	memset(h, 0, size);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy(h->name, name, 8);

	return (void *) (h + 1);
}

void *Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

int Hunk_LowMark(void)
{
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark)
{
//	Con_Printf("Freeing hunk from %d down to %d, diff: %d\n", mark, hunk_low_used, mark - hunk_low_used);
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error("bad mark %i", mark);
	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

void Memory_Init(size_t size)
{
	hunk_base = (byte *)Q_malloc(size);
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;
}
