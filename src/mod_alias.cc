/*
 * Alias Model loading
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
#include "model.h"

#define NUMVERTEXNORMALS 162
const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
	#include "anorms.h"
};

static void Mod_CalcAliasBounds(model_t *mod, alias_model_t *aliasmodel)
{
	// clear out all data
	for (size_t i = 0; i < 3; i++)
	{
		mod->mins[i] = INT32_MAX;
		mod->maxs[i] = INT32_MIN;
	}

	// find min/max of all frames
	for (size_t i = 0; i < aliasmodel->numframes; i++)
	{
		maliasframedesc_t *frame = &aliasmodel->frames[i];

		for (int j = 0; j < 3; j++)
		{
			mod->mins[j] = min(mod->mins[j], frame->bboxmin[j]);
			mod->maxs[j] = max(mod->maxs[j], frame->bboxmin[j]);
		}
	}
}

static void *Mod_LoadAllFrames(alias_model_t *aliasmodel, daliasframetype_t *pframetype)
{
	for (size_t i = 0; i < aliasmodel->numframes; i++)
	{
		maliasframedesc_t *frame = &aliasmodel->frames[i];

		aliasframetype_t frametype = (aliasframetype_t)LittleLong(pframetype->type);
		daliasframe_t *pdaliasframe;
		if (frametype == ALIAS_SINGLE)
		{
			frame->numposes = 1;
			frame->poses = (mpose_t *)Q_malloc(sizeof(mpose_t) * frame->numposes);
			frame->poses[0].interval = 0.0f;
			pdaliasframe = (daliasframe_t *)(pframetype + 1);
		}
		else
		{
			daliasgroup_t *pdaliasgroup = (daliasgroup_t *)(pframetype + 1);
			frame->numposes = LittleLong(pdaliasgroup->numframes);
			frame->poses = (mpose_t *)Q_malloc(sizeof(mpose_t) * frame->numposes);
			daliasinterval_t *pin_intervals = (daliasinterval_t *)(pdaliasgroup + 1);
			float last_interval = 0.0f;
			for (size_t i = 0; i < frame->numposes; i++)
			{
				// store delta intervals
				float interval = LittleFloat(pin_intervals->interval);
				frame->poses[i].interval = interval - last_interval;
				last_interval = interval;
				pin_intervals++;
			}
			pdaliasframe = (daliasframe_t *)(pin_intervals);
		}
		frame->current_pose = 0;
		frame->next_pose_time = 0.0;
		for (size_t j = 0; j < 3; j++)
		{
			// these are byte values, so we don't have to worry about endianness
			frame->bboxmin[j] = (aliasmodel->scale[j] * pdaliasframe->bboxmin.v[j]) + aliasmodel->scale_origin[j];
			frame->bboxmax[j] = (aliasmodel->scale[j] * pdaliasframe->bboxmax.v[j]) + aliasmodel->scale_origin[j];
		}
		strcpy(frame->name, pdaliasframe->name);

		for (size_t j = 0; j < frame->numposes; j++)
		{
			dtrivertx_t *pinframe = (dtrivertx_t *)(pdaliasframe + 1);
			mpose_t *pose = &frame->poses[j];
			pose->posevirts = (mtrivertx_t *)Q_malloc(sizeof(mtrivertx_t) * aliasmodel->numverts);
			for (size_t k = 0; k < aliasmodel->numverts; k++)
			{
				mtrivertx_t *posevirts = &pose->posevirts[k];
				for (size_t l = 0; l < 3; l++)
				{
					posevirts->v[l] = (aliasmodel->scale[l] * pinframe[k].v[l]) + aliasmodel->scale_origin[l];
					posevirts->normal[l] = r_avertexnormals[pinframe[k].lightnormalindex][l];
				}
				posevirts->normalindex = pinframe[k].lightnormalindex;
			}

			pdaliasframe = (daliasframe_t *)(pinframe + aliasmodel->numverts);
		}

		pframetype = (daliasframetype_t *)pdaliasframe;
	}

	return pframetype;
}

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
static void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	int inpt = 0, outpt = 0;
	byte fillcolor = *skin; // assume this is the pixel to fill
	struct {
		short x, y;
	} fifo[FLOODFILL_FIFO_SIZE];

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == 0) || (fillcolor == 255))
	{
		//printf("not filling skin from %d to %d\n", fillcolor, filledcolor);
		return;
	}

	fifo[inpt].x = 0;
	fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;
	while (outpt != inpt)
	{
		int x = fifo[outpt].x;
		int y = fifo[outpt].y;
		int fdc = 0;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)              FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)  FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)              FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1) FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

static void *Mod_LoadAllSkins(alias_model_t *aliasmodel, daliasskintype_t *pskintype, const char *mod_name)
{
	size_t skinsize = aliasmodel->skinwidth * aliasmodel->skinheight;
	size_t groupskins;
	char name[32];
	byte *skin;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	aliasmodel->gl_texturenum = (gltexture_t *(*)[4])Q_malloc(sizeof(*aliasmodel->gl_texturenum) * aliasmodel->numskins);
	aliasmodel->gl_fbtexturenum = (gltexture_t *(*)[4])Q_malloc(sizeof(*aliasmodel->gl_texturenum) * aliasmodel->numskins);

	for (size_t i = 0; i < aliasmodel->numskins; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			skin = (byte *)(pskintype + 1);
			Mod_FloodFillSkin(skin, aliasmodel->skinwidth, aliasmodel->skinheight);

			if (Mod_CheckFullbrights(skin, aliasmodel->skinwidth * aliasmodel->skinheight))
			{
				snprintf(name, sizeof(name), "%s_%zu", mod_name, i);
				aliasmodel->gl_texturenum[i][0] =
				aliasmodel->gl_texturenum[i][1] =
				aliasmodel->gl_texturenum[i][2] =
				aliasmodel->gl_texturenum[i][3] = TexMgr_LoadImage(name, aliasmodel->skinwidth, aliasmodel->skinheight, SRC_INDEXED, skin, TEX_MIPMAP);

				snprintf(name, sizeof(name), "%s_%zu_glowwww", mod_name, i);
				aliasmodel->gl_fbtexturenum[i][0] =
				aliasmodel->gl_fbtexturenum[i][1] =
				aliasmodel->gl_fbtexturenum[i][2] =
				aliasmodel->gl_fbtexturenum[i][3] = TexMgr_LoadImage(name, aliasmodel->skinwidth, aliasmodel->skinheight, SRC_INDEXED, skin, TEX_MIPMAP | TEX_FULLBRIGHT);
			}
			else
			{
				snprintf(name, sizeof(name), "%s_%zu", mod_name, i);
				aliasmodel->gl_texturenum[i][0] =
				aliasmodel->gl_texturenum[i][1] =
				aliasmodel->gl_texturenum[i][2] =
				aliasmodel->gl_texturenum[i][3] = TexMgr_LoadImage(name, aliasmodel->skinwidth, aliasmodel->skinheight, SRC_INDEXED, skin, TEX_MIPMAP);

				aliasmodel->gl_fbtexturenum[i][0] =
				aliasmodel->gl_fbtexturenum[i][1] =
				aliasmodel->gl_fbtexturenum[i][2] =
				aliasmodel->gl_fbtexturenum[i][3] = NULL;
			}

			pskintype = (daliasskintype_t *)(skin + skinsize);
		}
		else
		{
			// animating skin group.  yuck.
			pinskingroup = (daliasskingroup_t *)(pskintype + 1);
			groupskins = LittleLong(pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			skin = (byte *)(pinskinintervals + groupskins);
			size_t j;
			for (j = 0; j < groupskins; j++)
			{
				Mod_FloodFillSkin(skin, aliasmodel->skinwidth, aliasmodel->skinheight);
				snprintf(name, sizeof(name), "%s_%zu_%zu", mod_name, i, j);
				aliasmodel->gl_texturenum[i][j & 3] = TexMgr_LoadImage(name, aliasmodel->skinwidth, aliasmodel->skinheight, SRC_INDEXED, skin, TEX_MIPMAP);
				skin += skinsize;
			}
			pskintype = (daliasskintype_t *)skin;

			for (size_t k = j; j < 4; j++)
				aliasmodel->gl_texturenum[i][j & 3] = aliasmodel->gl_texturenum[i][j - k];
		}
	}

	return (void *)pskintype;
}

void Mod_LoadAliasModel(model_t *mod, void *buffer)
{
	mdl_t *pinmodel = (mdl_t *)buffer;
	char *mod_name = mod->name;

	int version = LittleLong(pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod_name, version, ALIAS_VERSION);

	alias_model_t *aliasmodel = (alias_model_t *)Q_malloc(sizeof(*aliasmodel));

	// endian-adjust and copy the data, starting with the alias model header
	for (size_t i = 0; i < sizeof(vec3_t)/sizeof(vec_t); i++)
	{
		aliasmodel->scale[i] = LittleFloat(pinmodel->scale[i]);
		aliasmodel->scale_origin[i] = LittleFloat(pinmodel->scale_origin[i]);
		aliasmodel->eyeposition[i] = LittleFloat(pinmodel->eyeposition[i]);
	}
	aliasmodel->boundingradius = LittleFloat(pinmodel->boundingradius);
	aliasmodel->numskins = LittleLong(pinmodel->numskins);
	aliasmodel->skinwidth = LittleLong(pinmodel->skinwidth);
	aliasmodel->skinheight = LittleLong(pinmodel->skinheight);
	aliasmodel->numverts = LittleLong(pinmodel->numverts);
	aliasmodel->numtris = LittleLong(pinmodel->numtris);
	aliasmodel->numframes = LittleLong(pinmodel->numframes);

	// validate the setup
	if (aliasmodel->numskins == 0) Sys_Error("model %s has no skins", mod_name);
	if (aliasmodel->numskins > MAX_SKINS) Sys_Error ("model %s has invalid # of skins: %zu", mod_name, aliasmodel->numskins);
	if (aliasmodel->skinheight == 0) Sys_Error("model %s has a skin with no height", mod_name);
	if (aliasmodel->skinheight > MAX_SKIN_HEIGHT) Sys_Error("model %s has a skin taller than %d", mod_name, MAX_SKIN_HEIGHT);
	if (aliasmodel->skinwidth == 0) Sys_Error("model %s has a skin with no width", mod_name);
	if (aliasmodel->skinwidth > MAX_SKIN_WIDTH) Sys_Error("model %s has a skin wider than %d", mod_name, MAX_SKIN_WIDTH);
	if (aliasmodel->numverts == 0) Sys_Error("model %s has no vertices", mod_name);
	if (aliasmodel->numverts > MAXALIASVERTS) Sys_Error("model %s has invalid # of vertices: %zu", mod_name, aliasmodel->numverts);
	if (aliasmodel->numtris == 0) Sys_Error("model %s has no triangles", mod_name);
	if (aliasmodel->numtris > MAXALIASTRIS) Sys_Error("model %s has invalid # of triangles: %zu", mod_name, aliasmodel->numtris);
	if (aliasmodel->numframes == 0) Sys_Error("model %s has no frames", mod_name);

	// load the skins
	daliasskintype_t *pskintype = (daliasskintype_t *)(pinmodel + 1);
	pskintype = (daliasskintype_t *)Mod_LoadAllSkins(aliasmodel, pskintype, mod_name);

	// load texture cords s and t
	dstvert_t *pinstverts = (dstvert_t *)pskintype;
	aliasmodel->frontstverts = (mstvert_t *)Q_malloc(sizeof(*aliasmodel->frontstverts) * aliasmodel->numverts);
	aliasmodel->backstverts = (mstvert_t *)Q_malloc(sizeof(*aliasmodel->backstverts) * aliasmodel->numverts);
	for (size_t i = 0; i < aliasmodel->numverts; i++)
	{
		int s = LittleLong(pinstverts[i].s);
		int t = LittleLong(pinstverts[i].t);

		aliasmodel->frontstverts[i].s = (s + 0.5f) / aliasmodel->skinwidth;
		aliasmodel->frontstverts[i].t = (t + 0.5f) / aliasmodel->skinheight;

		if (pinstverts[i].onseam)
			s += (aliasmodel->skinwidth * 0.5f);

		aliasmodel->backstverts[i].s = (s + 0.5f) / aliasmodel->skinwidth;
		aliasmodel->backstverts[i].t = (t + 0.5f) / aliasmodel->skinheight;
	}

	// load triangle lists while sorting the front facing to the start of the array
	aliasmodel->backstart = aliasmodel->numtris - 1;
	size_t front_index = 0;
	dtriangle_t *pintriangles = (dtriangle_t *)&pinstverts[aliasmodel->numverts];
	aliasmodel->triangles = (mtriangle_t *)Q_malloc(sizeof(*aliasmodel->triangles) * aliasmodel->numtris);
	for (size_t i = 0; i < aliasmodel->numtris; i++)
	{
		short *pvert;
		if (pintriangles[i].facesfront)
			pvert = aliasmodel->triangles[front_index++].vertindex;
		else
			pvert = aliasmodel->triangles[aliasmodel->backstart--].vertindex;

		for (int j = 0; j < 3; j++)
			pvert[j] = LittleLong(pintriangles[i].vertindex[j]);
	}
	aliasmodel->backstart++;

	// load the frames
	daliasframetype_t *pframetype = (daliasframetype_t *)&pintriangles[aliasmodel->numtris];
	aliasmodel->frames = (maliasframedesc_t *)Q_malloc(sizeof(*aliasmodel->frames) * aliasmodel->numframes);
	pframetype = (daliasframetype_t *)Mod_LoadAllFrames(aliasmodel, pframetype);

	mod->numframes = aliasmodel->numframes;

	mod->type = mod_alias;
	mod->synctype = (synctype_t)LittleLong(pinmodel->synctype);
	mod->flags = LittleLong(pinmodel->flags);
	if (strcmp("progs/player.mdl", mod_name) == 0)
		mod->flags |= MOD_PLAYER;

	Mod_CalcAliasBounds(mod, aliasmodel);

	mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

	mod->aliasmodel = aliasmodel;
}
