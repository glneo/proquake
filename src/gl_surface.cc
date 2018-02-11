/*
 * Surface-related refresh code
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

#define	BLOCK_WIDTH     128
#define	BLOCK_HEIGHT    128

#define	MAX_LIGHTMAPS   64

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

extern glpoly_t *lightmap_polys[MAX_LIGHTMAPS];

void DrawGLPoly(glpoly_t *p)
{
	glTexCoordPointer(2, GL_FLOAT, 0, &p->tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &p->verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

void DrawGLPolyLight(glpoly_t *p)
{
	glTexCoordPointer(2, GL_FLOAT, 0, &p->light_tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &p->verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

/* Returns the proper texture for a given time and base texture */
texture_t *R_TextureAnimation(int frame, texture_t *base)
{
	if (frame)
		if (base->alternate_anims)
			base = base->alternate_anims;

	if (!base->anim_total)
		return base;

	int relative = (int)(cl.time * 10) % base->anim_total;

	int count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error("broken cycle");
		if (++count > 100)
			Sys_Error("infinite cycle");
	}

	return base;
}

void R_DrawWaterSurfaces(void)
{
	int i;
	msurface_t *s;
	texture_t *t;

	if (r_wateralpha.value < 1.0)
	{
		glEnable(GL_BLEND);
		glColor4f(1.0f, 1.0f, 1.0f, r_wateralpha.value);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	for (i = 0; i < cl.worldmodel->brushmodel->numtextures; i++)
	{
		t = cl.worldmodel->brushmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (!(s->flags & SURF_DRAWTURB))
			continue;

		GL_Bind(t->gltexture);

		for (; s; s = s->texturechain)
			EmitWaterPolys(s);

		t->texturechain = NULL;
	}

	if (r_wateralpha.value < 1.0)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glDisable(GL_BLEND);
	}
}

void DrawTextureChains(brush_model_t *brushmodel)
{
	int i;
	msurface_t *s;
	texture_t *t;

	for (i = 0; i < brushmodel->numtextures; i++)
	{
		t = brushmodel->textures[i];
		if (!t)
			continue;

		s = t->texturechain;
		if (!s)
			continue;

		if (i == skytexturenum)
			R_DrawSkyChain(s);
		else
		{
			if (s->flags & SURF_DRAWTURB)
				continue; // draw water later
			for (; s; s = s->texturechain)
				R_RenderBrushPoly(s, 0);
		}

		t->texturechain = NULL;
	}

	R_DrawWaterSurfaces();
}

/* Warp the vertex coordinates */
void DrawGLWaterPoly(glpoly_t *p)
{
	vec3_t verts[p->numverts];

	for (int i = 0; i < p->numverts; i++)
	{
		float *v = p->verts[i];
		verts[i][0] = p->verts[0][0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][2] = v[2];
	}

	glTexCoordPointer(2, GL_FLOAT, 0, &p->tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

/* Warp the vertex coordinates */
void DrawGLWaterPolyLight(glpoly_t *p)
{
	vec3_t verts[p->numverts];

	for (int i = 0; i < p->numverts; i++)
	{
		float *v = p->verts[i];
		verts[i][0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][2] = v[2];
	}

	glTexCoordPointer(2, GL_FLOAT, 0, &p->light_tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

void R_RenderBrushPoly(msurface_t *fa, int frame)
{
	c_brush_polys++;

	texture_t *t = R_TextureAnimation(frame, fa->texinfo->texture);
	GL_Bind(t->gltexture);

	if ((fa->flags & SURF_UNDERWATER) && r_waterwarp.value)
		DrawGLWaterPoly(fa->polys);
	else
		DrawGLPoly(fa->polys);

	R_RenderDynamicLightmaps(fa);

	if (t->fullbright != NULL && gl_fullbright.value)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glDepthMask(GL_FALSE);
		GL_Bind(t->fullbright);
		DrawGLPoly(fa->polys);
		glDepthMask(GL_TRUE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}
}

void R_DrawBrushModel(entity_t *ent)
{
	vec3_t mins, maxs;
	model_t *clmodel = ent->model;
	bool rotated;

	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
	{
		rotated = true;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = ent->origin[i] - clmodel->radius;
			maxs[i] = ent->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(ent->origin, clmodel->mins, mins);
		VectorAdd(ent->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	vec3_t modelorg;
	VectorSubtract(r_refdef.vieworg, ent->origin, modelorg);
	if (rotated)
	{
		vec3_t temp, forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(ent->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	R_ClearLightmapPolys();

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->brushmodel->firstmodelsurface != 0)
	{
		for (int k = 0; k < MAX_DLIGHTS; k++)
		{
			if (cl_dlights[k].die < cl.time || !cl_dlights[k].radius)
				continue;

			R_MarkLights(&cl_dlights[k], k, &clmodel->brushmodel->nodes[clmodel->brushmodel->hulls[0].firstclipnode]);
		}
	}

	glPushMatrix();

	ent->angles[0] = -ent->angles[0];	// stupid quake bug
	GL_RotateForEntity(ent);
	ent->angles[0] = -ent->angles[0];	// stupid quake bug

	// draw texture
	msurface_t *psurf = &clmodel->brushmodel->surfaces[clmodel->brushmodel->firstmodelsurface];
	for (int i = 0; i < clmodel->brushmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		mplane_t *pplane = psurf->plane;
		float dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
		    (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_RenderBrushPoly(psurf, ent->frame);
		}
	}

	R_BlendLightmaps();

	glPopMatrix();
}
