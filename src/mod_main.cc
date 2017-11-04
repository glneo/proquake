/*
 * Common Model Functions
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

// models are the only shared resource between a client and server running
// on the same machine.
#include "quakedef.h"

void Mod_LoadAliasModel(model_t *mod, void *buffer);
void Mod_LoadSpriteModel(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer);

byte mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;

cvar_t gl_subdivide_size = { "gl_subdivide_size", "128", true };

texture_t *r_notexture_mip;
texture_t *r_notexture_mip2;

mleaf_t *Mod_PointInLeaf(vec3_t p, brush_model_t *model)
{
	float d;
	mnode_t *node;
	mplane_t *plane;

	if (!model || !model->nodes)
		Sys_Error("bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *) node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		node = (d > 0) ? node->children[0] : node->children[1];
	}

	return NULL;	// never reached
}

static byte *Mod_DecompressVis(byte *in, brush_model_t *model)
{
	int c, row;
	byte *out;
	static byte decompressed[MAX_MAP_LEAFS / 8];

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

byte *Mod_LeafPVS(mleaf_t *leaf, brush_model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;

	return Mod_DecompressVis(leaf->compressed_vis, model);
}

void Mod_ClearAll(void)
{
	for (int i = 0; i < mod_numknown; i++)
		if (mod_known[i].type != mod_alias)
			mod_known[i].needload = true;
}

model_t *Mod_FindName(char *name)
{
	int i;
	model_t *mod;

	if (!name[0])
		Sys_Error("NULL name");

	// search the currently loaded models
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!strcmp(mod->name, name))
			break;
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error("mod_numknown == MAX_MOD_KNOWN");

		mod_numknown++;
		strcpy(mod->name, name);
		mod->needload = true;
	}

	return mod;
}

/* Loads a model */
model_t *Mod_LoadModel(model_t *mod)
{
	void *buf;
	byte stackbuf[1024]; // avoid dirtying the cache heap
	unsigned int header_magic;

	if (!mod->needload)
		return mod;

	// allocate a new model

	// load the file
	buf = (void *) COM_LoadStackFile(mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
		return NULL;

	mod->needload = false;

	// call the appropriate loader
	header_magic = LittleLong(*(uint32_t *) buf);
	switch (header_magic)
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel(mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf);
		break;

	default:
		Mod_LoadBrushModel(mod, buf);
		break;
	}

	return mod;
}

/* Loads in a model for the given name */
model_t *Mod_ForName(char *name)
{
	model_t *mod;

	mod = Mod_FindName(name);

	return Mod_LoadModel(mod);
}

void Mod_TouchModel(char *name)
{
	Mod_FindName(name);
}

static void Mod_Print(void)
{
	static char *model_types[] = {
		[mod_brush] = "Brush",
		[mod_sprite] = "Sprite",
		[mod_alias] = "Alias",
	};

	Con_Printf("Cached models:\n");
	for (int i = 0; i < mod_numknown; i++)
		Con_Printf("%s: %s\t%s\n", mod_known[i].name, model_types[mod_known[i].type], mod_known[i].needload ? "Not Loaded" : "Loaded");
}

void Mod_Init(void)
{
	Cvar_RegisterVariable (&gl_subdivide_size);
	Cmd_AddCommand("mcache", Mod_Print);

	memset (mod_novis, 0xff, sizeof(mod_novis));

	r_notexture_mip = (texture_t *) Hunk_AllocName (sizeof(texture_t), "r_notexture_mip");
	strcpy (r_notexture_mip->name, "notexture");
	r_notexture_mip->height = r_notexture_mip->width = 32;

	r_notexture_mip2 = (texture_t *) Hunk_AllocName (sizeof(texture_t), "r_notexture_mip2");
	strcpy (r_notexture_mip2->name, "notexture2");
	r_notexture_mip2->height = r_notexture_mip2->width = 32;
}
