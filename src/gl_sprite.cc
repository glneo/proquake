/*
 * Sprite model rendering
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

#include "spritegen.h"

// shader program
extern GLuint brush_program;

// uniforms used in vertex shader
extern GLuint brush_ProjectionUniform;
extern GLuint brush_ModelViewUniform;
extern GLuint brush_TimeUniform;
extern GLuint brush_SpeedScaleUniform;

// uniforms used in fragment shader
extern GLuint brush_TexUniform;
extern GLuint brush_UseLMTexUniform;
extern GLuint brush_UseOverbrightUniform;
extern GLuint brush_AlphaUniform;

// attribs
extern GLuint brush_VertexAttrib;
extern GLuint brush_TexCoordsAttrib;

static mspriteframe_t *R_GetSpriteFrame(entity_t *ent)
{
	mspriteframe_t *pspriteframe;

	sprite_model_t *spritemodel = ent->model->spritemodel;
	int frame = ent->frame;

	if ((frame >= spritemodel->numframes) || (frame < 0))
	{
		Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (spritemodel->framedescs[frame].numposes == 1)
	{
		pspriteframe = &spritemodel->framedescs[frame].poses[0];
	}
	else
	{
		mspriteframedesc_t *spriteframedesc = (mspriteframedesc_t *)&spritemodel->framedescs[frame];
		int numposes = spriteframedesc->numposes;
		float fullinterval = spriteframedesc->poses[numposes - 1].interval;

		float time = cl.time + ent->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		float targettime = time - ((int) (time / fullinterval)) * fullinterval;
		int i;
		for (i = 0; i < (numposes - 1); i++)
		{
			if (spriteframedesc->poses[i].interval > targettime)
				break;
		}

		pspriteframe = &spriteframedesc->poses[i];
	}

	return pspriteframe;
}

void GL_DrawSpriteModel(entity_t *ent)
{
	vec3_t point;
	vec3_t v_forward, v_right, v_up;
	float angle, sr, cr;

	mspriteframe_t *frame = R_GetSpriteFrame(ent);
	sprite_model_t *spritemodel = ent->model->spritemodel;

	switch(spritemodel->type)
	{
	case SPR_VP_PARALLEL_UPRIGHT: //faces view plane, up is towards the heavens
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		VectorCopy(vright, v_right);
		break;
	case SPR_FACING_UPRIGHT: //faces camera origin, up is towards the heavens
		VectorSubtract(ent->origin, r_refdef.vieworg, v_forward);
		v_forward[2] = 0;
		VectorNormalize(v_forward);
		v_right[0] = v_forward[1];
		v_right[1] = -v_forward[0];
		v_right[2] = 0;
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		break;
	case SPR_VP_PARALLEL: //faces view plane, up is towards the top of the screen
		v_up[0] = vup[0];
		v_up[1] = vup[1];
		v_up[2] = vup[2];
		VectorCopy(vright, v_right);
		break;
	case SPR_ORIENTED: //pitch yaw roll are independent of camera
		AngleVectors (ent->angles, v_forward, v_right, v_up);
		break;
	case SPR_VP_PARALLEL_ORIENTED: //faces view plane, but obeys roll value
		angle = ent->angles[ROLL] * M_PI_DIV_180;
		sr = sinf(angle);
		cr = cosf(angle);
		v_right[0] = vright[0] * cr + vup[0] * sr;
		v_right[1] = vright[1] * cr + vup[1] * sr;
		v_right[2] = vright[2] * cr + vup[2] * sr;
		v_up[0] = vright[0] * -sr + vup[0] * cr;
		v_up[1] = vright[1] * -sr + vup[1] * cr;
		v_up[2] = vright[2] * -sr + vup[2] * cr;
		break;
	default:
		return;
	}

	GLfloat texts[] = {
		0, 1,
		0, 0,
		1, 0,
		1, 1,
	};

	GLfloat verts[3*4];

	VectorMA(ent->origin, frame->down, v_up, point);
	VectorMA(point, frame->left, v_right, point);
	verts[0] = point[0]; verts[1] = point[1]; verts[2] = point[2];

	VectorMA(ent->origin, frame->up, v_up, point);
	VectorMA(point, frame->left, v_right, point);
	verts[3] = point[0]; verts[4] = point[1]; verts[5] = point[2];

	VectorMA(ent->origin, frame->up, v_up, point);
	VectorMA(point, frame->right, v_right, point);
	verts[6] = point[0]; verts[7] = point[1]; verts[8] = point[2];

	VectorMA(ent->origin, frame->down, v_up, point);
	VectorMA(point, frame->right, v_right, point);
	verts[9] = point[0]; verts[10] = point[1]; verts[11] = point[2];

	// setup
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(brush_program);
	GL_BindToUnit(GL_TEXTURE0, frame->gltexture);

	// set vertex uniforms
	glUniformMatrix4fv(brush_ProjectionUniform, 1, GL_FALSE, projectionMatrix.get());
	glUniformMatrix4fv(brush_ModelViewUniform, 1, GL_FALSE, modelViewMatrix.get());
	glUniform1f(brush_TimeUniform, 0.0f);
	glUniform1f(brush_SpeedScaleUniform, 0.0f);

	// set fragment uniforms
	glUniform1i(brush_TexUniform, 0);
	glUniform1i(brush_UseLMTexUniform, GL_FALSE);
	glUniform1i(brush_UseOverbrightUniform, 1.0f);
	glUniform1f(brush_AlphaUniform, 1.0f);

	// set attributes
	GLuint tempVBOs[2];
	glGenBuffers(2, tempVBOs);
	glBindBuffer(GL_ARRAY_BUFFER, tempVBOs[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * 4, &verts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(brush_VertexAttrib);
	glVertexAttribPointer(brush_VertexAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, tempVBOs[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &texts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(brush_TexCoordsAttrib);
	glVertexAttribPointer(brush_TexCoordsAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// cleanup
	glDisableVertexAttribArray(brush_TexCoordsAttrib);
	glDisableVertexAttribArray(brush_VertexAttrib);
	glDeleteBuffers(2, tempVBOs);
	glDisable(GL_BLEND);
}
