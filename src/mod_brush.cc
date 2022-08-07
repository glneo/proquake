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

#include <cfloat>

#include "quakedef.h"
#include "glquake.h"

#include "bspfile.h"

#define ISTURBTEX(name) ((name)[0] == '*')
#define ISSKYTEX(name) ((name)[0] == 's' && \
                        (name)[1] == 'k' && \
			(name)[2] == 'y')

gltexture_t *solidskytexture;
gltexture_t *alphaskytexture;

static void Mod_LoadEntities(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->entities = NULL;
		return;
	}
	brushmodel->entities = (char *)Q_malloc(l->filelen);
	memcpy(brushmodel->entities, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadPlanes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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

/* A sky texture is 256*128, with the left side being a masked overlay */
static void Sky_LoadTexture(texture_t *mt)
{
	static byte front_data[128 * 128];
	static byte back_data[128 * 128];
	char texturename[64];

	byte *src = (byte *)mt + mt->offsets[0];

	// extract back layer and upload
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
			back_data[(i * 128) + j] = src[i * 256 + j + 128];

	snprintf(texturename, sizeof(texturename), "%s_back", mt->name);
	solidskytexture = TexMgr_LoadImage(texturename, 128, 128, SRC_INDEXED, back_data, TEX_NOFLAGS);

	// extract front layer and upload
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
		{
			front_data[(i * 128) + j] = src[i * 256 + j];
			if (front_data[(i * 128) + j] == 0)
				front_data[(i * 128) + j] = 255;
		}

	snprintf(texturename, sizeof(texturename), "%s_front", mt->name);
	alphaskytexture = TexMgr_LoadImage(texturename, 128, 128, SRC_INDEXED, front_data, TEX_ALPHA);

	mt->isskytexture = true;
}

void Mod_LoadTextures(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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
		unsigned int pixels = mt->width*mt->height/64*85;
		tx = (texture_t *) Hunk_AllocName (sizeof(texture_t) + pixels, mod_name );
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
			pixels = max(0, (int)((mod_base + l->fileofs + l->filelen) - (byte*)(mt+1)));
		}
		memcpy ( tx+1, mt+1, pixels);

		tx->fullbright = NULL; //johnfitz

		//johnfitz -- lots of changes
		if (cls.state != ca_dedicated) //no texture uploading for dedicated server
		{
			tx->isskytexture = false;
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
//				offset = (src_offset_t)(mt+1) - (src_offset_t)mod_base;
				if (Mod_CheckFullbrights ((byte *)(tx+1), pixels))
				{
					tx->gltexture = TexMgr_LoadImage (texturename, tx->width, tx->height,
						SRC_INDEXED, (byte *)(tx+1), TEX_MIPMAP | extraflags);
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

static void Mod_LoadVertexes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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

static void Mod_LoadVisibility(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->visdata = NULL;
		return;
	}
	brushmodel->visdata = (byte *)Q_malloc(l->filelen);
	memcpy(brushmodel->visdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadTexinfo(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dtexinfo_t *in = (dtexinfo_t *)(mod_base + l->fileofs);
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

		size_t miptex = LittleLong(in->miptex);
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

static void Mod_LoadLighting(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		brushmodel->lightdata = NULL;
		return;
	}

	brushmodel->lightdata = (byte *)Q_malloc(l->filelen);
	memcpy(brushmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadSurfedges(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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

static void Mod_LoadEdges(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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

// Fills in fa->verts[], fa->tex[], fa->numverts
static void BuildSurfaceDisplayList(brush_model_t *brushmodel, msurface_t *fa)
{
	// reconstruct the polygon
	size_t lnumverts = fa->numedges;
	fa->verts = (vec3_t *)Q_malloc(sizeof(*fa->verts) * lnumverts);
	fa->tex = (tex_cord *)Q_malloc(sizeof(*fa->tex) * lnumverts);
	fa->numverts = lnumverts;

	for (size_t i = 0; i < lnumverts; i++)
	{
		int lindex = brushmodel->surfedges[fa->firstedge + i];
		if (lindex > 0)
		{
			VectorCopy(brushmodel->vertexes[brushmodel->edges[lindex].v[0]].position, fa->verts[i]);
		}
		else
		{
			VectorCopy(brushmodel->vertexes[brushmodel->edges[-lindex].v[1]].position, fa->verts[i]);
		}

		float s = DotProduct (fa->verts[i], fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		float t = DotProduct (fa->verts[i], fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		fa->tex[i].s = s;
		fa->tex[i].t = t;
	}
}

/*
 * Fills in s->texturemins[] and s->extents[]
 */
static void CalcSurfaceExtents(brush_model_t *brushmodel, msurface_t *s)
{
	mtexinfo_t *tex = s->texinfo;

	for (size_t i = 0; i < 2; i++)
	{
		float min = FLT_MAX;
		float max = -FLT_MAX;

		for (size_t j = 0; j < s->numedges; j++)
		{
			float val = s->verts[j][0] * tex->vecs[i][0] +
				    s->verts[j][1] * tex->vecs[i][1] +
				    s->verts[j][2] * tex->vecs[i][2] +
						     tex->vecs[i][3];
			if (val < min)
				min = val;
			if (val > max)
				max = val;
		}

		int bmin = floorf(min / 16);
		int bmax = ceilf(max / 16);

		s->texturemins[i] = bmin * 16;
		s->extents[i] = (bmax - bmin) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Sys_Error("CalcSurfaceExtents: Bad surface extents");
	}
}

static void Mod_CalcSurfaceBounds(brush_model_t *brushmodel, msurface_t *s)
{
	s->mins[0] = s->mins[1] = s->mins[2] = FLT_MAX;
	s->maxs[0] = s->maxs[1] = s->maxs[2] = -FLT_MAX;

	for (size_t i = 0; i < s->numedges; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			s->mins[j] = min(s->mins[j], s->verts[i][j]);
			s->maxs[j] = max(s->maxs[j], s->verts[i][j]);
		}
	}
}

static void Mod_LoadSurfaces(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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

		BuildSurfaceDisplayList(brushmodel, out);

		CalcSurfaceExtents(brushmodel, out);

		Mod_CalcSurfaceBounds(brushmodel, out);

		// lighting info
		for (int i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		int lightofs = LittleLong(in->lightofs);
		out->samples = (lightofs == -1) ? NULL : (brushmodel->lightdata ? (brushmodel->lightdata + lightofs) : NULL);

		// set the drawing flags
		if (ISSKYTEX(out->texinfo->texture->name)) // sky
			out->flags |= SURF_DRAWSKY;
		else if (ISTURBTEX(out->texinfo->texture->name)) // turbulent
			out->flags |= SURF_DRAWTURB;
	}
}

static void Mod_LoadMarksurfaces(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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

static void Mod_LoadLeafs(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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
			out->bboxmin[j] = LittleShort(in->mins[j]);
			out->bboxmax[j] = LittleShort(in->maxs[j]);
		}

		int p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = brushmodel->marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : brushmodel->visdata + p;
		out->efrags = NULL;

		for (int j = 0; j < NUM_AMBIENTS; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		if (out->contents != CONTENTS_EMPTY)
			for (size_t j = 0; j < out->nummarksurfaces; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
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

static void Mod_LoadNodes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
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
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
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

static void Mod_LoadClipnodes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	hull_t *hull;

	dclipnode_t *in = (dclipnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	mclipnode_t *out = (mclipnode_t *)Q_calloc(count, sizeof(*out));

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

static void Mod_LoadSubmodels(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dmodel_t *in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	int count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Sys_Error("model %s has invalid # of vertices: %d", mod_name, count);

	mmodel_t *out = (mmodel_t *)Q_calloc(count, sizeof(*out));

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
	mclipnode_t *out = (mclipnode_t *)Q_calloc(count, sizeof(*out));

	hull_t *hull = &brushmodel->hulls[0];
	hull->clipnodes = out;
	hull->planes = brushmodel->planes;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->clip_mins[0] = 0;
	hull->clip_mins[1] = 0;
	hull->clip_mins[2] = 0;
	hull->clip_maxs[0] = 0;
	hull->clip_maxs[1] = 0;
	hull->clip_maxs[2] = 0;
	hull->available = true;

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

	// load and swap all the lump offsets and sizes
	mlump_t lumps[LUMP_MAX];
	for (size_t i = 0; i < LUMP_MAX; i++) {
		lumps[i].fileofs = LittleLong(header->lumps[i].fileofs);
		lumps[i].filelen = LittleLong(header->lumps[i].filelen);
	}

	// load into heap
	Mod_LoadEntities    (brushmodel, &lumps[LUMP_ENTITIES    ], (byte *) header, mod_name);
	Mod_LoadPlanes      (brushmodel, &lumps[LUMP_PLANES      ], (byte *) header, mod_name);
	Mod_LoadTextures    (brushmodel, &lumps[LUMP_TEXTURES    ], (byte *) header, mod_name);
	Mod_LoadVertexes    (brushmodel, &lumps[LUMP_VERTEXES    ], (byte *) header, mod_name);
	Mod_LoadVisibility  (brushmodel, &lumps[LUMP_VISIBILITY  ], (byte *) header, mod_name);
	Mod_LoadTexinfo     (brushmodel, &lumps[LUMP_TEXINFO     ], (byte *) header, mod_name);
	Mod_LoadLighting    (brushmodel, &lumps[LUMP_LIGHTING    ], (byte *) header, mod_name);
	Mod_LoadSurfedges   (brushmodel, &lumps[LUMP_SURFEDGES   ], (byte *) header, mod_name);
	Mod_LoadEdges       (brushmodel, &lumps[LUMP_EDGES       ], (byte *) header, mod_name);
	Mod_LoadSurfaces    (brushmodel, &lumps[LUMP_FACES       ], (byte *) header, mod_name);
	Mod_LoadMarksurfaces(brushmodel, &lumps[LUMP_MARKSURFACES], (byte *) header, mod_name);
	Mod_LoadLeafs       (brushmodel, &lumps[LUMP_LEAFS       ], (byte *) header, mod_name);
	Mod_LoadNodes       (brushmodel, &lumps[LUMP_NODES       ], (byte *) header, mod_name);
	Mod_LoadClipnodes   (brushmodel, &lumps[LUMP_CLIPNODES   ], (byte *) header, mod_name);
	Mod_LoadSubmodels   (brushmodel, &lumps[LUMP_MODELS      ], (byte *) header, mod_name);

	Mod_MakeHull0(brushmodel);

	glGenBuffers(1, &brushmodel->vertsVBO);
	glGenBuffers(1, &brushmodel->texVBO);
	glGenBuffers(1, &brushmodel->light_texVBO);

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
		mmodel_t *bm = &brushmodel->submodels[i];

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
