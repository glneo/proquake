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

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64

vec3_t modelorg;

extern int lightmap_bytes;
extern byte lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

extern glpoly_t *lightmap_polys[MAX_LIGHTMAPS];
extern bool lightmap_modified[MAX_LIGHTMAPS];
extern glRect_t lightmap_rectchange[MAX_LIGHTMAPS];

extern cvar_t gl_overbright;

void DrawGLPoly(glpoly_t *p, int tex_offset)
{
	glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][tex_offset]);
	glVertexPointer(3, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

/* Returns the proper texture for a given time and base texture */
texture_t *R_TextureAnimation(int frame, texture_t *base)
{
	int relative, count;

	if (frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (cl.time * 10) % base->anim_total;

	count = 0;
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

	// go back to the world matrix

	glLoadMatrixf(r_world_matrix);

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

		GL_Bind(t->gl_texturenum);

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

static void R_MirrorChain(msurface_t *s)
{
	if (mirror)
		return;
	mirror = true;
	mirror_plane = s->plane;
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
		else if (i == mirrortexturenum && r_mirroralpha.value < 1.0)
		{
			R_MirrorChain(s);
			continue;
		}
		else
		{
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value < 1.0)
				continue;	// draw translucent water later
			for (; s; s = s->texturechain)
				R_RenderBrushPoly(s);
		}

		t->texturechain = NULL;
	}
}

/* Warp the vertex coordinates */
void DrawGLWaterPoly(glpoly_t *p, int tex_offset)
{
	int i;
	float *v;
	vec3_t verts[p->numverts];

	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		verts[i][0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][2] = v[2];
	}

	glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][tex_offset]);
	glVertexPointer(3, GL_FLOAT, 0, &verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

void R_RenderBrushPoly(msurface_t *fa)
{
	texture_t *t;
	byte *base;
	int maps;
	glRect_t *theRect;
	int smax, tmax;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
	{	// warp texture, no lightmaps
		EmitBothSkyLayers(fa);
		return;
	}

	t = R_TextureAnimation(0, fa->texinfo->texture);
	GL_Bind(t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys(fa);
		return;
	}

	if ((fa->flags & SURF_UNDERWATER) && r_waterwarp.value) // JPG - added r_waterwarp
		DrawGLWaterPoly(fa->polys, 3);
	else
		DrawGLPoly(fa->polys, 3);

	if (gl_fullbright.value)
		fa->draw_this_frame = 1;

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// mh - overbrights - need to rebuild the lightmap if this changes
	if (fa->overbright != gl_overbright.value)
	{
		fa->overbright = gl_overbright.value;
		goto dynamic;
	}

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
	|| fa->cached_dlight)			// dynamic previously
	{
		dynamic: if (r_dynamic.value && !r_fullbright.value) // Bengt: added if no fullbrights
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t)
			{
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l)
			{
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0] >> 4) + 1;
			tmax = (fa->extents[1] >> 4) + 1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s - theRect->l) + smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t - theRect->t) + tmax;
			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap(fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
}

void R_DrawBrushModel(entity_t *ent)
{
	int i, k;
	float dot;
	vec3_t mins, maxs;
	msurface_t *psurf;
	mplane_t *pplane;
	model_t *clmodel = ent->model;
	bool rotated;

	current_texture_num = -1;

	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
	{
		rotated = true;
		for (i = 0; i < 3; i++)
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

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

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

	psurf = &clmodel->brushmodel->surfaces[clmodel->brushmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->brushmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		for (k = 0; k < MAX_DLIGHTS; k++)
		{
			if (cl_dlights[k].die < cl.time || !cl_dlights[k].radius)
				continue;

			R_MarkLights(&cl_dlights[k], 1 << k, clmodel->brushmodel->nodes + clmodel->brushmodel->hulls[0].firstclipnode);
		}
	}

	glPushMatrix();
	ent->angles[0] = -ent->angles[0];	// stupid quake bug
	R_RotateForEntity(ent);
	ent->angles[0] = -ent->angles[0];	// stupid quake bug

	// draw texture
	for (i = 0; i < clmodel->brushmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_RenderBrushPoly(psurf);
		}
	}

	R_BlendLightmaps();

	if (gl_fullbright.value)
		DrawFullBrightTextures(ent);

	glPopMatrix();

}

/*
 ===============================================================================

 WORLD MODEL

 ===============================================================================
 */

int r_visframecount; // bumped when going to a new PVS

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
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
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

			if (!mirror || surf->texinfo->texture != cl.worldmodel->brushmodel->textures[mirrortexturenum])
			{
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			}
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

	VectorCopy(r_refdef.vieworg, modelorg);

	current_texture_num = -1;

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	R_RecursiveWorldNode(cl.worldmodel->brushmodel->nodes);

	DrawTextureChains(cl.worldmodel->brushmodel);

	R_BlendLightmaps();

	if (gl_fullbright.value)
		DrawFullBrightTextures(&ent);
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

	if (r_oldviewleaf == r_viewleaf && (!r_novis.value && !nearwaterportal))
		return;

	if (mirror)
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
