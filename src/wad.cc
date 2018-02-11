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

size_t wad_numlumps;
lumpinfo_t *wad_lumps;
byte *wad_base;

/* automatic byte swapping */
void SwapPic(qpic_t *pic)
{
	pic->width = LittleLong(pic->width);
	pic->height = LittleLong(pic->height);
}

/*
 * Lowercases name and pads with spaces and a terminating 0 to the length of
 * lumpinfo_t->name. Used so lumpname lookups can proceed rapidly by comparing
 * 4 chars at a time Space padding is so names can be printed nicely in tables.
 * Can safely be performed in place.
 */
static void W_CleanupName(const char *in, char *out)
{
	int i;

	for (i = 0; i < 16; i++)
	{
		int c = in[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c += ('a' - 'A');
		out[i] = c;
	}

	for (; i < 16; i++)
		out[i] = 0;
}

void W_LoadWadFile(const char *filename)
{
	if (!(wad_base = COM_LoadHunkFile(filename)))
		Sys_Error("couldn't load %s\n\n"
				"Game data files are required to run;"
				"usually this means you need Quake shareware or registered version.\n\n"
				"Are game files in the proper place?", filename);

	wadinfo_t *header = (wadinfo_t *) wad_base;

	if (memcmp(header->identification, "WAD2", 4))
		Sys_Error("Wad file %s doesn't have WAD2 id\n", filename);

	wad_numlumps = LittleLong(header->numlumps);
	int infotableofs = LittleLong(header->infotableofs);
	wad_lumps = (lumpinfo_t *) (wad_base + infotableofs);

	for (size_t i = 0; i < wad_numlumps; i++)
	{
		wad_lumps[i].filepos = LittleLong(wad_lumps[i].filepos);
		wad_lumps[i].size = LittleLong(wad_lumps[i].size);
		W_CleanupName((const char*)wad_lumps[i].name, (char*)wad_lumps[i].name);
		if (wad_lumps[i].type == TYP_QPIC)
			SwapPic((qpic_t *) (wad_base + wad_lumps[i].filepos));
	}
}

void *W_GetLumpName(const char *name)
{
	char clean[16];
	W_CleanupName(name, clean);

	for (size_t i = 0; i < wad_numlumps; i++)
	{
		if (!strcmp(clean, (char*)wad_lumps[i].name))
			return (void *) (wad_base + wad_lumps[i].filepos);
	}

	Sys_Error("%s not found", name);
	return NULL;
}
