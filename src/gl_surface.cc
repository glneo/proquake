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

#define BACKFACE_EPSILON 0.01

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

static void DrawTextureChains(brush_model_t *brushmodel)
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
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value < 1.0)
				continue;	// draw translucent water later
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
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_BLEND);
		glBlendFunc (GL_ONE, GL_ONE);
		glDepthMask(GL_FALSE);
		GL_Bind(t->fullbright);
		DrawGLPoly(fa->polys);
		glDepthMask(GL_TRUE);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
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

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->brushmodel->firstmodelsurface != 0 && !gl_flashblend.value)
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
	R_RotateForEntity(ent);
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

/*
 ===============================================================================

 WORLD MODEL

 ===============================================================================
 */

int r_visframecount; // bumped when going to a new PVS

//FIXME: move to header
void R_StoreEfrags(efrag_t **ppefrag);

static void R_RecursiveWorldNode(mnode_t *node)
{
	int c, side;
	mplane_t *plane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	double dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *) node;
		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags(&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the appropriate sides

	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = r_refdef.vieworg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = r_refdef.vieworg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = r_refdef.vieworg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (r_refdef.vieworg, plane->normal) - plane->dist;
		break;
	}

	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side]);

	// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		if (dot < 0 - BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for (surf = cl.worldmodel->brushmodel->surfaces + node->firstsurface; c; c--, surf++)
		{
			if (surf->visframe != r_framecount)
				continue;

			// don't backface underwater surfaces, because they warp // JPG - added r_waterwarp
			if ((!(surf->flags & SURF_UNDERWATER) || !r_waterwarp.value) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
				continue;		// wrong side

			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side]);
}

void R_DrawWorld(void)
{
	entity_t ent;

	memset(&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	R_RecursiveWorldNode(cl.worldmodel->brushmodel->nodes);

	DrawTextureChains(cl.worldmodel->brushmodel);

	R_BlendLightmaps();
}

void R_MarkLeaves(void)
{
	int i;
	byte *vis, solid[4096];
	mnode_t *node;
	extern cvar_t gl_nearwater_fix;
	msurface_t **mark;
	bool nearwaterportal = false;

	// Check if near water to avoid HOMs when crossing the surface
	if (gl_nearwater_fix.value)
		for (i = 0, mark = r_viewleaf->firstmarksurface; i < r_viewleaf->nummarksurfaces; i++, mark++)
		{
			if ((*mark)->flags & SURF_DRAWTURB)
			{
				nearwaterportal = true;
				//	Con_SafePrintf ("R_MarkLeaves: nearwaterportal, surfs=%d\n", r_viewleaf->nummarksurfaces);
				break;
			}
		}

	if ((r_oldviewleaf == r_viewleaf) && !r_novis.value && !nearwaterportal)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset(solid, 0xff, (cl.worldmodel->brushmodel->numleafs + 7) >> 3);
	}
	else if (nearwaterportal)
	{
		extern byte *SV_FatPVS(vec3_t org, model_t *worldmodel);
		vis = SV_FatPVS(r_origin, cl.worldmodel);
	}
	else
	{
		vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel->brushmodel);
	}

	for (i = 0; i < cl.worldmodel->brushmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t *) &cl.worldmodel->brushmodel->leafs[i + 1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}
