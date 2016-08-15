/*
 * Sprite model loading and rendering
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
	sprite_model_t *spritemodel = currententity->model->spritemodel;

	if (spritemodel->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors(currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{
		// normal sprite
		up = vup;
		right = vright;
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	GL_DisableMultitexture();

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

static void *Mod_LoadSpriteFrame(mspriteframe_t *pspriteframe, dspriteframe_t *pinframe, int framenum, char *mod_name)
{
	int origin[2];
	char name[64];
	byte *frame = (byte *)(pinframe + 1);

	int width = LittleLong(pinframe->width);
	int height = LittleLong(pinframe->height);
	int size = width * height;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = origin[0] + width;

	snprintf(name, sizeof(name), "%s_%i", mod_name, framenum);
	pspriteframe->gl_texturenum = GL_LoadTexture(name, width, height, frame, TEX_MIPMAP | TEX_ALPHA);

	return (void *)(frame + size);
}

static void *Mod_LoadAllSpriteFrames(sprite_model_t *spritemodel, dspriteframetype_t *pframetype, char *mod_name)
{
	int posenum = 0;
	spritemodel->framedescs = Q_malloc(sizeof(*spritemodel->framedescs) * spritemodel->numframes);

	for (int i = 0; i < spritemodel->numframes; i++)
	{
		if (LittleLong(pframetype->type) == SPR_SINGLE)
		{
			spritemodel->framedescs[i].numposes = 1;
			spritemodel->framedescs[i].poses = Q_malloc(sizeof(*spritemodel->framedescs[i].poses));
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteFrame(&spritemodel->framedescs[i].poses[0],
			                                                       (dspriteframe_t *)(pframetype + 1),
									       posenum++, mod_name);
		}
		else
		{
			dspritegroup_t *spritegroup = (dspritegroup_t *)(pframetype + 1);
			int numposes = LittleLong(spritegroup->numframes);
			spritemodel->framedescs[i].numposes = numposes;
			spritemodel->framedescs[i].poses = Q_malloc(sizeof(*spritemodel->framedescs[i].poses) * numposes);
			dspriteinterval_t *pin_intervals = (dspriteinterval_t *)(spritegroup + 1);
			for (int j = 0; j < numposes; j++)
			{
				float interval = LittleFloat(pin_intervals->interval);
				if (interval <= 0.0)
					Sys_Error("interval %f <= 0 in %s", interval, mod_name);
				spritemodel->framedescs[i].poses[j].interval = interval;
				pin_intervals++;
			}
			for (int j = 0; j < numposes; j++)
				pframetype = (dspriteframetype_t *)Mod_LoadSpriteFrame(&spritemodel->framedescs[i].poses[j],
				                                                       (dspriteframe_t *)(pin_intervals),
										       posenum++, mod_name);
		}
	}

	return (void *)pframetype;
}

void Mod_LoadSpriteModel(model_t *mod, void *buffer)
{
	dsprite_t *pinmodel = (dsprite_t *) buffer;
	char *mod_name = mod->name;

	int version = LittleLong(pinmodel->version);
	if (version != SPRITE_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	sprite_model_t *spritemodel = Q_malloc(sizeof(*spritemodel));

	// endian-adjust and copy the data, starting with the sprite header
	spritemodel->type = LittleLong(pinmodel->type);
	spritemodel->maxwidth = LittleLong(pinmodel->width);
	spritemodel->maxheight = LittleLong(pinmodel->height);
	spritemodel->numframes = LittleLong(pinmodel->numframes);
	spritemodel->beamlength = LittleFloat(pinmodel->beamlength);

	// validate the setup
	if (spritemodel->numframes <= 0) Sys_Error("model %s has no frames", mod_name);

	// load the frames
	dspriteframetype_t *pframetype = (dspriteframetype_t *)(pinmodel + 1);
	pframetype = Mod_LoadAllSpriteFrames(spritemodel, pframetype, mod_name);

	mod->numframes = spritemodel->numframes;

	mod->type = mod_sprite;
	mod->synctype = LittleLong(pinmodel->synctype);

	mod->mins[0] = mod->mins[1] = -spritemodel->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = spritemodel->maxwidth / 2;
	mod->mins[2] = -spritemodel->maxheight / 2;
	mod->maxs[2] = spritemodel->maxheight / 2;

	mod->spritemodel = spritemodel;
}
