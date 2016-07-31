/*
 * Model loading and caching
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
#include "gl_fullbright.h"

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

/*
 ===============
 Mod_Init
 ===============
 */
void Mod_Init(void)
{
	Cvar_RegisterVariable(&gl_subdivide_size, NULL);
	memset(mod_novis, 0xff, sizeof(mod_novis));
}

void *Mod_Extradata(model_t *mod)
{
	return mod->cache.data;
}

/*
 ===============
 Mod_PointInLeaf
 ===============
 */
mleaf_t *Mod_PointInLeaf(vec3_t p, model_t *model)
{
	float d;
	mnode_t *node;
	mplane_t *plane;

	if (!model || !model->nodes)
		Sys_Error("Mod_PointInLeaf: bad model");

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

/*
 ===================
 Mod_DecompressVis
 ===================
 */
byte *Mod_DecompressVis(byte *in, model_t *model)
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

byte *Mod_LeafPVS(mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;

	return Mod_DecompressVis(leaf->compressed_vis, model);
}

/*
 ===================
 Mod_ClearAll
 ===================
 */
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

/*
 ==================
 Mod_FindName
 ==================
 */
model_t *Mod_FindName(char *name)
{
	int i;
	model_t *mod;

	printf("***** %s\n", name);

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

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
 ===============================================================================

 BRUSHMODEL LOADING

 ===============================================================================
 */

#define ISTURBTEX(name)		((name)[0] == '*')
#define ISSKYTEX(name)		((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

byte *mod_base;

/*
 =================
 Mod_LoadTextures
 =================
 */
void Mod_LoadTextures(lump_t *l)
{
	int i, j, pixels, num, max, altmax, texture_flag;
	miptex_t *mt;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
#ifndef NOFULLBRIGHT
	char fbr_mask_name[64];
#endif
	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *) (mod_base + l->fileofs);
	m->nummiptex = LittleLong(m->nummiptex);
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName(m->nummiptex * sizeof(*loadmodel->textures), loadname);

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *) ((byte *) m + m->dataofs[i]);
		mt->width = LittleLong(mt->width);
		mt->height = LittleLong(mt->height);
		for (j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong(mt->offsets[j]);

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32 && CRC_Block((byte *) (mt + 1), mt->width * mt->height) == 65393)
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy(mt + 1, (byte *) (mt + 1) + 32 * 31, 32);
		}

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error("Texture %s is not 16 aligned", mt->name);

		pixels = mt->width * mt->height / 64 * 85;
		loadmodel->textures[i] = tx = Hunk_AllocName(sizeof(texture_t) + pixels, loadname);

		memcpy(tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;

		for (j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);

		// the pixels immediately follow the structures
		memcpy(tx + 1, mt + 1, pixels);

		if (cls.state != ca_dedicated)	// Dedicated skips this whole ordeal
		{

			// If world model and sky texture and q1 bsp and not dedicated ...
			if (loadmodel->isworldmodel && ISSKYTEX(tx->name) && loadmodel->bspversion == BSPVERSION)
				R_InitSky(tx);

			// If world model and NOT sky texture
			if (loadmodel->isworldmodel && !ISSKYTEX(tx->name))	//R00k
				texture_flag |= TEX_WORLD;

			if (loadmodel->bspversion == BSPVERSION)
			{
				texture_mode = GL_LINEAR_MIPMAP_NEAREST; //_LINEAR;
				tx->gl_texturenum = GL_LoadTexture(mt->name, tx->width, tx->height, (byte *) (tx + 1), texture_flag | TEX_MIPMAP);
				texture_mode = GL_LINEAR;
			}

			tx->fullbright = -1; // because 0 is a potentially valid texture number
			// check for fullbright pixels in the texture - only if it ain't liquid, etc also

			if (loadmodel->bspversion == BSPVERSION && !ISTURBTEX(tx->name) && FindFullbrightTexture((byte *) (tx + 1), pixels))
			{
				// convert any non fullbright pixel to fully transparent
				ConvertPixels((byte *) (tx + 1), pixels);

				// get a new name for the fullbright mask to avoid cache mismatches
				snprintf(fbr_mask_name, sizeof(fbr_mask_name), "fullbright_mask_%s", mt->name);

				// load the fullbright pixels version of the texture
				tx->fullbright = GL_LoadTexture(fbr_mask_name, tx->width, tx->height, (byte *) (tx + 1), texture_flag | TEX_MIPMAP | TEX_ALPHA);
			}

		}

	} // End !dedicated
// END LOOP

	// sequence the animations
	for (i = 0; i < m->nummiptex; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';

		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
		{
			Sys_Error("Bad animating texture %s", tx->name);
		}

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp(tx2->name + 2, tx->name + 2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num + 1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			}
			else
			{
				Sys_Error("Bad animating texture %s", tx->name);
			}
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % max];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}

		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error("Mod_LoadTextures: Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

/*
 =================
 Mod_LoadLighting
 =================
 */
void Mod_LoadLighting(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = Hunk_AllocName(l->filelen, loadname);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

/*
 =================
 Mod_LoadVisibility
 =================
 */
void Mod_LoadVisibility(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName(l->filelen, loadname);
	memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

/*
 =================
 Mod_LoadEntities
 =================
 */
void Mod_LoadEntities(lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName(l->filelen, loadname);
	memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);
}

vec3_t worldmins; // Baker: get world bounds
vec3_t worldmaxs;

/*
 =================
 Mod_LoadVertexes
 =================
 */
void Mod_LoadVertexes(lump_t *l, bool storebounds)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	if (storebounds)
	{
		worldmins[0] = 99999999;
		worldmins[1] = 99999999;
		worldmins[2] = 99999999;
		worldmaxs[0] = -99999999;
		worldmaxs[1] = -99999999;
		worldmaxs[2] = -99999999;
	}

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadVertexes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);

		if (out->position[0] < worldmins[0])
			worldmins[0] = out->position[0];
		if (out->position[1] < worldmins[1])
			worldmins[1] = out->position[1];
		if (out->position[2] < worldmins[2])
			worldmins[2] = out->position[2];
		if (out->position[0] > worldmaxs[0])
			worldmaxs[0] = out->position[0];
		if (out->position[1] > worldmaxs[1])
			worldmaxs[1] = out->position[1];
		if (out->position[2] > worldmaxs[2])
			worldmaxs[2] = out->position[2];

	}
}

/*
 =================
 Mod_LoadSubmodels
 =================
 */
void Mod_LoadSubmodels(lump_t *l)
{
	dmodel_t *in, *out;
	int i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Sys_Error("Mod_LoadSubmodels: count > MAX_MODELS");

	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);
		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

/*
 =================
 Mod_LoadEdges
 =================
 */
void Mod_LoadEdges(lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadEdges: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName((count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/*
 =================
 Mod_LoadTexinfo
 =================
 */
void Mod_LoadTexinfo(lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, j, k, count, miptex;
	float len1, len2;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (k = 0; k < 2; k++)
			for (j = 0; j < 4; j++)
				out->vecs[k][j] = LittleFloat(in->vecs[k][j]);
		len1 = VectorLength(out->vecs[0]);
		len2 = VectorLength(out->vecs[1]);
		len1 = (len1 + len2) / 2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error("Mod_LoadTexinfo: miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

/*
 ================
 CalcSurfaceExtents

 Fills in s->texturemins[] and s->extents[]
 ================
 */
void CalcSurfaceExtents(msurface_t *s)
{
	float mins[2], maxs[2], val;
	int i, j, e, bmins[2], bmaxs[2];
	mvertex_t *v;
	mtexinfo_t *tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++)
	{
		e = loadmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j = 0; j < 2; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + v->position[1] * tex->vecs[j][1] + v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{
		bmins[i] = floor(mins[i] / 16);
		bmaxs[i] = ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Host_Error("CalcSurfaceExtents: Bad surface extents");
	}
}

/*
 =================
 Mod_LoadFaces
 =================
 */
void Mod_LoadFaces(lump_t *l)
{
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum, planenum, side;
#ifdef SUPPORTS_GL_OVERBRIGHTS
	extern cvar_t gl_overbright;
#endif
	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadFaces: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		if ((side = LittleShort(in->side)))
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleShort(in->texinfo);

		CalcSurfaceExtents(out);

		// lighting info
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		out->samples = (i == -1) ? NULL : loadmodel->lightdata + i;

#ifdef SUPPORTS_GL_OVERBRIGHTS
		// mh - overbrights
		out->overbright = gl_overbright.value;
#endif

		// set the drawing flags flag
		if (ISSKYTEX(out->texinfo->texture->name))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface(out);	// cut up polygon for warps
			continue;
		}

		if (ISTURBTEX(out->texinfo->texture->name))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface(out);	// cut up polygon for warps
			continue;
		}
	}
}

/*
 =================
 Mod_SetParent
 =================
 */
void Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

/*
 =================
 Mod_LoadNodes
 =================
 */
void Mod_LoadNodes(lump_t *l)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent(loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
 =================
 Mod_LoadLeafs
 =================
 */
void Mod_LoadLeafs(lump_t *l)
{
	dleaf_t *in;
	mleaf_t *out;
	int i, j, count, p;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : loadmodel->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

/*
 =================
 Mod_LoadClipnodes
 =================
 */
void Mod_LoadClipnodes(lump_t *l)
{
	dclipnode_t *in, *out;
	int i, count;
	hull_t *hull;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadClipnodes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;
	hull->available = true;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;
	hull->available = true;

	hull = &loadmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -6;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 30;
	hull->available = false;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

/*
 =================
 Mod_MakeHull0

 Deplicate the drawing hull structure as a clipping hull
 =================
 */
void Mod_MakeHull0(void)
{
	mnode_t *in, *child;
	dclipnode_t *out;
	int i, j, count;
	hull_t *hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = loadmodel->planes;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j = 0; j < 2; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
 =================
 Mod_LoadMarksurfaces
 =================
 */
void Mod_LoadMarksurfaces(lump_t *l)
{
	int i, j, count;
	short *in;
	msurface_t **out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
 =================
 Mod_LoadSurfedges
 =================
 */
void Mod_LoadSurfedges(lump_t *l)
{
	int i, count, *in, *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/*
 =================
 Mod_LoadPlanes
 =================
 */
void Mod_LoadPlanes(lump_t *l)
{
	int i, j, count, bits;
	mplane_t *out;
	dplane_t *in;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("Mod_LoadPlanes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName(count * 2 * sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;
		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat(in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}

/*
 =================
 RadiusFromBounds
 =================
 */
float RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength(corner);
}

/*
 =================
 Mod_LoadBrushModel
 =================
 */
void Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	int i, j;
	dheader_t *header;
	dmodel_t *bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *) buffer;

	mod->bspversion = LittleLong(header->version);

	if (mod->bspversion != BSPVERSION)
		Host_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, mod->bspversion, BSPVERSION);

	loadmodel->isworldmodel = !strcmp(loadmodel->name, va("maps/%s.bsp", host_worldname));

// swap all the lumps
	mod_base = (byte *) header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

// load into heap
	Mod_LoadVertexes(&header->lumps[LUMP_VERTEXES], loadmodel->isworldmodel ? 1 : 0);
	if (loadmodel->isworldmodel)
		Con_DPrintf("World bounds: %4.1f %4.1f %4.1f to %4.1f %4.1f %4.1f\n", worldmins[0], worldmins[1], worldmins[2], worldmaxs[0], worldmaxs[1],
				worldmaxs[2]);

	Mod_LoadEdges(&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures(&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces(&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces(&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes(&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities(&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels(&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0();

	mod->numframes = 2;		// regular and alternate animation

// set up the submodels (FIXME: this is confusing)

	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
	for (i = 0; i < mod->numsubmodels; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j = 1; j < MAX_MAP_HULLS; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels - 1)
		{	// duplicate the basic information
			char name[10];

			snprintf(name, sizeof(name), "*%i", i + 1);
			loadmodel = Mod_FindName(name);
			*loadmodel = *mod;
			strcpy(loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

//=============================================================================

/*
 =================
 Mod_LoadSpriteFrame
 =================
 */
void * Mod_LoadSpriteFrame(void * pin, mspriteframe_t **ppframe, int framenum)
{
	int width, height, size, origin[2];
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	char name[64];

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong(pinframe->width);
	height = LittleLong(pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName(sizeof(mspriteframe_t), loadname);
	memset(pspriteframe, 0, sizeof(mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf(name, sizeof(name), "%s_%i", loadmodel->name, framenum);
	pspriteframe->gl_texturenum = GL_LoadTexture(name, width, height, (byte *) (pinframe + 1), TEX_MIPMAP | TEX_ALPHA);

	return (void *) ((byte *) pinframe + sizeof(dspriteframe_t) + size);
}

/*
 =================
 Mod_LoadSpriteGroup
 =================
 */
void * Mod_LoadSpriteGroup(void * pin, mspriteframe_t **ppframe, int framenum)
{
	int i, numframes;
	float *poutintervals;
	void *ptemp;
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	dspriteinterval_t *pin_intervals;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong(pingroup->numframes);

	pspritegroup = Hunk_AllocName(sizeof(mspritegroup_t) + (numframes - 1) * sizeof(pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = Hunk_AllocName(numframes * sizeof(float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error("Mod_LoadSpriteGroup: interval %f <= 0 in %s", *poutintervals, loadmodel->name);

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}

/*
 =================
 Mod_LoadSpriteModel
 =================
 */
void Mod_LoadSpriteModel(model_t *mod, void *buffer)
{
	int i, numframes, size;
	int version;
	dsprite_t *pin;
	msprite_t *psprite;
	dspriteframetype_t *pframetype;

	pin = (dsprite_t *) buffer;

	version = LittleLong(pin->version);
	if (version != SPRITE_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong(pin->numframes);

	size = sizeof(msprite_t) + (numframes - 1) * sizeof(psprite->frames);

	psprite = Hunk_AllocName(size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong(pin->type);
	psprite->maxwidth = LittleLong(pin->width);
	psprite->maxheight = LittleLong(pin->height);
	psprite->beamlength = LittleFloat(pin->beamlength);
	mod->synctype = LittleLong(pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

	// load the frames
	if (numframes < 1)
		Sys_Error("Mod_LoadSpriteModel: invalid # of frames %d in %s", numframes, mod->name);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *) (pin + 1);

	for (i = 0; i < numframes; i++)
	{
		spriteframetype_t frametype;

		frametype = LittleLong(pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteFrame(pframetype + 1, &psprite->frames[i].frameptr, i);
		}
		else
		{
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteGroup(pframetype + 1, &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
 ================
 Mod_Print
 ================
 */
void Mod_Print(void)
{
	int i;
	model_t *mod;

	Con_Printf("Cached models:\n");
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
		Con_Printf("%8p : %s\n", mod->cache.data, mod->name);
}
