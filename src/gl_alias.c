/*
 * Alias Model loading and rendering
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

#include "gl_model.h"

#define NUMVERTEXNORMALS 162

float shadelight, ambientlight;

const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
	#include "anorms.h"
};

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
	#include "anorm_dots.h"
};

float *shadedots = r_avertexnormal_dots[0];

static void GL_DrawAliasFrame(entity_t *ent, alias_model_t *aliasmodel, int pose)
{
	float alpha;

	if (ent != &cl.viewent)
		alpha = 1.0f;
	else
	{
		if(cl.items & IT_INVISIBILITY)
			alpha = r_ringalpha.value;
		else
			alpha = 1.0f;
	}

	if (alpha < 1.0f)
		glEnable(GL_BLEND);

	float l = shadedots[aliasmodel->poseverts[pose]->normalindex] * shadelight;
	glColor4f(l, l, l, alpha);

//	glBindTexture(GL_TEXTURE_2D, aliasmodel->gl_texturenum[0][0]);
//	glEnable(GL_TEXTURE_2D);

//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glVertexPointer(3, GL_FLOAT, sizeof(*(aliasmodel->poseverts[0])), &aliasmodel->poseverts[pose]->v);
//	glNormalPointer(GL_FLOAT, sizeof(*(aliasmodel->poseverts[0])), &aliasmodel->poseverts[pose]->normal);

	glTexCoordPointer(2, GL_FLOAT, sizeof(*(aliasmodel->stverts[0])), &aliasmodel->stverts[0]->s);
	glDrawElements(GL_TRIANGLES, aliasmodel->backstart * 3, GL_UNSIGNED_SHORT, aliasmodel->triangles);
	glTexCoordPointer(2, GL_FLOAT, sizeof(*(aliasmodel->stverts[1])), &aliasmodel->stverts[1]->s);
	glDrawElements(GL_TRIANGLES, (aliasmodel->numtris - aliasmodel->backstart) * 3, GL_UNSIGNED_SHORT, (aliasmodel->triangles + aliasmodel->backstart));

	if (alpha < 1.0f)
		glDisable(GL_BLEND);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

extern vec3_t lightspot;

static void GL_DrawAliasShadow(entity_t *ent, alias_model_t *aliasmodel, int pose)
{
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glColor4f(0.0f, 0.0f, 0.0f, r_shadows.value);

	vec3_t point;
	float height, lheight;

	vec3_t shadevector;

	float an = ent->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	lheight = ent->origin[2] - lightspot[2];
	height = -lheight + 1.0;

	vec3_t shadowverts[aliasmodel->numverts];
	for (int i = 0; i < aliasmodel->numverts; i++)
	{
		shadowverts[i][0] = aliasmodel->poseverts[pose][i].v[0] - (shadevector[0] * (point[2] + lheight));
		shadowverts[i][1] = aliasmodel->poseverts[pose][i].v[1] - (shadevector[1] * (point[2] + lheight));
		shadowverts[i][2] = height;
	}

	glVertexPointer(3, GL_FLOAT, sizeof(*shadowverts), shadowverts);
	glDrawElements(GL_TRIANGLES, aliasmodel->numtris * 3, GL_UNSIGNED_SHORT, aliasmodel->triangles);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
}

static int R_GetAliasPose(entity_t *ent, alias_model_t *aliasmodel)
{
	int pose, numposes;
	float interval;
	int frame = ent->frame;

	if ((frame >= aliasmodel->numframes) || (frame < 0))
	{
		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = aliasmodel->frames[frame].firstpose;
	numposes = aliasmodel->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = aliasmodel->frames[frame].interval;
		pose += (int) (cl.time / interval) % numposes;
	}

	return pose;
}

void R_DrawAliasModel(entity_t *ent)
{

	bool isPlayer = ent > cl_entities &&
			ent <= (cl_entities + cl.maxclients);
	int tx;

//	vec3_t mins, maxs;
//
//	VectorAdd(ent->origin, ent->model->mins, mins);
//	VectorAdd(ent->origin, ent->model->maxs, maxs);
//
//	if (R_CullBox (mins, maxs))
//		return;

	if (R_CullForEntity(ent))
		return;

	// get lighting information
	ambientlight = shadelight = R_LightPoint(ent->origin);

	// always give the gun some light
	if (ent == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			vec3_t dist;
			VectorSubtract(ent->origin, cl_dlights[lnum].origin, dist);
			float add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
			{
				ambientlight += add;
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// never allow players to go totally black
	if (isPlayer && ambientlight < 8)
		ambientlight = shadelight = 8;

	// no fullbright colors, so make torches full light
	if (ent->model->flags & MOD_FBRIGHT)
		ambientlight = shadelight = 255;

	int quantizedangle = ((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1);

	shadedots = r_avertexnormal_dots[quantizedangle];
	shadelight = shadelight / 200.0;

	// locate the proper data
	alias_model_t *aliasmodel = ent->model->aliasmodel;

	// add the polys to our running total
	c_alias_polys += aliasmodel->numtris;

	// draw all the triangles

	glPushMatrix();
	R_RotateForEntity(ent);

	int anim = (int) (cl.time * 10) & 3;
	if ((ent->skinnum < 0) ||
	    (ent->skinnum >= aliasmodel->numskins))
	{
		Con_DPrintf("no such skin # %d for '%s'\n", ent->skinnum, ent->model->name);
		tx = NULL; // NULL will give the checkerboard texture
	}
	else
	{
		tx = aliasmodel->gl_texturenum[ent->skinnum][anim];
	}

	GL_Bind(tx);

	int pose = R_GetAliasPose(ent, aliasmodel);

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_DrawAliasFrame(ent, aliasmodel, pose);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (gl_smoothmodels.value)
		glShadeModel(GL_FLAT);

	if (r_shadows.value && !(ent->model->flags & MOD_NOSHADOW))
		GL_DrawAliasShadow(ent, aliasmodel, pose);

	glPopMatrix();
}

static void Mod_CalcAliasBounds(model_t *mod, alias_model_t *aliasmodel)
{
	//clear out all data
	for (int i = 0; i < 3; i++)
	{
		mod->mins[i] = INT32_MAX;
		mod->maxs[i] = INT32_MIN;
	}

	//process verts
	for (int i = 0; i < aliasmodel->numposes; i++)
	{
		for (int j = 0; j < aliasmodel->numverts; j++)
		{
			vec3_t v;

			for (int k = 0; k < 3; k++)
				v[k] = aliasmodel->poseverts[i][j].v[k];

			for (int k = 0; k < 3; k++)
			{
				mod->mins[k] = min(mod->mins[k], v[k]);
				mod->maxs[k] = max(mod->maxs[k], v[k]);
			}
		}
	}
}

static void *Mod_LoadAliasFrame(alias_model_t *aliasmodel, int *posenum, int i, daliasframetype_t *pframetype)
{
	maliasframedesc_t *frame = &aliasmodel->frames[i];
	daliasframe_t *pdaliasframe = (daliasframe_t *)(pframetype + 1);

	strcpy(aliasmodel->frames[i].name, pdaliasframe->name);
	frame->firstpose = *posenum;
	frame->numposes = 1;

	for (int j = 0; j < 3; j++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[j] = pdaliasframe->bboxmin.v[j];
		frame->bboxmax[j] = pdaliasframe->bboxmax.v[j];
	}

	dtrivertx_t *pinframe = (dtrivertx_t *)(pdaliasframe + 1);

//	aliasmodel->poseverts = Q_realloc(aliasmodel->poseverts, sizeof(*aliasmodel->poseverts) * (posenum + 1));
	aliasmodel->poseverts[*posenum] = Q_malloc(sizeof(mtrivertx_t) * aliasmodel->numverts);
	for (int j = 0; j < aliasmodel->numverts; j++)
	{
		for (int k = 0; k < 3; k++)
		{
			aliasmodel->poseverts[*posenum][j].v[k] = (aliasmodel->scale[k] * pinframe[j].v[k]) + aliasmodel->scale_origin[k];
//			aliasmodel->poseverts[*posenum][j].normal[k] = r_avertexnormals[pinframe[j].lightnormalindex][k];
		}
		aliasmodel->poseverts[*posenum][j].normalindex = pinframe[j].lightnormalindex;
	}

	(*posenum)++;

	return (pinframe + aliasmodel->numverts);
}

static void *Mod_LoadAliasGroup(alias_model_t *aliasmodel, int *posenum, int i, daliasframetype_t *pframetype)
{
	maliasframedesc_t *frame = &aliasmodel->frames[i];
	daliasgroup_t *pdaliasgroup = (daliasgroup_t *)(pframetype + 1);
	int numsubframes = LittleLong(pdaliasgroup->numframes);

	frame->firstpose = *posenum;
	frame->numposes = numsubframes;

	for (int j = 0; j < 3; j++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[j] = pdaliasgroup->bboxmin.v[j];
		frame->bboxmax[j] = pdaliasgroup->bboxmax.v[j];
	}

	daliasinterval_t *pin_intervals = (daliasinterval_t *) (pdaliasgroup + 1);
	frame->interval = LittleFloat(pin_intervals->interval);
	daliasframe_t *pdaliasframe = (daliasframe_t *)(pin_intervals + numsubframes);

	for (int j = 0; j < numsubframes; j++)
	{
		dtrivertx_t *pinframe = (dtrivertx_t *)(pdaliasframe + 1);
		aliasmodel->poseverts[*posenum] = Q_malloc(sizeof(mtrivertx_t) * aliasmodel->numverts);
		for (int k = 0; k < aliasmodel->numverts; k++)
		{
			for (int l = 0; l < 3; l++)
				aliasmodel->poseverts[*posenum][k].v[l] = (aliasmodel->scale[l] * pinframe[k].v[l]) + aliasmodel->scale_origin[l];
			aliasmodel->poseverts[*posenum][k].normalindex = pinframe[k].lightnormalindex;
		}
		(*posenum)++;
		pdaliasframe = (daliasframe_t *)(pinframe + aliasmodel->numverts);
	}

	return pdaliasframe;
}

static void *Mod_LoadAllFrames(alias_model_t *aliasmodel, daliasframetype_t *pframetype)
{
	int posenum = 0;

	for (int i = 0; i < aliasmodel->numframes; i++)
	{
		aliasframetype_t frametype = LittleLong(pframetype->type);
		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame(aliasmodel, &posenum, i, pframetype);
		else
			pframetype = (daliasframetype_t *)Mod_LoadAliasGroup(aliasmodel, &posenum, i, pframetype);
	}

	aliasmodel->numposes = posenum;

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
	int skinsize = aliasmodel->skinwidth * aliasmodel->skinheight;
	int groupskins;
	char name[32];
	byte *skin;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	aliasmodel->gl_texturenum = Q_malloc(sizeof(*aliasmodel->gl_texturenum) * aliasmodel->numskins);

	for (int i = 0; i < aliasmodel->numskins; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			skin = (byte *)(pskintype + 1);
			Mod_FloodFillSkin(skin, aliasmodel->skinwidth, aliasmodel->skinheight);

			snprintf(name, sizeof(name), "%s_%i", mod_name, i);
			aliasmodel->gl_texturenum[i][0] =
			aliasmodel->gl_texturenum[i][1] =
			aliasmodel->gl_texturenum[i][2] =
			aliasmodel->gl_texturenum[i][3] = GL_LoadTexture(name, aliasmodel->skinwidth, aliasmodel->skinheight, skin, TEX_MIPMAP);
			pskintype = (daliasskintype_t *)(skin + skinsize);
		}
		else
		{
			// animating skin group.  yuck.
			pinskingroup = (daliasskingroup_t *)(pskintype + 1);
			groupskins = LittleLong(pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			skin = (void *)(pinskinintervals + groupskins);
			int j;
			for (j = 0; j < groupskins; j++)
			{
				Mod_FloodFillSkin(skin, aliasmodel->skinwidth, aliasmodel->skinheight);
				snprintf(name, sizeof(name), "%s_%i_%i", mod_name, i, j);
				aliasmodel->gl_texturenum[i][j & 3] = GL_LoadTexture(name, aliasmodel->skinwidth, aliasmodel->skinheight, skin, TEX_MIPMAP);
				skin += skinsize;
			}
			pskintype = (daliasskintype_t *)skin;

			for (int k = j; j < 4; j++)
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

	alias_model_t *aliasmodel = Q_malloc(sizeof(*aliasmodel));

	// endian-adjust and copy the data, starting with the alias model header
	for (int i = 0; i < 3; i++)
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
	if (aliasmodel->numskins <= 0) Sys_Error("model %s has no skins", mod_name);
	if (aliasmodel->numskins > MAX_SKINS) Sys_Error ("model %s has invalid # of skins: %d\n", mod_name, aliasmodel->numskins);
	if (aliasmodel->skinheight <= 0) Sys_Error("model %s has a skin with no height", mod_name);
	if (aliasmodel->skinheight > MAX_SKIN_HEIGHT) Sys_Error("model %s has a skin taller than %d", mod_name, MAX_SKIN_HEIGHT);
	if (aliasmodel->skinwidth <= 0) Sys_Error("model %s has a skin with no width", mod_name);
	if (aliasmodel->skinwidth > MAX_SKIN_WIDTH) Sys_Error("model %s has a skin wider than %d", mod_name, MAX_SKIN_WIDTH);
	if (aliasmodel->numverts <= 0) Sys_Error("model %s has no vertices", mod_name);
	if (aliasmodel->numverts > MAXALIASVERTS) Sys_Error("model %s has invalid # of vertices: %d", mod_name, aliasmodel->numverts);
	if (aliasmodel->numtris <= 0) Sys_Error("model %s has no triangles", mod_name);
	if (aliasmodel->numtris > MAXALIASTRIS) Sys_Error("model %s has invalid # of triangles: %d", mod_name, aliasmodel->numtris);
	if (aliasmodel->numframes <= 0) Sys_Error("model %s has no frames", mod_name);

	// load the skins
	daliasskintype_t *pskintype = (daliasskintype_t *)(pinmodel + 1);
	pskintype = Mod_LoadAllSkins(aliasmodel, pskintype, mod_name);

	// load texture cords s and t
	dstvert_t *pinstverts = (dstvert_t *)pskintype;
	aliasmodel->stverts[0] = Q_malloc(sizeof(*aliasmodel->stverts) * aliasmodel->numverts);
	aliasmodel->stverts[1] = Q_malloc(sizeof(*aliasmodel->stverts) * aliasmodel->numverts);
	for (int i = 0; i < aliasmodel->numverts; i++)
	{
		int s = LittleLong(pinstverts[i].s);
		int t = LittleLong(pinstverts[i].t);

		aliasmodel->stverts[0][i].s = (s + 0.5f) / aliasmodel->skinwidth;
		aliasmodel->stverts[0][i].t = (t + 0.5f) / aliasmodel->skinheight;

		if (pinstverts[i].onseam)
			s += (aliasmodel->skinwidth * 0.5f);

		aliasmodel->stverts[1][i].s = (s + 0.5f) / aliasmodel->skinwidth;
		aliasmodel->stverts[1][i].t = (t + 0.5f) / aliasmodel->skinheight;
	}

	// load triangle lists while sorting the front facing to the start of the array
	aliasmodel->backstart = aliasmodel->numtris - 1;
	int front_index = 0;
	dtriangle_t *pintriangles = (dtriangle_t *)&pinstverts[aliasmodel->numverts];
	aliasmodel->triangles = Q_malloc(sizeof(*aliasmodel->triangles) * aliasmodel->numtris);
	for (int i = 0; i < aliasmodel->numtris; i++)
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
	aliasmodel->frames = Q_malloc(sizeof(*aliasmodel->frames) * aliasmodel->numframes);
	pframetype = Mod_LoadAllFrames(aliasmodel, pframetype);

	mod->numframes = aliasmodel->numframes;

	mod->type = mod_alias;
	mod->synctype = LittleLong(pinmodel->synctype);
	mod->flags = LittleLong(pinmodel->flags);
	if (strcmp("progs/player.mdl", mod_name) == 0)
		mod->flags |= MOD_PLAYER;

	Mod_CalcAliasBounds(mod, aliasmodel);

	mod->radius = RadiusFromBounds(mod->mins, mod->maxs);

	mod->aliasmodel = aliasmodel;
}
