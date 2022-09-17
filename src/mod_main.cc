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
#include "gl_texmgr.h"

#include "modelgen.h"
#include "spritegen.h"

void Mod_LoadAliasModel(model_t *mod, void *buffer);
void Mod_LoadSpriteModel(model_t *mod, void *buffer);
void Mod_LoadBrushModel(model_t *mod, void *buffer);

static byte *mod_novis;
static int mod_novis_capacity;

#define	MAX_MOD_KNOWN 2048
static model_t mod_known[MAX_MOD_KNOWN];
static int mod_numknown;

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
		node = (d > 0) ? node->left_node : node->right_node;
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
		return Mod_NoVisPVS(model);

	return Mod_DecompressVis(leaf->compressed_vis, model);
}

byte *Mod_NoVisPVS(brush_model_t *model)
{
	int pvsbytes;

	pvsbytes = (model->numleafs + 7) >> 3;
	if (mod_novis == NULL || pvsbytes > mod_novis_capacity)
	{
		mod_novis_capacity = pvsbytes;
		mod_novis = (byte *) Q_realloc(mod_novis, mod_novis_capacity);
		memset(mod_novis, 0xff, mod_novis_capacity);
	}

	return mod_novis;
}

/*
 =============================================================================

 The PVS must include a small area around the client to allow head bobbing
 or other small motion on the client side.  Otherwise, a bob might cause an
 entity that should be visible to not show up, especially when the bob
 crosses a waterline.

 =============================================================================
 */

int fatbytes;
byte fatpvs[MAX_MAP_LEAFS / 8];
static void Mod_AddToFatPVS(vec3_t org, mnode_t *node, brush_model_t *brushmodel)
{
	int i;
	byte *pvs;
	mplane_t *plane;
	float d;

	while (1) {
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0) {
			if (node->contents != CONTENTS_SOLID) {
				pvs = Mod_LeafPVS((mleaf_t *) node, brushmodel);
				for (i = 0; i < fatbytes; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->left_node;
		else if (d < -8)
			node = node->right_node;
		else {	// go down both
			Mod_AddToFatPVS(org, node->left_node, brushmodel);
			node = node->right_node;
		}
	}
}

/*
 * Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
 * given point.
 */
byte *Mod_FatPVS(vec3_t org, brush_model_t *brushmodel)
{
	fatbytes = (brushmodel->numleafs + 31) >> 3;
	memset(fatpvs, 0, fatbytes);
	Mod_AddToFatPVS(org, brushmodel->nodes, brushmodel);
	return fatpvs;
}

void Mod_ClearAll(void)
{
	for (int i = 0; i < mod_numknown; i++)
		if (mod_known[i].type != mod_alias)
			mod_known[i].needload = true;
}

/* Loads a model */
static model_t *Mod_LoadModel(model_t *mod)
{
	void *buf;
	unsigned int header_magic;

	if (!mod->needload)
		return mod;

	// allocate a new model

	// load the file
	buf = (void *)COM_LoadMallocFile(mod->name);
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

	free(buf);

	return mod;
}

model_t *Mod_FindName(const char *name)
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

/* Loads in a model for the given name */
model_t *Mod_ForName(const char *name)
{
	model_t *mod;

	mod = Mod_FindName(name);

	return Mod_LoadModel(mod);
}

void Mod_TouchModel(const char *name)
{
	Mod_FindName(name);
}

static void Mod_Print(void)
{
	static const char *model_types[] = {
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
	Cmd_AddCommand("mcache", Mod_Print);
}
