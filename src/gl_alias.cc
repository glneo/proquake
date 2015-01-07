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
#include "glquake.h"
#include "model.h"

float shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
const float r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
	#include "anorm_dots.h"
};

static void GL_DrawAliasFrame(alias_model_t *aliasmodel, size_t frame, size_t pose, float light, float alpha)
{
	if (alpha < 1.0f)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	mpose_t *ppose = &aliasmodel->frames[frame].poses[pose];
	glVertexPointer(3, GL_FLOAT, sizeof(ppose->posevirts[0]), &ppose->posevirts[0].v);
//	glNormalPointer(GL_FLOAT, sizeof(ppose->posevirts[0]), &ppose->posevirts[0].normal);

	glTexCoordPointer(2, GL_FLOAT, sizeof(aliasmodel->frontstverts[0]), &aliasmodel->frontstverts->s);
	glDrawElements(GL_TRIANGLES, aliasmodel->backstart * 3, GL_UNSIGNED_SHORT, aliasmodel->triangles);
	glTexCoordPointer(2, GL_FLOAT, sizeof(aliasmodel->backstverts[0]), &aliasmodel->backstverts->s);
	glDrawElements(GL_TRIANGLES, (aliasmodel->numtris - aliasmodel->backstart) * 3, GL_UNSIGNED_SHORT, (aliasmodel->triangles + aliasmodel->backstart));

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	if (alpha < 1.0f)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

extern vec3_t lightspot;

static void GL_DrawAliasShadow(entity_t *ent, alias_model_t *aliasmodel, size_t frame, size_t pose)
{
	vec3_t shadevector;
	float an = ent->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	float lheight = ent->origin[2] - lightspot[2];
	float height = -lheight + 1.0;

	mpose_t *ppose = &aliasmodel->frames[frame].poses[pose];

	vec3_t shadowverts[aliasmodel->numverts];
	for (size_t i = 0; i < aliasmodel->numverts; i++)
	{
		shadowverts[i][0] = ppose->posevirts[i].v[0] - (shadevector[0] * lheight);
		shadowverts[i][1] = ppose->posevirts[i].v[1] - (shadevector[1] * lheight);
		shadowverts[i][2] = height;
	}

	glDisable(GL_TEXTURE_2D);
	glColor4f(0.0f, 0.0f, 0.0f, r_shadows.value);

	glVertexPointer(3, GL_FLOAT, sizeof(*shadowverts), shadowverts);
	glDrawElements(GL_TRIANGLES, aliasmodel->numtris * 3, GL_UNSIGNED_SHORT, aliasmodel->triangles);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
}

static size_t R_GetAliasPose(alias_model_t *aliasmodel, size_t frame)
{
	maliasframedesc_t *pframe = &aliasmodel->frames[frame];

	size_t numposes = pframe->numposes;
	if (numposes > 1)
	{
		double next_pose_time = pframe->next_pose_time;
		float interval = pframe->poses[pframe->current_pose].interval;
		// First time
		if (next_pose_time == 0.0)
			pframe->next_pose_time = cl.time + interval;
		else if (cl.time > next_pose_time)
		{
			pframe->next_pose_time = cl.time + interval;
			pframe->current_pose++;
			pframe->current_pose %= numposes;
		}
	}

	return pframe->current_pose;
}

void R_DrawAliasModel(entity_t *ent)
{
	bool isPlayer = ent > cl_entities &&
			ent <= (cl_entities + cl.maxclients);
	bool isViewent = ent == &cl.viewent;
	gltexture_t *tx;
	gltexture_t *fb;

	if (R_CullForEntity(ent))
		return;

	// get lighting information
	ambientlight = shadelight = R_LightPoint(ent->origin);

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

	// always give the gun some light
	if (isViewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	// no fullbright colors, so make torches full light
	if (ent->model->flags & MOD_FBRIGHT)
		ambientlight = shadelight = 255;

	shadelight = shadelight / 200.0;

	// locate the proper data
	alias_model_t *aliasmodel = ent->model->aliasmodel;

	// add the polys to our running total
	c_alias_polys += aliasmodel->numtris;

	// draw all the triangles

	GLfloat old_matrix[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, old_matrix);

	GLfloat matrix[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
	Q_Matrix modelViewMatrix;
	modelViewMatrix.set(matrix);

	ent->angles[0] = -ent->angles[0];	// stupid quake bug
	GL_RotateForEntity(ent, modelViewMatrix);
	ent->angles[0] = -ent->angles[0];	// stupid quake bug

	glLoadMatrixf(modelViewMatrix.get());

	int anim = (int) (cl.time * 10) & 3;
	if ((ent->skinnum < 0) ||
	    (ent->skinnum >= aliasmodel->numskins))
	{
		Con_DPrintf("no such skin # %d for '%s'\n", ent->skinnum, ent->model->name);
		tx = 0; // NULL will give the checkerboard texture
		fb = 0;
	}
	else
	{
		tx = aliasmodel->gl_texturenum[ent->skinnum][anim];
		fb = aliasmodel->gl_fbtexturenum[ent->skinnum][anim];
	}

	GL_Bind(tx);

	// hack the depth range to prevent view model from poking into walls
	if (isViewent)
	{
	#ifdef OPENGLES
			glDepthRangef(0, 0.3f);
	#else
			glDepthRange(0, 0.3);
	#endif
	}

	size_t frame = ent->frame;
	if (frame >= aliasmodel->numframes || frame < 0)
	{
		Con_DPrintf("no such frame %zu\n", frame);
		frame = 0;
	}
	size_t pose = R_GetAliasPose(aliasmodel, frame);

	if (gl_smoothmodels.value)
		glShadeModel(GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	float alpha = 1.0f;
	if (isViewent && (cl.items & IT_INVISIBILITY))
		alpha = r_ringalpha.value;

//	int quantizedangle = ((int)(ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1);
//	const float *shadedots = r_avertexnormal_dots[quantizedangle];
//	float light = shadedots[aliasmodel->poseverts[pose]->normalindex] * shadelight;
	float light = 0.0f;
	glColor4f(light, light, light, alpha);

	GL_DrawAliasFrame(aliasmodel, frame, pose, light, alpha);

	if (fb)
	{
		GL_Bind(fb);
		glBlendFunc (GL_ONE, GL_ONE);
		glDepthMask(GL_FALSE);

		GL_DrawAliasFrame(aliasmodel, frame, pose, light, alpha);

		glDepthMask(GL_TRUE);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	if (gl_affinemodels.value)
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (gl_smoothmodels.value)
		glShadeModel(GL_FLAT);

	if (r_shadows.value && !(ent->model->flags & MOD_NOSHADOW))
		GL_DrawAliasShadow(ent, aliasmodel, frame, pose);

	glLoadMatrixf(old_matrix);

	if (isViewent)
	{
#ifdef OPENGLES
		glDepthRangef(0, 1.0f);
#else
		glDepthRange(0, 1.0);
#endif
	}
}
