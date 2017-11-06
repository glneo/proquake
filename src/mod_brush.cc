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
#include "glquake.h"

#define ISTURBTEX(name) ((name)[0] == '*')
#define ISSKYTEX(name) ((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

static void Mod_LoadEntities(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->entities = NULL;
		return;
	}
	brushmodel->entities = (char *)Q_malloc(l->filelen);
	memcpy(brushmodel->entities, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadPlanes(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dplane_t *in = (dplane_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mplane_t *out = (mplane_t *)Q_calloc(count * 2, sizeof(*out));

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

bool Mod_CheckFullbrights (byte *pixels, int count)
{
	for (int i = 0; i < count; i++)
		if (*pixels++ > 223)
			return true;
	return false;
}

void Sky_LoadTexture (texture_t *mt);
void Mod_LoadTextures(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t	*m;
//johnfitz -- more variables
	char		texturename[64];
	int			nummiptex;
//johnfitz

	//johnfitz -- don't return early if no textures; still need to create dummy texture
	if (!l->filelen)
	{
		Con_Printf ("Mod_LoadTextures: no textures in bsp file\n");
		nummiptex = 0;
		m = NULL; // avoid bogus compiler warning
	}
	else
	{
		m = (dmiptexlump_t *)(mod_base + l->fileofs);
		m->nummiptex = LittleLong (m->nummiptex);
		nummiptex = m->nummiptex;
	}
	//johnfitz

	brushmodel->numtextures = nummiptex + 2; //johnfitz -- need 2 dummy texture chains for missing textures
	brushmodel->textures = (texture_t **) Hunk_AllocName (brushmodel->numtextures * sizeof(*brushmodel->textures) , mod_name);

	for (int i=0 ; i<nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		miptex_t	*mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (int j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ( (mt->width & 15) || (mt->height & 15) )
			Sys_Error ("Texture %s is not 16 aligned", mt->name);
		int pixels = mt->width*mt->height/64*85;
		tx = (texture_t *) Hunk_AllocName (sizeof(texture_t) +pixels, mod_name );
		brushmodel->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;
		for (int j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures

		// ericw -- check for pixels extending past the end of the lump.
		// appears in the wild; e.g. jam2_tronyn.bsp (func_mapjam2),
		// kellbase1.bsp (quoth), and can lead to a segfault if we read past
		// the end of the .bsp file buffer
		if (((byte*)(mt+1) + pixels) > (mod_base + l->fileofs + l->filelen))
		{
			Con_DPrintf("Texture %s extends past end of lump\n", mt->name);
			pixels = max(0, (mod_base + l->fileofs + l->filelen) - (byte*)(mt+1));
		}
		memcpy ( tx+1, mt+1, pixels);

		tx->fullbright = NULL; //johnfitz

		//johnfitz -- lots of changes
		if (cls.state != ca_dedicated) //no texture uploading for dedicated server
		{
			if (!strncasecmp(tx->name,"sky",3)) //sky texture
				Sky_LoadTexture(tx);
			else if (tx->name[0] == '*') //warping texture
			{

					snprintf (texturename, sizeof(texturename), "%s:%s", mod_name, tx->name);
//					offset = (src_offset_t)(mt+1) - (src_offset_t)mod_base;
					tx->gltexture = TexMgr_LoadImage (texturename, tx->width, tx->height,
						SRC_INDEXED, (byte *)(tx+1), TEX_NOFLAGS);
			}
			else //regular texture
			{
				// ericw -- fence textures
				int	extraflags;

				extraflags = 0;
				if (tx->name[0] == '{')
					extraflags |= TEX_ALPHA;
				// ericw

				//external textures -- first look in "textures/mapname/" then look in "textures/"

					snprintf (texturename, sizeof(texturename), "%s:%s", mod_name, tx->name);
//					offset = (src_offset_t)(mt+1) - (src_offset_t)mod_base;
					if (Mod_CheckFullbrights ((byte *)(tx+1), pixels))
					{
						tx->gltexture = TexMgr_LoadImage (texturename, tx->width, tx->height,
							SRC_INDEXED, (byte *)(tx+1), TEX_MIPMAP | TEX_NOBRIGHT | extraflags);
						snprintf (texturename, sizeof(texturename), "%s:%s_glow", mod_name, tx->name);
						tx->fullbright = TexMgr_LoadImage (texturename, tx->width, tx->height,
							SRC_INDEXED, (byte *)(tx+1), TEX_MIPMAP | TEX_FULLBRIGHT | extraflags);
					}
					else
					{
						tx->gltexture = TexMgr_LoadImage (texturename, tx->width, tx->height,
							SRC_INDEXED, (byte *)(tx+1), TEX_MIPMAP | extraflags);
					}
			}
		}
		//johnfitz
	}

	//johnfitz -- last 2 slots in array should be filled with dummy textures
	brushmodel->textures[brushmodel->numtextures-2] = r_notexture_mip; //for lightmapped surfs
	brushmodel->textures[brushmodel->numtextures-1] = r_notexture_mip2; //for SURF_DRAWTILED surfs

//
// sequence the animations
//
	for (int i=0 ; i<nummiptex ; i++)
	{
		tx = brushmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		int maxanim = tx->name[1];
		int altmax = 0;
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
			Sys_Error ("Bad animating texture %s", tx->name);

		for (int j=i+1 ; j<nummiptex ; j++)
		{
			tx2 = brushmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			int num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > maxanim)
					maxanim = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Sys_Error ("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
	// link them all together
		for (int j=0 ; j<maxanim ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = maxanim * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%maxanim ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (int j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (maxanim)
				tx2->alternate_anims = anims[0];
		}
	}
}

static void Mod_LoadVertexes(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dvertex_t *in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mvertex_t *out = (mvertex_t *)Q_calloc(count, sizeof(*out));

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
	brushmodel->visdata = (byte *)Q_malloc(l->filelen);
	memcpy(brushmodel->visdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadTexinfo(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	texinfo_t *in = (texinfo_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mtexinfo_t *out = (mtexinfo_t *)Q_calloc(count, sizeof(*out));

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

	brushmodel->lightdata = (byte *)Q_malloc(l->filelen);
	memcpy(brushmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadSurfedges(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	int *in = (int *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	int *out = (int *)Q_calloc(count, sizeof(*out));

	brushmodel->surfedges = out;
	brushmodel->numsurfedges = count;

	for (int i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

static void Mod_LoadEdges(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dedge_t *in = (dedge_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	medge_t *out = (medge_t *)Q_calloc((count + 1), sizeof(*out));

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
			Sys_Error("CalcSurfaceExtents: Bad surface extents");
	}
}

static void Mod_LoadSurfaces(brush_model_t *brushmodel, lump_t *l, byte *mod_base, char *mod_name)
{
	dface_t *in = (dface_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	msurface_t *out = (msurface_t *)Q_calloc(count, sizeof(*out));

	brushmodel->surfaces = out;
	brushmodel->numsurfaces = count;

	for (int surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

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
	short *in = (short *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	msurface_t **out = (msurface_t **)Q_calloc(count, sizeof(*out));

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
	dleaf_t *in = (dleaf_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mleaf_t *out = (mleaf_t *)Q_calloc(count, sizeof(*out));

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
	dnode_t *in = (dnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mnode_t *out = (mnode_t *)Q_calloc(count, sizeof(*out));

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

	dclipnode_t *in = (dclipnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	dclipnode_t *out = (dclipnode_t *)Q_calloc(count, sizeof(*out));

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
	dmodel_t *in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Sys_Error("model %s has invalid # of vertices: %d", mod_name, count);

	dmodel_t *out = (dmodel_t *)Q_calloc(count, sizeof(*out));

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
	dclipnode_t *out = (dclipnode_t *)Q_calloc(count, sizeof(*out));

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

	brush_model_t *brushmodel = (brush_model_t *)Q_malloc(sizeof(*brushmodel));

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
			loadmodel->brushmodel = (brush_model_t *)Q_malloc(sizeof(*brushmodel));
			*loadmodel->brushmodel = *brushmodel;
			brushmodel = loadmodel->brushmodel;
			strcpy(loadmodel->name, name);
			mod = loadmodel;
		}
	}
}
