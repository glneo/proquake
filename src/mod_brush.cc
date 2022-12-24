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
static void Sky_LoadTexture(mtexture_t *tx, byte *tex_data)
{
	static byte front_data[128 * 128];
	static byte back_data[128 * 128];
	char texturename[64];

	// extract back layer and upload
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
			back_data[(i * 128) + j] = tex_data[i * 256 + j + 128];

	snprintf(texturename, sizeof(texturename), "%s_back", tx->name);
	solidskytexture = TexMgr_LoadImage(texturename, 128, 128, SRC_INDEXED, back_data, TEX_NOFLAGS);

	// extract front layer and upload
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
		{
			front_data[(i * 128) + j] = tex_data[i * 256 + j];
			if (front_data[(i * 128) + j] == 0)
				front_data[(i * 128) + j] = 255;
		}

	snprintf(texturename, sizeof(texturename), "%s_front", tx->name);
	alphaskytexture = TexMgr_LoadImage(texturename, 128, 128, SRC_INDEXED, front_data, TEX_ALPHA);
}

void Mod_LoadTextures(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	if (!l->filelen)
	{
		Con_Printf ("Mod_LoadTextures: no textures in bsp file\n");
		brushmodel->numtextures = 0;
		brushmodel->textures = NULL;
		return;
	}

	dmiptexlump_t *in = (dmiptexlump_t *)(mod_base + l->fileofs);

	brushmodel->numtextures = LittleLong(in->nummiptex);
	brushmodel->textures = (mtexture_t **)Q_calloc(brushmodel->numtextures, sizeof(*brushmodel->textures));

	for (size_t i = 0; i < brushmodel->numtextures; i++)
	{
		int data_offset = LittleLong(in->dataofs[i]);
		if (data_offset == -1)
			continue;
		dmiptex_t *dmiptex = (dmiptex_t *)((byte *)in + data_offset);

		int tex_width = LittleLong(dmiptex->width);
		int tex_height = LittleLong(dmiptex->height);

//		int mip_offsets[MIPLEVELS];
//		for (size_t j = 0 ;j < MIPLEVELS ;j++)
//			mip_offsets[j] = LittleLong (mt->offsets[j]);

		if ((tex_width & 15) || (tex_height & 15))
			Sys_Error ("Texture %s is not 16 aligned", dmiptex->name);

		brushmodel->textures[i] = (mtexture_t *)Q_malloc(sizeof(mtexture_t));
		mtexture_t *tx = brushmodel->textures[i];

		memcpy (tx->name, dmiptex->name, sizeof(tx->name));
		tx->width = tex_width;
		tx->height = tex_height;

		tx->isskytexture = false;

		tx->anim_total = 0;
		tx->anim_min = 0;
		tx->anim_max = 0;
		tx->anim_next = NULL;
		tx->alternate_anims = NULL;

//		for (int j=0 ; j<MIPLEVELS ; j++)
//			tx->offsets[j] = mip_offsets[j] + sizeof(dmiptexlump_t) - sizeof(dmiptex_t);

		// the pixels immediately follow the structures
		byte *tex_data = (byte *)(dmiptex + 1);
		int tex_size = tex_width * tex_height;

		// ericw -- check for pixels extending past the end of the lump.
		// appears in the wild; e.g. jam2_tronyn.bsp (func_mapjam2),
		// kellbase1.bsp (quoth), and can lead to a segfault if we read past
		// the end of the .bsp file buffer
		if ((tex_data + tex_size) > (mod_base + (l->fileofs + l->filelen)))
		{
			Sys_Error("Texture %s extends past end of lump\n", tx->name);
//			tex_size = max(0, (int)((mod_base + l->fileofs + l->filelen) - (tex_data)));
		}

		if (cls.state != ca_dedicated) //no texture uploading for dedicated server
		{
			char texturename[64];
			snprintf(texturename, sizeof(texturename), "%s:%s", mod_name, tx->name);

			if (!strncasecmp(tx->name,"sky",3)) //sky texture
			{
				Sky_LoadTexture(tx, tex_data);
				tx->isskytexture = true;
			}
			else if (tx->name[0] == '*') //warping texture
			{
				tx->gltexture = TexMgr_LoadImage (texturename, tx->width, tx->height,
					SRC_INDEXED, tex_data, TEX_NOFLAGS);
			}
			else //regular texture
			{
				// ericw -- fence textures
				unsigned extraflags = 0;
				if (tx->name[0] == '{')
					extraflags |= TEX_ALPHA;
				// ericw

				//external textures -- first look in "textures/mapname/" then look in "textures/"

//				offset = (src_offset_t)(tex_data) - (src_offset_t)mod_base;
				tx->gltexture = TexMgr_LoadImage (texturename, tx->width, tx->height,
					SRC_INDEXED, tex_data, TEX_MIPMAP | TEX_FULLBRIGHT | extraflags);
			}
		}
	}




//
// sequence the animations
//
//	for (size_t i = 0 ; i < brushmodel->numtextures ; i++)
//	{
//		mtexture_t	*anims[10];
//		mtexture_t	*altanims[10];
//
//		mtexture_t *tx = brushmodel->textures[i];
//		if (!tx || tx->name[0] != '+')
//			continue;
//		if (tx->anim_next)
//			continue; // already sequenced
//
//		// find the number of frames in the animation
//		memset (anims, 0, sizeof(anims));
//		memset (altanims, 0, sizeof(altanims));
//
//		int maxanim = tx->name[1];
//		int altmax = 0;
//		if (maxanim >= 'a' && maxanim <= 'z')
//			maxanim -= 'a' - 'A';
//		if (maxanim >= '0' && maxanim <= '9')
//		{
//			maxanim -= '0';
//			anims[maxanim] = tx;
//			maxanim++;
//		}
//		else if (maxanim >= 'A' && maxanim <= 'J')
//		{
//			altmax = maxanim - 'A';
//			maxanim = 0;
//			altanims[altmax] = tx;
//			altmax++;
//		}
//		else
//			Sys_Error ("Bad animating texture %s", tx->name);
//
//		for (size_t j = i + 1; j < brushmodel->numtextures; j++)
//		{
//			mtexture_t *tx2 = brushmodel->textures[j];
//			if (!tx2 || tx2->name[0] != '+')
//				continue;
//			if (strcmp (tx2->name+2, tx->name+2))
//				continue;
//
//			int num = tx2->name[1];
//			if (num >= 'a' && num <= 'z')
//				num -= 'a' - 'A';
//			if (num >= '0' && num <= '9')
//			{
//				num -= '0';
//				anims[num] = tx2;
//				if (num+1 > maxanim)
//					maxanim = num + 1;
//			}
//			else if (num >= 'A' && num <= 'J')
//			{
//				num = num - 'A';
//				altanims[num] = tx2;
//				if (num+1 > altmax)
//					altmax = num+1;
//			}
//			else
//				Sys_Error ("Bad animating texture %s", tx->name);
//		}
//
//#define	ANIM_CYCLE 2
//		// link them all together
//		for (int j = 0; j < maxanim; j++)
//		{
//			mtexture_t *tx = anims[j];
//			if (!tx)
//				Sys_Error("Missing frame %i of %s", j, tx->name);
//			tx->anim_total = maxanim * ANIM_CYCLE;
//			tx->anim_min = j * ANIM_CYCLE;
//			tx->anim_max = (j + 1) * ANIM_CYCLE;
//			tx->anim_next = anims[(j + 1) % maxanim];
//			if (altmax)
//				tx->alternate_anims = altanims[0];
//		}
//		for (int j = 0; j < altmax; j++)
//		{
//			mtexture_t *tx = altanims[j];
//			if (!tx)
//				Sys_Error("Missing frame %i of %s", j, tx->name);
//			tx->anim_total = altmax * ANIM_CYCLE;
//			tx->anim_min = j * ANIM_CYCLE;
//			tx->anim_max = (j + 1) * ANIM_CYCLE;
//			tx->anim_next = altanims[(j + 1) % altmax];
//			if (maxanim)
//				tx->alternate_anims = anims[0];
//		}
//	}
}

static void Mod_LoadVertexes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dvertex_t *in = (dvertex_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numvertexes = l->filelen / sizeof(*in);
	brushmodel->vertexes = (mvertex_t *)Q_malloc(sizeof(mvertex_t) * brushmodel->numvertexes);

	for (size_t i = 0; i < brushmodel->numvertexes; i++)
	{
		brushmodel->vertexes[i].position[0] = LittleFloat(in[i].point[0]);
		brushmodel->vertexes[i].position[1] = LittleFloat(in[i].point[1]);
		brushmodel->vertexes[i].position[2] = LittleFloat(in[i].point[2]);
	}
}

static void Mod_LoadVisibility(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	byte *in = (byte *)(mod_base + l->fileofs);
	if (!l->filelen)
	{
		brushmodel->numvisdata = 0;
		brushmodel->visdata = NULL;
		return;
	}

	brushmodel->numvisdata = l->filelen / sizeof(*in);
	brushmodel->visdata = (byte *)Q_malloc(sizeof(byte) * brushmodel->numvisdata);

	memcpy(brushmodel->visdata, in, brushmodel->numvisdata);
}

static void Mod_LoadTexinfo(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dtexinfo_t *in = (dtexinfo_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	size_t numtexinfo = l->filelen / sizeof(*in);
	mtexinfo_t *texinfo = (mtexinfo_t *)Q_malloc(sizeof(mtexinfo_t) * numtexinfo);

	for (size_t i = 0; i < numtexinfo; i++, in++)
	{
		mtexinfo_t *out = &texinfo[i];
		for (size_t j = 0; j < 2; j++)
		{
			out->vecs[j].vecs[0] = LittleFloat(in->vecs[j][0]);
			out->vecs[j].vecs[1] = LittleFloat(in->vecs[j][1]);
			out->vecs[j].vecs[2] = LittleFloat(in->vecs[j][2]);
			out->vecs[j].offset  = LittleFloat(in->vecs[j][3]);
		}

		size_t miptex = LittleLong(in->miptex);
		out->flags = LittleLong(in->flags);

		if (miptex >= brushmodel->numtextures)
			Sys_Error("miptex >= brushmodel->numtextures");

		out->texture = brushmodel->textures ? brushmodel->textures[miptex] : NULL;
		if (!out->texture)
		{
			out->texture = NULL; // FIXME: texture not found
			out->flags = 0;
		}
	}

	brushmodel->numtexinfo = numtexinfo;
	brushmodel->texinfo = texinfo;
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
	uint32_t *in = (uint32_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numsurfedges = l->filelen / sizeof(*in);
	brushmodel->surfedges = (int *)Q_malloc(sizeof(*brushmodel->surfedges) * brushmodel->numsurfedges);

	for (size_t i = 0; i < brushmodel->numsurfedges; i++)
		brushmodel->surfedges[i] = LittleLong(in[i]);
}

static void Mod_LoadEdges(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dedge_t *in = (dedge_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numedges = l->filelen / sizeof(*in);
	brushmodel->edges = (medge_t *)Q_malloc(sizeof(*brushmodel->edges) * brushmodel->numedges + 1);

	for (size_t i = 0; i < brushmodel->numedges; i++)
	{
		brushmodel->edges[i].v[0] = (unsigned short) LittleShort(in[i].v[0]);
		brushmodel->edges[i].v[1] = (unsigned short) LittleShort(in[i].v[1]);
	}
}

// Fills in fa->verts[], fa->tex[], fa->numverts
static void BuildSurfaceDisplayList(brush_model_t *brushmodel, msurface_t *fa)
{
	// reconstruct the polygon
	fa->numverts = fa->numedges;
	fa->verts.resize(fa->numverts);
	fa->tex.resize(fa->numverts);

	for (size_t i = 0; i < fa->numverts; i++)
	{
		int lindex = brushmodel->surfedges[fa->firstedge + i];
		if (lindex > 0)
		{
			fa->verts[i].x = brushmodel->vertexes[brushmodel->edges[lindex].v[0]].position[0];
			fa->verts[i].y = brushmodel->vertexes[brushmodel->edges[lindex].v[0]].position[1];
			fa->verts[i].z = brushmodel->vertexes[brushmodel->edges[lindex].v[0]].position[2];
		}
		else
		{
			fa->verts[i].x = brushmodel->vertexes[brushmodel->edges[-lindex].v[1]].position[0];
			fa->verts[i].y = brushmodel->vertexes[brushmodel->edges[-lindex].v[1]].position[1];
			fa->verts[i].z = brushmodel->vertexes[brushmodel->edges[-lindex].v[1]].position[2];
		}

		vec3_t temp_vert = {fa->verts[i].x, fa->verts[i].y, fa->verts[i].z};

		float s = DotProduct (temp_vert, fa->texinfo->vecs[0].vecs) + fa->texinfo->vecs[0].offset;
		s /= fa->texinfo->texture->width;

		float t = DotProduct (temp_vert, fa->texinfo->vecs[1].vecs) + fa->texinfo->vecs[1].offset;
		t /= fa->texinfo->texture->height;

		fa->tex[i].s = s;
		fa->tex[i].t = t;
	}
}

/* Fills in s->texturemins[] and s->extents[] */
static void CalcSurfaceExtents(brush_model_t *brushmodel, msurface_t *s)
{
	mtexinfo_t *tex = s->texinfo;

	for (size_t i = 0; i < 2; i++)
	{
		float min = FLT_MAX;
		float max = -FLT_MAX;

		for (size_t j = 0; j < s->numedges; j++)
		{
			mvertex_t *v;

			int e = brushmodel->surfedges[s->firstedge + j];
			if (e >= 0)
				v = &brushmodel->vertexes[brushmodel->edges[e].v[0]];
			else
				v = &brushmodel->vertexes[brushmodel->edges[-e].v[1]];

			float val = ((double)v->position[0] * (double)s->texinfo->vecs[i].vecs[0]) +
			            ((double)v->position[1] * (double)s->texinfo->vecs[i].vecs[1]) +
			            ((double)v->position[2] * (double)s->texinfo->vecs[i].vecs[2]) +
			                                      (double)s->texinfo->vecs[i].offset;

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
		mvertex_t *v;

		int e = brushmodel->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &brushmodel->vertexes[brushmodel->edges[e].v[0]];
		else
			v = &brushmodel->vertexes[brushmodel->edges[-e].v[1]];

		for (size_t j = 0; j < 3; j++)
		{
			s->mins[j] = min(s->mins[j], v->position[j]);
			s->maxs[j] = max(s->maxs[j], v->position[j]);
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

		out->plane = brushmodel->planes + LittleShort(in->planenum);
		if (LittleShort(in->side))
			out->flags |= SURF_PLANEBACK;

		out->texinfo = brushmodel->texinfo + LittleShort(in->texinfo);

		out->indices = new std::vector<unsigned short>();

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

	brushmodel->nummarksurfaces = l->filelen / sizeof(*in);
	brushmodel->marksurfaces = (msurface_t **)Q_malloc(sizeof(*brushmodel->marksurfaces) * brushmodel->nummarksurfaces);

	for (size_t i = 0; i < brushmodel->nummarksurfaces; i++)
	{
		size_t j = LittleShort(in[i]);
		if (j >= brushmodel->numsurfaces)
			Sys_Error("bad surface number");
		brushmodel->marksurfaces[i] = &brushmodel->surfaces[j];
	}
}

static void Mod_LoadLeafs(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dleaf_t *in = (dleaf_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numleafs = l->filelen / sizeof(*in);
	brushmodel->leafs = (mleaf_t *)Q_malloc(sizeof(mleaf_t) * brushmodel->numleafs);

	for (size_t i = 0; i < brushmodel->numleafs; i++, in++)
	{
		mleaf_t *leaf = &brushmodel->leafs[i];

		for (int j = 0; j < 3; j++)
		{
			leaf->bboxmin[j] = LittleShort(in->mins[j]);
			leaf->bboxmax[j] = LittleShort(in->maxs[j]);
		}

		leaf->contents = LittleLong(in->contents);

		int visofs = LittleLong(in->visofs);
		if (visofs < 0 || (size_t)visofs >= brushmodel->numvisdata)
			leaf->compressed_vis = NULL;
		else
			leaf->compressed_vis = &brushmodel->visdata[(size_t)visofs];
		leaf->efrags = NULL;

		leaf->nummarksurfaces = LittleShort(in->nummarksurfaces);
		int firstmarksurface = LittleShort(in->firstmarksurface);
		if ((size_t)firstmarksurface > brushmodel->nummarksurfaces)
			Sys_Error("bad firstmarksurface in %s", mod_name);
		leaf->firstmarksurface = &brushmodel->marksurfaces[firstmarksurface];

		if (leaf->contents != CONTENTS_EMPTY)
			for (size_t j = 0; j < leaf->nummarksurfaces; j++)
				leaf->firstmarksurface[j]->flags |= SURF_UNDERWATER;

		for (int j = 0; j < NUM_AMBIENTS; j++)
			leaf->ambient_sound_level[j] = in->ambient_level[j];
	}
}

static void Mod_LoadNodes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dnode_t *in = (dnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numnodes = l->filelen / sizeof(*in);
	brushmodel->nodes = (mnode_t *)Q_malloc(sizeof(mnode_t) * brushmodel->numnodes);

	for (size_t i = 0; i < brushmodel->numnodes; i++, in++)
	{
		mnode_t *out = &brushmodel->nodes[i];
		out->contents = 0;
		out->visframe = 0;

		for (int j = 0; j < 3; j++)
		{
			out->bboxmin[j] = LittleShort(in->mins[j]);
			out->bboxmax[j] = LittleShort(in->maxs[j]);
		}

		int p = LittleLong(in->planenum);
		out->plane = brushmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);


		p = LittleShort(in->children[0]);
		if (p >= 0)
			out->left_node = brushmodel->nodes + p;
		else
			out->left_node = (mnode_t *) (brushmodel->leafs + (-1 - p));

		p = LittleShort(in->children[1]);
		if (p >= 0)
			out->right_node = brushmodel->nodes + p;
		else
			out->right_node = (mnode_t *) (brushmodel->leafs + (-1 - p));

	}
}

static void Mod_LoadClipnodes(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dclipnode_t *in = (dclipnode_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numclipnodes = l->filelen / sizeof(*in);
	brushmodel->clipnodes = (mclipnode_t *)Q_malloc(sizeof(mclipnode_t) * brushmodel->numclipnodes);

	for (size_t i = 0; i < brushmodel->numclipnodes; i++, in++)
	{
		brushmodel->clipnodes[i].planenum = LittleLong(in->planenum);
		brushmodel->clipnodes[i].children[0] = LittleShort(in->children[0]);
		brushmodel->clipnodes[i].children[1] = LittleShort(in->children[1]);
	}
}

static void Mod_LoadSubmodels(brush_model_t *brushmodel, mlump_t *l, byte *mod_base, char *mod_name)
{
	dmodel_t *in = (dmodel_t *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error("funny lump size in %s", mod_name);

	brushmodel->numsubmodels = l->filelen / sizeof(*in);
	if (brushmodel->numsubmodels > MAX_MODELS)
		Sys_Error("model %s has invalid # of submodels: %zu", mod_name, brushmodel->numsubmodels);

	brushmodel->submodels = (msubmodel_t *)Q_malloc(sizeof(*brushmodel->submodels) * brushmodel->numsubmodels);

	for (size_t i = 0; i < brushmodel->numsubmodels; i++, in++)
	{
		msubmodel_t *submodel = &brushmodel->submodels[i];

		for (int j = 0; j < 3; j++)
		{	// spread the mins / maxs by a pixel
			submodel->mins[j] = LittleFloat(in->mins[j]) - 1;
			submodel->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			submodel->origin[j] = LittleFloat(in->origin[j]);
		}

		for (int j = 0; j < MAX_MAP_HULLS; j++)
			submodel->headnode[j] = LittleLong(in->headnode[j]);

		submodel->visleafs = LittleLong(in->visleafs);
		submodel->firstface = LittleLong(in->firstface);
		submodel->numfaces = LittleLong(in->numfaces);
	}
}

/*
 * Duplicate the drawing hull structure as a clipping hull
 */
static void Mod_MakeHull0(brush_model_t *brushmodel)
{
	brushmodel->hulls[1].clipnodes = brushmodel->clipnodes;
	brushmodel->hulls[1].firstclipnode = 0;
	brushmodel->hulls[1].lastclipnode = brushmodel->numclipnodes - 1;
	brushmodel->hulls[1].planes = brushmodel->planes;
	brushmodel->hulls[1].clip_mins[0] = -16;
	brushmodel->hulls[1].clip_mins[1] = -16;
	brushmodel->hulls[1].clip_mins[2] = -24;
	brushmodel->hulls[1].clip_maxs[0] = 16;
	brushmodel->hulls[1].clip_maxs[1] = 16;
	brushmodel->hulls[1].clip_maxs[2] = 32;
	brushmodel->hulls[1].available = true;

	brushmodel->hulls[2].clipnodes = brushmodel->clipnodes;
	brushmodel->hulls[2].firstclipnode = 0;
	brushmodel->hulls[2].lastclipnode = brushmodel->numclipnodes - 1;
	brushmodel->hulls[2].planes = brushmodel->planes;
	brushmodel->hulls[2].clip_mins[0] = -32;
	brushmodel->hulls[2].clip_mins[1] = -32;
	brushmodel->hulls[2].clip_mins[2] = -24;
	brushmodel->hulls[2].clip_maxs[0] = 32;
	brushmodel->hulls[2].clip_maxs[1] = 32;
	brushmodel->hulls[2].clip_maxs[2] = 64;
	brushmodel->hulls[2].available = true;

	brushmodel->hulls[3].clipnodes = brushmodel->clipnodes;
	brushmodel->hulls[3].firstclipnode = 0;
	brushmodel->hulls[3].lastclipnode = brushmodel->numclipnodes - 1;
	brushmodel->hulls[3].planes = brushmodel->planes;
	brushmodel->hulls[3].clip_mins[0] = -16;
	brushmodel->hulls[3].clip_mins[1] = -16;
	brushmodel->hulls[3].clip_mins[2] = -6;
	brushmodel->hulls[3].clip_maxs[0] = 16;
	brushmodel->hulls[3].clip_maxs[1] = 16;
	brushmodel->hulls[3].clip_maxs[2] = 30;
	brushmodel->hulls[3].available = false;


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

		mnode_t *child = in->left_node;
		if (child->contents < 0)
			out->children[0] = child->contents;
		else
			out->children[0] = child - brushmodel->nodes;

		child = in->right_node;
		if (child->contents < 0)
			out->children[1] = child->contents;
		else
			out->children[1] = child - brushmodel->nodes;

	}
}

void GL_CreateSurfaceLists(brush_model_t *brushmodel);

void Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	dheader_t *header = (dheader_t *) buffer;
	char *mod_name = mod->name;

	brush_model_t *brushmodel = (brush_model_t *)Q_malloc(sizeof(*brushmodel));

	int bspversion = LittleLong(header->version);
	if (bspversion != BSPVERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod_name, bspversion, BSPVERSION);

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



	mod->numframes = 2; // regular and alternate animation
	mod->synctype = ST_SYNC;

	mod->flags = 0;

	mod->type = mod_brush;
	mod->brushmodel = brushmodel;



	// Main model has some data stored in submodel 0
	msubmodel_t *submodel = &brushmodel->submodels[0];

	brushmodel->hulls[0].firstclipnode = submodel->headnode[0];
	for (int j = 1; j < MAX_MAP_HULLS; j++)
	{
		brushmodel->hulls[j].firstclipnode = submodel->headnode[j];
		brushmodel->hulls[j].lastclipnode = brushmodel->numclipnodes - 1;
	}

	brushmodel->firstmodelsurface = submodel->firstface;
	brushmodel->nummodelsurfaces = submodel->numfaces;

	VectorCopy(submodel->maxs, mod->maxs);
	VectorCopy(submodel->mins, mod->mins);

	mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

	brushmodel->numleafs = submodel->visleafs;




	Con_Printf("Creating lists for: %s\n", mod_name);
	GL_CreateSurfaceLists(brushmodel);



	for (size_t i = 1; i < brushmodel->numsubmodels; i++)
	{
		char name[10];
		snprintf(name, sizeof(name), "*%zu", i);
		Con_Printf("Creating submodel: %s\n", name);
		model_t *loadmodel = Mod_FindName(name);

		// duplicate the basic information
		*loadmodel = *mod;
		loadmodel->brushmodel = (brush_model_t *)Q_malloc(sizeof(brush_model_t));
		*loadmodel->brushmodel = *brushmodel;

		strcpy(loadmodel->name, name);

		msubmodel_t *submodel = &brushmodel->submodels[i];

		for (int j = 0; j < MAX_MAP_HULLS; j++)
		{
			loadmodel->brushmodel->hulls[j].firstclipnode = submodel->headnode[j];
			loadmodel->brushmodel->hulls[j].lastclipnode = brushmodel->numclipnodes - 1;
		}

		loadmodel->brushmodel->firstmodelsurface = submodel->firstface;
		loadmodel->brushmodel->nummodelsurfaces = submodel->numfaces;

		VectorCopy(submodel->maxs, loadmodel->maxs);
		VectorCopy(submodel->mins, loadmodel->mins);

		loadmodel->radius = RadiusFromBounds(loadmodel->mins, loadmodel->maxs);

		loadmodel->brushmodel->numleafs = submodel->visleafs;
	}
}
