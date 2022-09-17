/*
 * Alias model rendering
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

#include <string>
#include <cstddef>

#include "quakedef.h"
#include "glquake.h"
#include "model.h"

#include "alias.fs.h"
#include "alias.vs.h"

// shader program
static GLuint alias_program;

// uniforms used in vertex shader
static GLuint alias_shadevectorLoc;
static GLuint alias_lightColorLoc;

// uniforms used in fragment shader
static GLuint alias_texLoc;
static GLuint alias_fullbrightTexLoc;
static GLuint alias_useFullbrightTexLoc;
static GLuint alias_projectionLoc;
static GLuint alias_modelViewLoc;

// attributes
static GLuint alias_vertexAttrIndex;
static GLuint alias_normalAttrIndex;
static GLuint alias_texCoordsAttrIndex;

void GL_UploadAliasVBOs(alias_model_t *aliasmodel)
{
	glGenBuffers(1, &aliasmodel->texvertsVBO);
	glBindBuffer(GL_ARRAY_BUFFER, aliasmodel->texvertsVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(aliasmodel->texverts[0]) * aliasmodel->numverts, &aliasmodel->texverts[0], GL_STATIC_DRAW);

	for (size_t i = 0; i < aliasmodel->numframes; i++)
	{
		maliasframedesc_t *frame = &aliasmodel->frames[i];

		for (size_t j = 0; j < frame->numposes; j++)
		{
			mpose_t *pose = &frame->poses[j];

			glGenBuffers(1, &pose->posevertsVBO);
			glBindBuffer(GL_ARRAY_BUFFER, pose->posevertsVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(pose->poseverts[0]) * aliasmodel->numverts, &pose->poseverts[0], GL_STATIC_DRAW);
		}
	}

	glGenBuffers(1, &aliasmodel->trianglesVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, aliasmodel->trianglesVBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(aliasmodel->triangles[0]) * aliasmodel->numtris, &aliasmodel->triangles[0], GL_STATIC_DRAW);
}

static void GL_DrawAliasFrame(alias_model_t *aliasmodel, size_t frame, size_t pose, vec3_t shadevector, float light, float alpha)
{
	mpose_t *ppose = &aliasmodel->frames[frame].poses[pose];
	glBindBuffer(GL_ARRAY_BUFFER, ppose->posevertsVBO);
	glEnableVertexAttribArray(alias_vertexAttrIndex);
	glVertexAttribPointer(alias_vertexAttrIndex, 3, GL_FLOAT, GL_FALSE, sizeof(ppose->poseverts[0]), BUFFER_OFFSET(offsetof(mtrivertx_t, v)));
	glEnableVertexAttribArray(alias_normalAttrIndex);
	glVertexAttribPointer(alias_normalAttrIndex, 3, GL_FLOAT, GL_FALSE, sizeof(ppose->poseverts[0]), BUFFER_OFFSET(offsetof(mtrivertx_t, normal)));

	glBindBuffer(GL_ARRAY_BUFFER, aliasmodel->texvertsVBO);
	glEnableVertexAttribArray(alias_texCoordsAttrIndex);
	glVertexAttribPointer(alias_texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, sizeof(aliasmodel->texverts[0]), BUFFER_OFFSET(offsetof(mstvert_t, s)));

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, aliasmodel->trianglesVBO);
	glDrawElements(GL_TRIANGLES, aliasmodel->numtris * 3, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));

	glDisableVertexAttribArray(alias_texCoordsAttrIndex);
	glDisableVertexAttribArray(alias_normalAttrIndex);
	glDisableVertexAttribArray(alias_vertexAttrIndex);
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

void GL_DrawAliasModel(entity_t *ent)
{
	bool isPlayer = ent > cl_entities &&
			ent <= (cl_entities + cl.maxclients);
	bool isViewent = ent == &cl.viewent;
	gltexture_t *tx;
	gltexture_t *fb;

	// get lighting information
	float ambientlight = R_LightPoint(ent->origin);

//	if (shadelight)
//		Con_Printf("Have a close DLIGHT\n");

//	 clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;

	// never allow players to go totally black
	if (isPlayer && ambientlight < 8)
		ambientlight = 8;

	// always give the gun some light
	if (isViewent && ambientlight < 24)
		ambientlight = 24;

	// no fullbright colors, so make torches full light
//	if (ent->model->flags & MOD_FBRIGHT)
//		ambientlight = shadelight = 255;

//	shadelight /= 128.0;
	ambientlight /= 128.0;

	float rad = ent->angles[1] / 180 * M_PI;
	vec3_t shadevector;
	shadevector[0] = cos(-rad);
	shadevector[1] = sin(-rad);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	// locate the proper data
	alias_model_t *aliasmodel = ent->model->aliasmodel;

	// add the polys to our running total
	c_alias_polys += aliasmodel->numtris;

	// Push
	GLfloat mv_matrix[16];
	memcpy(mv_matrix, modelViewMatrix.get(), sizeof(mv_matrix));

	ent->angles[0] = -ent->angles[0];	// stupid quake bug
	GL_RotateForEntity(ent, modelViewMatrix);
	ent->angles[0] = -ent->angles[0];	// stupid quake bug

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

	size_t frame = ent->frame;
	if (frame >= aliasmodel->numframes || frame < 0)
	{
		Con_DPrintf("no such frame %zu\n", frame);
		frame = 0;
	}
	size_t pose = R_GetAliasPose(aliasmodel, frame);

	float alpha = 1.0f;
	if (isViewent && (cl.items & IT_INVISIBILITY))
		alpha = r_ringalpha.value;

// setup

	glUseProgram(alias_program);
	if (alpha < 1.0f)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	GL_BindToUnit(GL_TEXTURE0, tx);
	if (fb)
		GL_BindToUnit(GL_TEXTURE1, fb);

// set uniforms
	glUniformMatrix4fv(alias_projectionLoc, 1, GL_FALSE, projectionMatrix.get());
	glUniformMatrix4fv(alias_modelViewLoc, 1, GL_FALSE, modelViewMatrix.get());

	glUniform3f(alias_shadevectorLoc, shadevector[0], shadevector[1], shadevector[2]);
	glUniform4f(alias_lightColorLoc, ambientlight, ambientlight, ambientlight, alpha);
	glUniform1i(alias_texLoc, 0);
	glUniform1i(alias_fullbrightTexLoc, 1);
	glUniform1i(alias_useFullbrightTexLoc, fb ? GL_TRUE : GL_FALSE);

	// hack the depth range to prevent view model from poking into walls
	if (isViewent)
	{
#ifdef OPENGLES
		glDepthRangef(0, 0.3f);
#else
		glDepthRange(0, 0.3);
#endif
	}

	GL_DrawAliasFrame(aliasmodel, frame, pose, shadevector, ambientlight, alpha);

	// cleanup
	if (isViewent)
	{
#ifdef OPENGLES
		glDepthRangef(0, 1.0f);
#else
		glDepthRange(0, 1.0);
#endif
	}
	if (alpha < 1.0f)
		glDisable(GL_BLEND);

	// Pop
	modelViewMatrix.set(mv_matrix);
}

void GL_CreateAliasShaders(void)
{
	// generate program
	alias_program = LoadShader((const char *)alias_vs, alias_vs_len,
	                           (const char *)alias_fs, alias_fs_len);

	// get uniform locations
	alias_shadevectorLoc = GL_GetUniformLocation(alias_program, "ShadeVector");
	alias_lightColorLoc = GL_GetUniformLocation(alias_program, "LightColor");

	alias_texLoc = GL_GetUniformLocation(alias_program, "Tex");
	alias_fullbrightTexLoc = GL_GetUniformLocation(alias_program, "FullbrightTex");
	alias_useFullbrightTexLoc = GL_GetUniformLocation(alias_program, "UseFullbrightTex");
	alias_projectionLoc = GL_GetUniformLocation(alias_program, "Projection");
	alias_modelViewLoc = GL_GetUniformLocation(alias_program, "ModelView");

	// get uniform locations
	alias_texCoordsAttrIndex = GL_GetAttribLocation(alias_program, "TexCoords");
	alias_vertexAttrIndex = GL_GetAttribLocation(alias_program, "Vertex");
	alias_normalAttrIndex = GL_GetAttribLocation(alias_program, "Normal");
}
