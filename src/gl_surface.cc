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

#define	MAX_LIGHTMAPS   128
extern gltexture_t *lightmap_textures[MAX_LIGHTMAPS];

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

static void GL_DrawPoly(glpoly_t *p)
{
	glTexCoordPointer(2, GL_FLOAT, 0, &p->tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &p->verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

static void GL_DrawPolyLight(glpoly_t *p)
{
	glTexCoordPointer(2, GL_FLOAT, 0, &p->light_tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &p->verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

/* Returns the proper texture for a given time and base texture */
static texture_t *R_TextureAnimation(int frame, texture_t *base)
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

/* Warp the vertex coordinates */
static void GL_DrawWaterPoly(glpoly_t *p)
{
	vec3_t verts[p->numverts];

	for (int i = 0; i < p->numverts; i++)
	{
		float *v = p->verts[i];
		verts[i][0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][2] = v[2];
	}

	glTexCoordPointer(2, GL_FLOAT, 0, &p->tex[0]);
	glVertexPointer(3, GL_FLOAT, 0, &verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

/* Warp the vertex coordinates */
static void GL_DrawWaterPolyLight(glpoly_t *p)
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

static void GL_RenderBrushPoly(msurface_t *fa, int frame)
{
	c_brush_polys++;

	texture_t *t = R_TextureAnimation(frame, fa->texinfo->texture);
	GL_Bind(t->gltexture);

	if ((fa->flags & SURF_UNDERWATER) && r_waterwarp.value)
		GL_DrawWaterPoly(fa->polys);
	else
		GL_DrawPoly(fa->polys);

	R_RenderDynamicLightmaps(fa);

	GL_Bind(lightmap_textures[fa->lightmaptexturenum]);
	R_UploadLightmap(fa->lightmaptexturenum);

	glDepthMask(GL_FALSE); // don't bother writing Z
	if (!r_lightmap.value)
	{
		if (gl_overbright.value)
			glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		else
			glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	}

	if ((fa->polys->flags & SURF_UNDERWATER) && r_waterwarp.value)
		GL_DrawWaterPolyLight(fa->polys);
	else
		GL_DrawPolyLight(fa->polys);

	if (t->fullbright != NULL && r_fullbright.value)
	{
		glBlendFunc(GL_ONE, GL_ONE);
		GL_Bind(t->fullbright);
		GL_DrawPoly(fa->polys);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE); // back to normal Z buffering
}

static void R_DrawWaterSurfaces(brush_model_t *brushmodel)
{
	int i;
	msurface_t *s;
	texture_t *t;

	if (r_wateralpha.value < 1.0)
	{
		glColor4f(1.0f, 1.0f, 1.0f, r_wateralpha.value);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	for (i = 0; i < brushmodel->numtextures; i++)
	{
		t = brushmodel->textures[i];
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
		glDepthMask(GL_TRUE);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
}

void R_DrawSurfaces(brush_model_t *brushmodel)
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
				GL_RenderBrushPoly(s, 0);
		}

		t->texturechain = NULL;
	}

	R_DrawWaterSurfaces(brushmodel);
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

	GLfloat old_matrix[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, old_matrix);

	GLfloat matrix[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
	Q_Matrix modelViewMatrix;
	modelViewMatrix.set(matrix);

	GL_RotateForEntity(ent, modelViewMatrix);

	glLoadMatrixf(modelViewMatrix.get());

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
			GL_RenderBrushPoly(psurf, ent->frame);
		}
	}

	glLoadMatrixf(old_matrix);
}
