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

#include "gl_model.h"

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

void R_DrawSpriteModel(entity_t *ent)
{
	vec3_t point;
	float *up, *right;
	vec3_t v_forward, v_right, v_up;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	mspriteframe_t *frame = R_GetSpriteFrame(ent);
	sprite_model_t *spritemodel = ent->model->spritemodel;

	if (spritemodel->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors(ent->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{
		// normal sprite
		up = vup;
		right = vright;
	}

	GL_Bind(frame->gl_texturenum);

	glEnable(GL_ALPHA_TEST);

	GLfloat texts[] = {
		0, 1,
		0, 0,
		1, 0,
		1, 1,
	};

	GLfloat verts[3*4];

	VectorMA(ent->origin, frame->down, up, point);
	VectorMA(point, frame->left, right, point);
	verts[0] = point[0]; verts[1] = point[1]; verts[2] = point[2];

	VectorMA(ent->origin, frame->up, up, point);
	VectorMA(point, frame->left, right, point);
	verts[3] = point[0]; verts[4] = point[1]; verts[5] = point[2];

	VectorMA(ent->origin, frame->up, up, point);
	VectorMA(point, frame->right, right, point);
	verts[6] = point[0]; verts[7] = point[1]; verts[8] = point[2];

	VectorMA(ent->origin, frame->down, up, point);
	VectorMA(point, frame->right, right, point);
	verts[9] = point[0]; verts[10] = point[1]; verts[11] = point[2];

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisable(GL_ALPHA_TEST);
}
