/*
 * Brush model loading
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

#include "quakedef.h"

#define ISTURBTEX(name) ((name)[0] == '*')
#define ISSKYTEX(name) ((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

static void Mod_LoadEntities(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->entities = NULL;
		return;
	}
	brushmodel->entities = Q_malloc(l->filelen);
	memcpy(brushmodel->entities, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadPlanes(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dplane_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mplane_t *out = Q_malloc(count * 2 * sizeof(*out));

	brushmodel->planes = out;
	brushmodel->numplanes = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		int bits = 0;
		for (int j = 0; j < 3; j++)
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

static void Mod_LoadTextures(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->textures = NULL;
		return;
	}

	dmiptexlump_t *miptexlump = (dmiptexlump_t *) (mod_base + l->fileofs);
	int nummiptex = LittleLong(miptexlump->nummiptex);

	brushmodel->numtextures = nummiptex;
	brushmodel->textures = Q_malloc(nummiptex * sizeof(*brushmodel->textures));

	for (int i = 0; i < nummiptex; i++)
	{
		int dataofs = LittleLong(miptexlump->dataofs[i]);
		if (dataofs == -1)
			continue;

		miptex_t *mt = (miptex_t *) ((byte *) miptexlump + dataofs);
		unsigned int width = LittleLong(mt->width);
		unsigned int height = LittleLong(mt->height);

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32 && CRC_Block((byte *) (mt + 1), mt->width * mt->height) == 65393)
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy(mt + 1, (byte *) (mt + 1) + 32 * 31, 32);
		}

		if ((width & 15) || (height & 15))
			Sys_Error("Texture %s's dimensions are not multiples of 16", mt->name);

		unsigned int pixels = width * height / 64 * 85;
		texture_t *tx = Q_calloc(sizeof(*tx), pixels);

		memcpy(tx->name, mt->name, sizeof(tx->name));
		tx->width = width;
		tx->height = height;
		for (int j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = LittleLong(mt->offsets[j]) + sizeof(texture_t) - sizeof(miptex_t);

		// the pixels immediately follow the structures
		memcpy(tx + 1, mt + 1, pixels);

		// If world model and sky texture and q1 bsp and not dedicated ...
		if (brushmodel->isworldmodel && ISSKYTEX(tx->name))
			R_InitSky(tx, (byte *) mt + tx->offsets[0]);

		// If world model and NOT sky texture
		int texture_flag = 0;
		if (brushmodel->isworldmodel && !ISSKYTEX(tx->name))	//R00k
			texture_flag |= TEX_WORLD;

		tx->gl_texturenum = GL_LoadTexture(mt->name, tx->width, tx->height, (byte *) (tx + 1), texture_flag | TEX_MIPMAP);

		tx->fullbright = -1; // because 0 is a potentially valid texture number
		// check for fullbright pixels in the texture - only if it ain't liquid, etc also

		if (!ISTURBTEX(tx->name) && FindFullbrightTexture((byte *) (tx + 1), pixels))
		{
			// convert any non fullbright pixel to fully transparent
			ConvertPixels((byte *) (tx + 1), pixels);

			char fbr_mask_name[64];

			// get a new name for the fullbright mask to avoid cache mismatches
			snprintf(fbr_mask_name, sizeof(fbr_mask_name), "fullbright_mask_%s", mt->name);

			// load the fullbright pixels version of the texture
			tx->fullbright = GL_LoadTexture(fbr_mask_name, tx->width, tx->height, (byte *) (tx + 1), texture_flag | TEX_MIPMAP | TEX_ALPHA);
		}

		brushmodel->textures[i] = tx;
	}

	texture_t *tx, *tx2;
	texture_t *anims[10];
	texture_t *altanims[10];
	int num, maxanim, altmax;
	//
	// sequence the animations
	//
	for (int i = 0; i < nummiptex; i++)
	{
		tx = brushmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		maxanim = tx->name[1];
		altmax = 0;
		if (maxanim >= 'a' && maxanim <= 'z')
			maxanim -= 'a' - 'A';
		if (maxanim >= '0' && maxanim <= '9')
		{
			maxanim -= '0';
			altmax = 0;
			anims[maxanim] = tx;
			maxanim++;
		}
		else if (maxanim >= 'A' && maxanim <= 'J')
		{
			altmax = maxanim - 'A';
			maxanim = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error("Bad animating texture %s", tx->name);

		for (int j = i + 1; j < nummiptex; j++)
		{
			tx2 = brushmodel->textures[j];
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
				if (num + 1 > maxanim)
					maxanim = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num + 1 > altmax)
					altmax = num + 1;
			}
			else
				Sys_Error("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (int j = 0; j < maxanim; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = maxanim * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j + 1) % maxanim];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (int j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error("Missing frame %i of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j + 1) % altmax];
			if (maxanim)
				tx2->alternate_anims = anims[0];
		}
	}
}

static void Mod_LoadVertexes(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dvertex_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mvertex_t *out = Q_malloc(sizeof(*out) * count);

	brushmodel->vertexes = out;
	brushmodel->numvertexes = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static void Mod_LoadVisibility(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->visdata = NULL;
		return;
	}
	brushmodel->visdata = Q_malloc(l->filelen);
	memcpy(brushmodel->visdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadTexinfo(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	texinfo_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mtexinfo_t *out = Q_malloc(count * sizeof(*out));

	brushmodel->texinfo = out;
	brushmodel->numtexinfo = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int j = 0; j < 2; j++)
			for (int k = 0; k < 4; k++)
				out->vecs[j][k] = LittleFloat(in->vecs[j][k]);

		float len1 = VectorLength(out->vecs[0]);
		float len2 = VectorLength(out->vecs[1]);
		len1 = (len1 + len2) / 2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		int miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		if (!brushmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= brushmodel->numtextures)
				Sys_Error("miptex >= brushmodel->numtextures");
			out->texture = brushmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

static void Mod_LoadLighting(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->lightdata = NULL;
		return;
	}

	brushmodel->lightdata = Q_malloc(l->filelen);
	memcpy(brushmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadSurfedges(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	int *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	int *out = Q_malloc(count * sizeof(*out));

	brushmodel->surfedges = out;
	brushmodel->numsurfedges = count;

	for (int i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

static void Mod_LoadEdges(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dedge_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	medge_t *out = Q_malloc((count + 1) * sizeof(*out));

	brushmodel->edges = out;
	brushmodel->numedges = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/*
 * Fills in s->texturemins[] and s->extents[]
 */
static void CalcSurfaceExtents(brush_model_t *brushmodel, msurface_t *s)
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
		e = brushmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &brushmodel->vertexes[brushmodel->edges[e].v[0]];
		else
			v = &brushmodel->vertexes[brushmodel->edges[-e].v[1]];

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

static void Mod_LoadSurfaces(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dface_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	msurface_t *out = Q_malloc(count * sizeof(*out));

	brushmodel->surfaces = out;
	brushmodel->numsurfaces = count;

	for (int surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		int planenum = LittleShort(in->planenum);
		if (LittleShort(in->side))
			out->flags |= SURF_PLANEBACK;

		out->plane = brushmodel->planes + planenum;
		out->texinfo = brushmodel->texinfo + LittleShort(in->texinfo);

		CalcSurfaceExtents(brushmodel, out);

		// lighting info
		for (int i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		int lightofs = LittleLong(in->lightofs);
		out->samples = (lightofs == -1) ? NULL : brushmodel->lightdata + lightofs;

		// set the drawing flags
		if (ISSKYTEX(out->texinfo->texture->name)) // sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface(brushmodel, out); // cut up polygon for warps
		}
		else if (ISTURBTEX(out->texinfo->texture->name)) // turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (int i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface(brushmodel, out); // cut up polygon for warps
		}
	}
}

static void Mod_LoadMarksurfaces(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	short *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	msurface_t **out = Q_malloc(count * sizeof(*out));

	brushmodel->marksurfaces = out;
	brushmodel->nummarksurfaces = count;

	for (int i = 0; i < count; i++)
	{
		int j = LittleShort(in[i]);
		if (j >= brushmodel->numsurfaces)
			Sys_Error("bad surface number");
		out[i] = brushmodel->surfaces + j;
	}
}

static void Mod_LoadLeafs(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dleaf_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mleaf_t *out = Q_malloc(count * sizeof(*out));

	brushmodel->leafs = out;
	brushmodel->numleafs = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		int p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = brushmodel->marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : brushmodel->visdata + p;
		out->efrags = NULL;

		for (int j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (int j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

static void Mod_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent(node->children[0], node);
	Mod_SetParent(node->children[1], node);
}

static void Mod_LoadNodes(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dnode_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mnode_t *out = Q_calloc(count, sizeof(*out));

	brushmodel->nodes = out;
	brushmodel->numnodes = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		out->contents = 0;

		for (int j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		int p = LittleLong(in->planenum);
		out->plane = brushmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (int j = 0; j < 2; j++)
		{
			p = LittleShort(in->children[j]);
			if (p >= 0)
				out->children[j] = brushmodel->nodes + p;
			else
				out->children[j] = (mnode_t *) (brushmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent(brushmodel->nodes, NULL);	// recursively sets nodes and leafs
}

static void Mod_LoadClipnodes(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	hull_t *hull;

	dclipnode_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	dclipnode_t *out = Q_malloc(count * sizeof(*out));

	brushmodel->clipnodes = out;
	brushmodel->numclipnodes = count;

	hull = &brushmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brushmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;
	hull->available = true;

	hull = &brushmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brushmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;
	hull->available = true;

	hull = &brushmodel->hulls[3];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brushmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -6;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 30;
	hull->available = false;

	for (int i = 0; i < count; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

static void Mod_LoadSubmodels(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dmodel_t *in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Sys_Error("model %s has invalid # of vertices: %d", mod_name, count);

	dmodel_t *out = Q_malloc(count * sizeof(*out));

	brushmodel->submodels = out;
	brushmodel->numsubmodels = count;

	for (int i = 0; i < count; i++, in++, out++)
	{
		for (int j = 0; j < 3; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}

		for (int j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);

		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

/*
 * Duplicate the drawing hull structure as a clipping hull
 */
void Mod_MakeHull0(brush_model_t *brushmodel)
{
	mnode_t *in = brushmodel->nodes;
	int count = brushmodel->numnodes;
	dclipnode_t *out = Q_malloc(count * sizeof(*out));

	hull_t *hull = &brushmodel->hulls[0];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brushmodel->planes;

	for (int i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->plane - brushmodel->planes;
		for (int j = 0; j < 2; j++)
		{
			mnode_t *child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - brushmodel->nodes;
		}
	}
}

void Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	dheader_t *header = (dheader_t *) buffer;
	char *mod_name = mod->name;

	brush_model_t *brushmodel = Q_malloc(sizeof(*brushmodel));

	brushmodel->bspversion = LittleLong(header->version);
	if (brushmodel->bspversion != BSPVERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod_name, brushmodel->bspversion, BSPVERSION);

	brushmodel->isworldmodel = !strcmp(mod_name, va("maps/%s.bsp", host_worldname));

	// swap all the lumps
	for (int i = 0; i < sizeof(dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	// load into heap
	Mod_LoadEntities(brushmodel, &header->lumps[LUMP_ENTITIES], (byte *) header, mod_name);
	Mod_LoadPlanes(brushmodel, &header->lumps[LUMP_PLANES], (byte *) header, mod_name);
	Mod_LoadTextures(brushmodel, &header->lumps[LUMP_TEXTURES], (byte *) header, mod_name);
	Mod_LoadVertexes(brushmodel, &header->lumps[LUMP_VERTEXES], (byte *) header, mod_name);
	Mod_LoadVisibility(brushmodel, &header->lumps[LUMP_VISIBILITY], (byte *) header, mod_name);
	Mod_LoadTexinfo(brushmodel, &header->lumps[LUMP_TEXINFO], (byte *) header, mod_name);
	Mod_LoadLighting(brushmodel, &header->lumps[LUMP_LIGHTING], (byte *) header, mod_name);
	Mod_LoadSurfedges(brushmodel, &header->lumps[LUMP_SURFEDGES], (byte *) header, mod_name);
	Mod_LoadEdges(brushmodel, &header->lumps[LUMP_EDGES], (byte *) header, mod_name);
	Mod_LoadSurfaces(brushmodel, &header->lumps[LUMP_FACES], (byte *) header, mod_name);
	Mod_LoadMarksurfaces(brushmodel, &header->lumps[LUMP_MARKSURFACES], (byte *) header, mod_name);
	Mod_LoadLeafs(brushmodel, &header->lumps[LUMP_LEAFS], (byte *) header, mod_name);
	Mod_LoadNodes(brushmodel, &header->lumps[LUMP_NODES], (byte *) header, mod_name);
	Mod_LoadClipnodes(brushmodel, &header->lumps[LUMP_CLIPNODES], (byte *) header, mod_name);
	Mod_LoadSubmodels(brushmodel, &header->lumps[LUMP_MODELS], (byte *) header, mod_name);

	Mod_MakeHull0(brushmodel);

	mod->numframes = 2; // regular and alternate animation

	mod->type = mod_brush;

	mod->brushmodel = brushmodel;

	// set up the submodels (FIXME: this is confusing)

	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
	for (int i = 0; i < brushmodel->numsubmodels; i++)
	{
		dmodel_t *bm = &brushmodel->submodels[i];

		brushmodel->hulls[0].firstclipnode = bm->headnode[0];
		for (int j = 1; j < MAX_MAP_HULLS; j++)
		{
			brushmodel->hulls[j].firstclipnode = bm->headnode[j];
			brushmodel->hulls[j].lastclipnode = brushmodel->numclipnodes - 1;
		}

		brushmodel->firstmodelsurface = bm->firstface;
		brushmodel->nummodelsurfaces = bm->numfaces;

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);

		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

		brushmodel->numleafs = bm->visleafs;

		if (i < brushmodel->numsubmodels - 1)
		{	// duplicate the basic information
			char name[10];

			snprintf(name, sizeof(name), "*%i", i + 1);
			model_t *loadmodel = Mod_FindName(name);
			*loadmodel = *mod;
			loadmodel->brushmodel = Q_malloc(sizeof(*brushmodel));
			*loadmodel->brushmodel = *brushmodel;
			brushmodel = loadmodel->brushmodel;
			strcpy(loadmodel->name, name);
			mod = loadmodel;
		}
	}
}
