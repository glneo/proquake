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

model_t *loadmodel;
char loadname[32];	// for hunk tags

void Mod_LoadAliasModel(model_t *mod, void *buffer);
void Mod_LoadSpriteModel(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer);
model_t *Mod_LoadModel(model_t *mod, bool crash);

byte mod_novis[MAX_MAP_LEAFS / 8];

#define	MAX_MOD_KNOWN	512
model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;

cvar_t gl_subdivide_size = { "gl_subdivide_size", "128", true };

void Mod_Init(void)
{
	Cvar_RegisterVariable(&gl_subdivide_size, NULL);
	memset(mod_novis, 0xff, sizeof(mod_novis));
}

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

byte *Mod_DecompressVis(byte *in, brush_model_t *model)
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
	int i;
	model_t *mod;
	static bool NoFree, Done;
	extern void GL_FreeTextures(void);

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (mod->type != mod_alias)
			mod->needload = true;
	}

	if (!Done)
	{
		// Some 3dfx miniGLs don't support glDeleteTextures (i.e. do nothing)
		NoFree = isDedicated || COM_CheckParm("-nofreetex");
		Done = true;
	}

	if (!NoFree)
		GL_FreeTextures();
}

model_t *Mod_FindName(char *name)
{
	int i;
	model_t *mod;

	if (!name[0])
		Sys_Error("Mod_FindName: NULL name");

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

/*
 ==================
 Mod_LoadModel

 Loads a model into the cache
 ==================
 */
model_t *Mod_LoadModel(model_t *mod, bool crash)
{
	void *buf;
	byte stackbuf[1024]; // avoid dirtying the cache heap
	unsigned int header_magic;

	if (!mod->needload)
		return mod;

	// load the file
	buf = (void *) COM_LoadStackFile(mod->name, stackbuf, sizeof(stackbuf));
	if (!buf)
	{
		if (crash)
			Sys_Error("Mod_LoadModel: %s not found", mod->name);

		return NULL;
	}

	// allocate a new model
	COM_FileBase(mod->name, loadname);

	loadmodel = mod;

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

/*
 ==================
 Mod_ForName

 Loads in a model for the given name
 ==================
 */
model_t *Mod_ForName(char *name, bool crash)
{
	model_t *mod;

	mod = Mod_FindName(name);

	return Mod_LoadModel(mod, crash);
}

void Mod_TouchModel(char *name)
{
	model_t *mod;

	mod = Mod_FindName(name);
}

//=============================================================================

void Mod_Print(void)
{
	int i;
	model_t *mod;

	Con_Printf("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		Con_Printf("%8p : %s\n", mod->aliasmodel, mod->name);
}
