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

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadAliasModel (model_t *mod, void *buffer);
void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", true};


/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	Cvar_RegisterVariable (&gl_subdivide_size, NULL);
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	if ((r = Cache_Check (&mod->cache)))
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Sys_Error ("Mod_Extradata: caching failed");

	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	float		d;
	mnode_t		*node;
	mplane_t	*plane;

	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
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
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	int		c, row;
	byte	*out;
	static byte	decompressed[MAX_MAP_LEAFS/8];

	row = (model->numleafs+7)>>3;
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

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;

	return Mod_DecompressVis (leaf->compressed_vis, model);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;
	static qboolean NoFree, Done;
	extern	void GL_FreeTextures (void);

	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++) 
	{
		if (mod->type != mod_alias)
			mod->needload = true;
	}

	if (!Done)
	{
		// Some 3dfx miniGLs don't support glDeleteTextures (i.e. do nothing)
		NoFree = isDedicated || COM_CheckParm ("-nofreetex");
		Done = true;
	}

	if (!NoFree)
		GL_FreeTextures ();
}

/*
==================
Mod_FindName
==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;

	if (!name[0])
		Sys_Error ("Mod_FindName: NULL name");

// search the currently loaded models
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++) 
	{
		if (!strcmp (mod->name, name) )
			break;
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Sys_Error ("mod_numknown == MAX_MOD_KNOWN");

		mod_numknown++;
		strcpy (mod->name, name);
		mod->needload = true;
	}

	return mod;
}

/*
==================
Mod_TouchModel
==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	if (!mod->needload && (mod->type == mod_alias))
			Cache_Check (&mod->cache);
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	unsigned *buf;
	byte	stackbuf[1024];		// avoid dirtying the cache heap

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			if (Cache_Check (&mod->cache))
				return mod;
		}
		 else
		{
			return mod;		// not cached at all
		}
	}

// because the world is so huge, load it one piece at a time

// load the file
	if (!(buf = (unsigned *)COM_LoadStackFile (mod->name, stackbuf, sizeof(stackbuf))))
	{
		if (crash)
			Sys_Error ("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

// allocate a new model
	COM_FileBase (mod->name, loadname);

	loadmodel = mod;

// fill it in

// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	default:
		Mod_LoadBrushModel (mod, buf);
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
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

#define ISTURBTEX(name)		((name)[0] == '*')
#define ISSKYTEX(name)		((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

byte	*mod_base;


/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int		i, j, pixels, num, max, altmax, texture_flag;
	miptex_t	*mt;
	texture_t	*tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
#ifndef NOFULLBRIGHT
	char fbr_mask_name[64];
#endif
	if (!l->filelen)
	{
		loadmodel->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_AllocName (m->nummiptex * sizeof(*loadmodel->textures) , loadname);

	for (i=0 ; i<m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32  && CRC_Block((byte *)(mt + 1), mt->width * mt->height) == 65393)
		{	// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy (mt + 1, (byte *)(mt + 1) + 32*31, 32);
		}

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error ("Texture %s is not 16 aligned", mt->name);

		pixels = mt->width*mt->height/64*85;
		loadmodel->textures[i] = tx = Hunk_AllocName (sizeof(texture_t) +pixels, loadname );

		memcpy (tx->name, mt->name, sizeof(tx->name));
		tx->width = mt->width;
		tx->height = mt->height;

		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);

		// the pixels immediately follow the structures
		memcpy ( tx+1, mt+1, pixels);

		if (cls.state != ca_dedicated)	// Dedicated skips this whole ordeal
		{

			// If world model and sky texture and q1 bsp and not dedicated ...
			if (loadmodel->isworldmodel && ISSKYTEX(tx->name) && loadmodel->bspversion == BSPVERSION)
					R_InitSky (tx);

			// If world model and NOT sky texture
			if (loadmodel->isworldmodel && !ISSKYTEX(tx->name))//R00k
				texture_flag |= TEX_WORLD;

			if (loadmodel->bspversion == BSPVERSION)
			{
				texture_mode = GL_LINEAR_MIPMAP_NEAREST; //_LINEAR;
				tx->gl_texturenum = GL_LoadTexture (mt->name, tx->width, tx->height, (byte *)(tx+1), texture_flag | TEX_MIPMAP);
				texture_mode = GL_LINEAR;
			}


			tx->fullbright = -1; // because 0 is a potentially valid texture number
			// check for fullbright pixels in the texture - only if it ain't liquid, etc also

			if (loadmodel->bspversion == BSPVERSION && !ISTURBTEX(tx->name) && FindFullbrightTexture ((byte *)(tx+1), pixels))
			{
				// convert any non fullbright pixel to fully transparent
				ConvertPixels ((byte *)(tx + 1), pixels);

				// get a new name for the fullbright mask to avoid cache mismatches
				SNPrintf (fbr_mask_name, sizeof(fbr_mask_name), "fullbright_mask_%s", mt->name);

				// load the fullbright pixels version of the texture
				tx->fullbright = GL_LoadTexture (fbr_mask_name, tx->width, tx->height, (byte *)(tx + 1), texture_flag | TEX_MIPMAP | TEX_ALPHA);
			}

		}

	} // End !dedicated
// END LOOP

	// sequence the animations
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

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
			Sys_Error ("Bad animating texture %s", tx->name);
		}

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
				max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
				altmax = num+1;
			}
			else
			{
				Sys_Error ("Bad animating texture %s", tx->name);
			}
		}

		#define	ANIM_CYCLE	2
		// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}

		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
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
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName ( l->filelen, loadname);
	memcpy (loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = NULL;
		return;
	}
	loadmodel->entities = Hunk_AllocName ( l->filelen, loadname);
	memcpy (loadmodel->entities, mod_base + l->fileofs, l->filelen);
}

vec3_t worldmins; // Baker: get world bounds
vec3_t worldmaxs;

/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l, qboolean storebounds)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	if (storebounds) {
		worldmins[0]=99999999;
		worldmins[1]=99999999;
		worldmins[2]=99999999;
		worldmaxs[0]=-99999999;
		worldmaxs[1]=-99999999;
		worldmaxs[2]=-99999999;
	}

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadVertexes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);

		if (out->position[0] < worldmins[0]) worldmins[0] = out->position[0];
		if (out->position[1] < worldmins[1]) worldmins[1] = out->position[1];
		if (out->position[2] < worldmins[2]) worldmins[2] = out->position[2];
		if (out->position[0] > worldmaxs[0]) worldmaxs[0] = out->position[0];
		if (out->position[1] > worldmaxs[1]) worldmaxs[1] = out->position[1];
		if (out->position[2] > worldmaxs[2]) worldmaxs[2] = out->position[2];

	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in, *out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadSubmodels: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Sys_Error ("Mod_LoadSubmodels: count > MAX_MODELS");

	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadEdges: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, k, count, miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadTexinfo: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (k = 0; k < 2; k++ )
			for (j=0 ; j<4 ; j++)
				out->vecs[k][j] = LittleFloat (in->vecs[k][j]);
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;


		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("Mod_LoadTexinfo: miptex >= loadmodel->numtextures");
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
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e, bmins[2], bmaxs[2];
	mvertex_t	*v;
	mtexinfo_t	*tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + v->position[1] * tex->vecs[j][1] + v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512)
			Host_Error ("CalcSurfaceExtents: Bad surface extents");
	}
}

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum, planenum, side;
#ifdef SUPPORTS_GL_OVERBRIGHTS
	extern cvar_t gl_overbright;
#endif
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadFaces: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		if ((side = LittleShort(in->side)))
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);

	// lighting info
		for (i=0 ; i<MAXLIGHTMAPS ; i++)
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
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}

		if (ISTURBTEX(out->texinfo->texture->name))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
			continue;
		}
	}
}

/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);

		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadLeafs: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces + LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ?  NULL :  loadmodel->visdata + p;
		out->efrags = NULL;

		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadClipnodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
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
		hull->lastclipnode = count-1;
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
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -6;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 30;
		hull->available = false;


	for (i=0 ; i<count ; i++, out++, in++)
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
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	dclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
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
void Mod_LoadMarksurfaces (lump_t *l)
{
	int		i, j, count;
	short		*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{
	int	i, count, *in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadSurfedges: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int		i, j, count, bits;
	mplane_t	*out;
	dplane_t 	*in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Sys_Error ("Mod_LoadPlanes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*2*sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength (corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;


	mod->bspversion = LittleLong (header->version);

	if (mod->bspversion != BSPVERSION)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, mod->bspversion, BSPVERSION);

	loadmodel->isworldmodel = !strcmp (loadmodel->name, va("maps/%s.bsp", host_worldname) );
	

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES], loadmodel->isworldmodel ? 1 : 0);
	if (loadmodel->isworldmodel)
		Con_DPrintf ("World bounds: %4.1f %4.1f %4.1f to %4.1f %4.1f %4.1f\n", worldmins[0], worldmins[1], worldmins[2], worldmaxs[0], worldmaxs[1], worldmaxs[2]);

	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation

// set up the submodels (FIXME: this is confusing)


	// johnfitz -- okay, so that i stop getting confused every time i look at this loop, here's how it works:
	// we're looping through the submodels starting at 0.  Submodel 0 is the main model, so we don't have to
	// worry about clobbering data the first time through, since it's the same data.  At the end of the loop,
	// we create a new copy of the data to use the next time through.
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[10];

			SNPrintf (name, sizeof(name), "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

static aliashdr_t *pheader;

// meshing needs access to these
stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];
trivertx_t	*poseverts[MAXALIASFRAMES];

static int	posenum; // a pose is a single set of vertexes. a frame may be an animating sequence of poses
static int	aliasbboxmins[3], aliasbboxmaxs[3];

/*
=================
Mod_LoadAliasFrame
=================
*/
static void * Mod_LoadAliasFrame (void * pin, maliasframedesc_t *frame)
{
	daliasframe_t	*pdaliasframe = (daliasframe_t *)pin;
	int				i;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];

		aliasbboxmins[i] = min(aliasbboxmins[i], frame->bboxmin.v[i]);
		aliasbboxmaxs[i] = max(aliasbboxmaxs[i], frame->bboxmax.v[i]);
	}

	{
		trivertx_t		*pinframe = (trivertx_t *)(pdaliasframe + 1);

		poseverts[posenum] = pinframe;
		posenum++;

		pinframe += pheader->numvertsperframe;

		return (void *)pinframe;
	}


}

/*
=================
Mod_LoadAliasGroup
=================
*/
static void *Mod_LoadAliasGroup (void *pin,  maliasframedesc_t *frame)
{	
	void				*ptemp;
	
	daliasgroup_t		*pingroup = (daliasgroup_t *)pin;
	int					numframes		= LittleLong (pingroup->numframes);
	int					i;

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];

		aliasbboxmins[i] = min(aliasbboxmins[i], frame->bboxmin.v[i]);
		aliasbboxmaxs[i] = max(aliasbboxmaxs[i], frame->bboxmax.v[i]);
	}

	{
		daliasinterval_t	*pin_intervals;

		pin_intervals = (daliasinterval_t *)(pingroup + 1);
		frame->interval = LittleFloat (pin_intervals->interval);
		pin_intervals += numframes;
		ptemp = (void *)pin_intervals;
	}

	for (i=0 ; i<numframes ; i++, posenum++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numvertsperframe;
	}

	return ptemp;
}

//=========================================================

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/
void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	int					i, inpt = 0, outpt = 0, filledcolor = -1;
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
		{
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}


/*
===============
Mod_LoadAllSkins
===============
*/
static void *Mod_LoadAllSkins (int numskins, daliasskintype_t *pskintype)
{
	int		i, j, k, size, groupskins;
	char	name[32];
	byte	*skin, *texels;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;

	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Sys_Error ("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);

	size = pheader->skinwidth * pheader->skinheight;

	for (i=0 ; i<numskins ; i++) 
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE) 
		{
			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			// save 8 bit texels for the player model to remap
				texels = Hunk_AllocName(size, loadname);
				pheader->texels[i] = texels - (byte *)pheader;
				memcpy (texels, (byte *)(pskintype + 1), size);

			SNPrintf (name, sizeof(name), "%s_%i", loadmodel->name, i);
			pheader->gl_texturenum[i][0] =
			pheader->gl_texturenum[i][1] =
			pheader->gl_texturenum[i][2] =
			pheader->gl_texturenum[i][3] =
				GL_LoadTexture (name, pheader->skinwidth,
				pheader->skinheight, (byte *)(pskintype + 1), TEX_MIPMAP);
			pskintype = (daliasskintype_t *)((byte *)(pskintype+1) + size);
		} else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);


			for (j=0 ; j<groupskins ; j++) 
			{
					Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
					if (j == 0) {
						texels = Hunk_AllocName(size, loadname);
						pheader->texels[i] = texels - (byte *)pheader;
						memcpy (texels, (byte *)(pskintype), size);
					}
					SNPrintf (name, sizeof(name), "%s_%i_%i", loadmodel->name, i,j);
					pheader->gl_texturenum[i][j&3] =
						GL_LoadTexture (name, pheader->skinwidth,
						pheader->skinheight, (byte *)(pskintype), TEX_MIPMAP);
					pskintype = (daliasskintype_t *)((byte *)(pskintype) + size);
			}

			for (k = j; j < 4; j++)
				pheader->gl_texturenum[i][j&3] = pheader->gl_texturenum[i][j - k];
		}
	}

	return (void *)pskintype;
}

qboolean COM_ListMatch (char *liststr, const char *itemstr)
{
	char	*start, *matchspot, *nextspot;
	int		itemlen;

	// Check for disqualifying situations ...
	if (liststr == NULL || itemstr == NULL || *itemstr == 0 || strchr (itemstr, ','))
		return false;

	itemlen = strlen (itemstr);

	for (start = liststr ; matchspot = strstr (start, itemstr) ; start = nextspot)
	{
		nextspot = matchspot + itemlen;

		// Validate that either the match begins the string or the previous character is a comma
		// And that the terminator is either null or a comma.  This protects against partial
		// matches

		if ((matchspot == start || *(matchspot - 1) == ',') && (*nextspot == 0 || *nextspot == ','))
			return true; // matchspot;
	}

	return false;
}


/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	mdl_t	*pinmodel	= (mdl_t *)buffer;
	int		version 	= LittleLong (pinmodel->version);
	int		i;	
	int		hunk_start;

	if (version != ALIAS_VERSION) Sys_Error ("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);
	
	mod->type = mod_alias;
	mod->flags = LittleLong (pinmodel->flags);
	if (strcmp ("progs/player.mdl", mod->name) == 0)
		mod->flags |= MOD_PLAYER;

// allocate space for a working header, plus all the data except the frames, skin and group info
	{
		int	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) * sizeof (pheader->frames[0]);
		hunk_start = Hunk_LowMark ();
		pheader = Hunk_AllocName (size, loadname);
	}

// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);
	pheader->numvertsperframe = LittleLong (pinmodel->numverts);
	pheader->numtris = LittleLong (pinmodel->numtris);
	pheader->numframes = LittleLong (pinmodel->numframes);

	// validate the setup
	if (pheader->numframes < 1) Sys_Error ("Mod_LoadAliasModel: invalid # of frames %d in %s", pheader->numframes, mod->name);
	if (pheader->skinheight > MAX_LBM_HEIGHT) Sys_Error ("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);
	if (pheader->numvertsperframe <= 0) Sys_Error ("Mod_LoadAliasModel: model %s has no vertices", mod->name);
	if (pheader->numvertsperframe > MAXALIASVERTS) Sys_Error ("Mod_LoadAliasModel: model %s has too many vertices (%d, max = %d)", mod->name, pheader->numvertsperframe, MAXALIASVERTS);
	if (pheader->numtris <= 0) Sys_Error ("Mod_LoadAliasModel: model %s has no triangles", mod->name);
	if (pheader->numtris > MAXALIASTRIS) Sys_Error ("Mod_LoadAliasModel: model %s has too many triangles (%d, max = %d)", mod->name, pheader->numtris, MAXALIASTRIS);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

// origins and scale
	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	// Drill down and load
	{ // load the skins
		daliasskintype_t *pskintype = (daliasskintype_t *)&pinmodel[1];
		pskintype = Mod_LoadAllSkins (pheader->numskins, pskintype);

		{ // load base s and t vertices
			stvert_t *pinstverts = (stvert_t *)pskintype;

			for (i = 0 ; i < pheader->numvertsperframe ; i ++)
			{
				stverts[i].onseam = LittleLong (pinstverts[i].onseam);
				stverts[i].s = LittleLong (pinstverts[i].s);
				stverts[i].t = LittleLong (pinstverts[i].t);
			}

			{ // load triangle lists
				dtriangle_t	*pintriangles = (dtriangle_t *)&pinstverts[pheader->numvertsperframe];
				for (i = 0 ; i < pheader->numtris; i ++)
				{
					int j;
					triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);
			
					for (j = 0 ; j < 3 ; j ++)
						triangles[i].vertindex[j] = LittleLong (pintriangles[i].vertindex[j]);
				}

				{ // load the frames
					daliasframetype_t	*pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];
					posenum = 0;
	
					aliasbboxmins[0] = aliasbboxmins[1] = aliasbboxmins[2] = 99999;
					aliasbboxmaxs[0] = aliasbboxmaxs[1] = aliasbboxmaxs[2] = -99999;
	
					for (i = 0 ; i < pheader->numframes ; i ++)
					{
						aliasframetype_t	frametype = LittleLong (pframetype->type);
	
						if (frametype == ALIAS_SINGLE)
							pframetype = (daliasframetype_t *) Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
						else
							pframetype = (daliasframetype_t *) Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
	
					}
				} // End of frames
			} // End of triangles
		} // End of st verts
	} //End of skins
	pheader->numposes = posenum;

	// bboxes fun

	for (i=0 ; i<3 ; i++) 
	{
		mod->mins[i] = aliasbboxmins[i] * pheader->scale[i] + pheader->scale_origin[i];
		mod->maxs[i] = aliasbboxmaxs[i] * pheader->scale[i] + pheader->scale_origin[i];
	}

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);


	// build the draw lists
	GL_MakeAliasModelDisplayLists (pheader);

	// move the complete, relocatable alias model to the cache
	{
		int end = Hunk_LowMark ();
		int total 	= end - hunk_start;

		Cache_Alloc (&mod->cache, total, loadname);

		if (!mod->cache.data)
			return;
	
		memcpy (mod->cache.data, pheader, total);
	}

	Hunk_FreeToLowMark (hunk_start);
}

//=============================================================================

/*
=================
Mod_LoadSpriteFrame
=================
*/
void * Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum)
{
	int					width, height, size, origin[2];
	dspriteframe_t		*pinframe;
	mspriteframe_t		*pspriteframe;
	char				name[64];

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = Hunk_AllocName (sizeof (mspriteframe_t),loadname);
	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	SNPrintf (name, sizeof(name), "%s_%i", loadmodel->name, framenum);
	pspriteframe->gl_texturenum = GL_LoadTexture (name, width, height, (byte *)(pinframe + 1), TEX_MIPMAP | TEX_ALPHA);

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}

/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum)
{
	int					i, numframes;
	float				*poutintervals;
	void				*ptemp;
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	dspriteinterval_t	*pin_intervals;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadSpriteGroup: interval %f <= 0 in %s", *poutintervals, loadmodel->name);

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++) 
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i, numframes, size;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	dspriteframetype_t	*pframetype;

	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

// load the frames
	if (numframes < 1)
		Sys_Error ("Mod_LoadSpriteModel: invalid # of frames %d in %s", numframes, mod->name);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) 
		{
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
		}
		else 
		{
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
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
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod = mod_known ; i < mod_numknown ; i++, mod++)
		Con_Printf ("%8p : %s\n",mod->cache.data, mod->name);
}
