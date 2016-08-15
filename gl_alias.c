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

vec3_t shadevector;
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

int lastposenum;
int lastposenum0;  // Interpolation

static void GL_DrawAliasFrame(alias_model_t *aliasmodel, int pose)
{
//	float alpha;
//
//	if (currententity != &cl.viewent)
//		alpha = 1.0f;
//	else
//	{
//		if(cl.items & IT_INVISIBILITY)
//			alpha = r_ringalpha.value;
//		else if (r_drawviewmodel.value)
//			alpha = 1.0f;
//		else
//			alpha = 0;
//	}
//
//
//
//	if (alpha < 1.0f)
//	{
//		glEnable(GL_BLEND);
//		glColor4f(1.0f, 1.0f, 1.0f, alpha);
//	}

	lastposenum = pose;

//	glBindTexture(GL_TEXTURE_2D, aliasmodel->gl_texturenum[0][0]);
//	glEnable(GL_TEXTURE_2D);

//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glVertexPointer(3, GL_FLOAT, sizeof(*(aliasmodel->poseverts[0])), &aliasmodel->poseverts[pose]->v);
//	glNormalPointer(GL_FLOAT, sizeof(*(aliasmodel->poseverts[0])), &aliasmodel->poseverts[pose]->normal);

	glTexCoordPointer(2, GL_FLOAT, sizeof(*(aliasmodel->stverts[0])), &aliasmodel->stverts[0]->s);
	glDrawElements(GL_TRIANGLES, aliasmodel->backstart * 3, GL_UNSIGNED_SHORT, aliasmodel->triangles);
	glTexCoordPointer(2, GL_FLOAT, sizeof(*(aliasmodel->stverts[1])), &aliasmodel->stverts[1]->s);
	glDrawElements(GL_TRIANGLES, (aliasmodel->numtris - aliasmodel->backstart) * 3, GL_UNSIGNED_SHORT, (aliasmodel->triangles + aliasmodel->backstart));

//	if (alpha < 1.0f)
//		glDisable(GL_BLEND);
}

/*
 =============
 GL_DrawAliasBlendedFrame

 fenix@io.com: model animation interpolation
 =============
 */
//void GL_DrawAliasBlendedFrame(aliashdr_t *paliashdr, int pose1, int pose2, float blend)
//{
//	float alpha; // Baker 3.80x - added alpha for r_ringalpha
//	float l;
//	trivertx_t* verts1;
//	trivertx_t* verts2;
//	int* order;
//	int count;
//	vec3_t d;
//
//	// Baker 3.80x - Transparent weapon (invisibility ring option)
//	alpha = (currententity == &cl.viewent) ?
//			((cl.items & IT_INVISIBILITY) ? (r_ringalpha.value < 1 ? r_ringalpha.value : 0) : (r_drawviewmodel.value ? 1 : 0)) : 1;
//
//	lastposenum0 = pose1;
//	lastposenum = pose2;
//
//	verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
//	verts2 = verts1;
//
//	verts1 += pose1 * paliashdr->poseverts;
//	verts2 += pose2 * paliashdr->poseverts;
//
//	order = (int *) ((byte *) paliashdr + paliashdr->commands);
//
//	if (alpha < 1)
//		glEnable(GL_BLEND);
//
//	for (;;)
//	{
//		// get the vertex count and primitive type
//		count = *order++;
//
//		if (!count)
//			break;
//
//		if (count < 0)
//		{
//			count = -count;
//			glBegin(GL_TRIANGLE_FAN);
//		}
//		else
//		{
//			glBegin(GL_TRIANGLE_STRIP);
//		}
//
//		do
//		{
//			// texture coordinates come from the draw list
//			glTexCoord2f(((float *) order)[0], ((float *) order)[1]);
//			order += 2;
//
//			// normals and vertexes come from the frame list
//			// blend the light intensity from the two frames together
//			d[0] = shadedots[verts2->lightnormalindex] - shadedots[verts1->lightnormalindex];
//
//			l = shadelight * (shadedots[verts1->lightnormalindex] + (blend * d[0]));
//			glColor4f(l, l, l, 1.0f);
//
//			VectorSubtract(verts2->v, verts1->v, d);
//
//			// blend the vertex positions from each frame together
//			glColor4f(l, l, l, alpha); // Baker 3.80x - transparent weapon
//			glVertex3f(verts1->v[0] + (blend * d[0]), verts1->v[1] + (blend * d[1]), verts1->v[2] + (blend * d[2]));
//
//			verts1++;
//			verts2++;
//		} while (--count);
//
//		glEnd();
//	}
//
//	if (alpha < 1)
//		glDisable(GL_BLEND);
//
//}

/*
 =============
 GL_DrawAliasShadow
 =============
 */
//extern vec3_t lightspot;

//void GL_DrawAliasShadow(aliashdr_t *paliashdr, int posenum)
//{
//	trivertx_t *verts;
//	int *order;
//	vec3_t point;
//	float height, lheight;
//	int count;
//
//	lheight = currententity->origin[2] - lightspot[2];
//
//	height = 0;
//	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
//	verts += posenum * paliashdr->poseverts;
//	order = (int *) ((byte *) paliashdr + paliashdr->commands);
//
//	height = -lheight + 1.0;
//
//	while (1)
//	{
//		// get the vertex count and primitive type
//		count = *order++;
//		if (!count)
//			break;		// done
//		if (count < 0)
//		{
//			count = -count;
//			glBegin(GL_TRIANGLE_FAN);
//		}
//		else
//		{
//			glBegin(GL_TRIANGLE_STRIP);
//		}
//
//		do
//		{
//			// texture coordinates come from the draw list
//			// (skipped for shadows) glTexCoord2fv ((float *)order);
//			order += 2;
//
//			// normals and vertexes come from the frame list
//			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
//			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
//			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
//
//			point[0] -= shadevector[0] * (point[2] + lheight);
//			point[1] -= shadevector[1] * (point[2] + lheight);
//			point[2] = height;
////			height -= 0.001;
//			glVertex3fv(point);
//
//			verts++;
//		} while (--count);
//
//		glEnd();
//	}
//}
//
///*
// =============
// GL_DrawAliasBlendedShadow
//
// fenix@io.com: model animation interpolation
// =============
// */
//void GL_DrawAliasBlendedShadow(aliashdr_t *paliashdr, int pose1, int pose2, entity_t* e)
//{
//	trivertx_t* verts1;
//	trivertx_t* verts2;
//	int* order;
//	vec3_t point1;
//	vec3_t point2;
//	vec3_t d;
//	float height;
//	float lheight;
//	int count;
//	float blend;
//
//	blend = (cl.time - e->frame_start_time) / e->frame_interval;
//
//	if (blend > 1)
//		blend = 1;
//
//	lheight = e->origin[2] - lightspot[2];
//	height = 1.0 - lheight;
//
//	verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
//	verts2 = verts1;
//
//	verts1 += pose1 * paliashdr->poseverts;
//	verts2 += pose2 * paliashdr->poseverts;
//
//	order = (int *) ((byte *) paliashdr + paliashdr->commands);
//
//	for (;;)
//	{
//		// get the vertex count and primitive type
//		count = *order++;
//
//		if (!count)
//			break;
//
//		if (count < 0)
//		{
//			count = -count;
//			glBegin(GL_TRIANGLE_FAN);
//		}
//		else
//		{
//			glBegin(GL_TRIANGLE_STRIP);
//		}
//
//		do
//		{
//			order += 2;
//
//			point1[0] = verts1->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
//			point1[1] = verts1->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
//			point1[2] = verts1->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
//
//			point1[0] -= shadevector[0] * (point1[2] + lheight);
//			point1[1] -= shadevector[1] * (point1[2] + lheight);
//
//			point2[0] = verts2->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
//			point2[1] = verts2->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
//			point2[2] = verts2->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];
//
//			point2[0] -= shadevector[0] * (point2[2] + lheight);
//			point2[1] -= shadevector[1] * (point2[2] + lheight);
//
//			VectorSubtract(point2, point1, d);
//
//			glVertex3f(point1[0] + (blend * d[0]), point1[1] + (blend * d[1]), height);
//
//			verts1++;
//			verts2++;
//		} while (--count);
//
//		glEnd();
//	}
//}

static void R_SetupAliasFrame(int frame, alias_model_t *aliasmodel)
{
	int pose, numposes;
	float interval;

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

	GL_DrawAliasFrame(aliasmodel, pose);
}

/*
 =================
 R_SetupAliasBlendedFrame

 fenix@io.com: model animation interpolation
 =================
 */
//void R_SetupAliasBlendedFrame(int frame, aliashdr_t *paliashdr, entity_t* e)
//{
//	int pose;
//	int numposes;
//	float blend;
//
//	if ((frame >= paliashdr->numframes) || (frame < 0))
//	{
//		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
//		frame = 0;
//	}
//
//	pose = paliashdr->frames[frame].firstpose;
//	numposes = paliashdr->frames[frame].numposes;
//
//	if (numposes > 1)
//	{
//		e->frame_interval = paliashdr->frames[frame].interval;
//		pose += (int) (cl.time / e->frame_interval) % numposes;
//	}
//	else
//	{
//// One tenth of a second is a good for most Quake animations.
//// If the nextthink is longer then the animation is usually meant to pause
//// (e.g. check out the shambler magic animation in shambler.qc). If its
//// shorter then things will still be smoothed partly, and the jumps will be
//// less noticable because of the shorter time. So, this is probably a good assumption.
//		e->frame_interval = 0.1;
//	}
//
//	if (e->currpose != pose)
//	{
//		e->frame_start_time = cl.time;
//		e->lastpose = e->currpose;
//		e->currpose = pose;
//		blend = 0;
//	}
//	else
//	{
//		blend = (cl.time - e->frame_start_time) / e->frame_interval;
//	}
//
//	// weird things start happening if blend passes 1
//	if (cl.paused || blend > 1)
//		blend = 1;
//
//	GL_DrawAliasBlendedFrame(paliashdr, e->lastpose, e->currpose, blend);
//}

void R_DrawAliasModel(entity_t *ent)
{
	int client_no = currententity - cl_entities;
	bool isPlayer = (client_no >= 1 && client_no <= cl.maxclients) ? true : false;
	int lnum;
	vec3_t dist;
	float add;
	model_t *clmodel = currententity->model;
	vec3_t mins, maxs;
	alias_model_t *aliasmodel;
	float an;
	int anim;

	bool torch = false; // Flags is this model is a torch

	VectorAdd(currententity->origin, clmodel->mins, mins);
	VectorAdd(currententity->origin, clmodel->maxs, maxs);

//	if (R_CullBox (mins, maxs))
//		return;

	if (R_CullForEntity(ent))
		return;

	VectorSubtract(r_origin, currententity->origin, modelorg);

	// get lighting information

	ambientlight = shadelight = R_LightPoint(currententity->origin);

	// always give the gun some light
	if (ent == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract(currententity->origin, cl_dlights[lnum].origin, dist);
			add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
			{
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// ZOID: never allow players to go totally black
	//client_no = currententity - cl_entities;
	if (isPlayer /*client_no >= 1 && client_no<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		if (ambientlight < 8)
			ambientlight = shadelight = 8;

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!strcmp(clmodel->name, "progs/flame2.mdl") || !strcmp(clmodel->name, "progs/flame.mdl") || !strcmp(clmodel->name, "progs/bolt2.mdl"))// JPG 3.20 - LG should be fullbright too
	{
		ambientlight = shadelight = 255;	// JPG 3.02 - was 256
		torch = true; // This model is a torch. KH
	}

	ambientlight = gammatable[(int) ambientlight];	// JPG 3.02 - gamma correction
	shadelight = gammatable[(int) shadelight];		// JPG 3.02 - gamma correction

	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;

	an = ent->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	// locate the proper data
	aliasmodel = (alias_model_t *)currententity->model->aliasmodel;

	c_alias_polys += aliasmodel->numtris;

	// draw all the triangles

	GL_DisableMultitexture();

	glPushMatrix();

	R_RotateForEntity(ent);

	anim = (int) (cl.time * 10) & 3;

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
//	if (1 <= currententity->colormap && currententity->colormap <= MAX_SCOREBOARD &&	// color map is valid
//			!gl_nocolors.value &&												// No colors isn't on
//			(currententity->model->flags & MOD_PLAYER) &&									// And is a player model
//			(r_colored_dead_bodies.value || isPlayer))					// Either we want colored dead bodies or it is a player
//		GL_Bind(playertextures - 1 + currententity->colormap /*client_no*/);
//	else
		GL_Bind(aliasmodel->gl_texturenum[currententity->skinnum][anim]);

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	// fenix@io.com: model animation interpolation
	// Baker: r_interpolate_weapon here!
//	if (r_interpolate_animation.value)
//	{
//		if (client_no == 1)
//			Con_Debugf("Player entity 1 current num is %i %s\n", currententity, currententity->model->name);
//		if (client_no == 0)
//			Con_Debugf("Player entity 0 current num is %i %s\n", currententity, currententity->model->name);
//		R_SetupAliasBlendedFrame(currententity->frame, aliasmodel, currententity);
//	}
//	else
//	{
		R_SetupAliasFrame(currententity->frame, aliasmodel);
//	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel(GL_FLAT);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix();

	// aguirRe ... no shadows if r_fullbright or unlit world
	if (r_shadows.value && !r_fullbright.value && cl.worldmodel->brushmodel->lightdata)
	{
		//
		// Test for models that we don't want to shadow. KH
		// Not a nice way to do it...
		//

		// Torches. Early-out to avoid the strcmp's. KH
		if (torch)
			return;
		// Grenades. KH
		if (!strcmp(clmodel->name, "progs/grenade.mdl"))
			return;
		// Lightning bolts. KH
		if (!strncmp(clmodel->name, "progs/bolt", 10))
			return;
		glPushMatrix();
		R_RotateForEntity(ent);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);

		// Baker 3.60 - interpolation plus flicker fix
		// Quick-fix issue with self-overlapping alias triangles.
		//glColor4f (0,0,0,0.5); // Original.
		// glColor4f (0.0f, 0.0f, 0.0f, 1.0f); // KH
		glColor4f(0.0f, 0.0f, 0.0f, 0.5f /* r_shadows.value */); // KH

		// fenix@io.com: model animation interpolation
//		if (r_interpolate_animation.value)
//		{
//			GL_DrawAliasBlendedShadow(aliasmodel, lastposenum0, lastposenum, currententity);
//		}
//		else
//		{
//			GL_DrawAliasShadow(aliasmodel, lastposenum);
//		}

		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
		glPopMatrix();
	}
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
		for (int k = 0; k < 3; k++)
		{
			aliasmodel->poseverts[*posenum][j].v[k] = (aliasmodel->scale[k] * pinframe[j].v[k]) + aliasmodel->scale_origin[k];
			aliasmodel->poseverts[*posenum][j].normal[k] = r_avertexnormals[pinframe[j].lightnormalindex][k];
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
			for (int l = 0; l < 3; l++)
			{
				aliasmodel->poseverts[*posenum][k].v[l] = (aliasmodel->scale[l] * pinframe[k].v[l]) + aliasmodel->scale_origin[l];
				aliasmodel->poseverts[*posenum][k].normal[l] = r_avertexnormals[pinframe[k].lightnormalindex][l];
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
	byte *skin, *texels;
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
