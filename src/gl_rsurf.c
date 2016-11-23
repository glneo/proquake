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

#include "gl_model.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64

int lightmap_bytes;		// 1, 2, or 4

int lightmap_textures;
unsigned blocklights[18 * 18];
int active_lightmaps;

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

static glpoly_t *lightmap_polys[MAX_LIGHTMAPS];
static bool lightmap_modified[MAX_LIGHTMAPS];
static glRect_t lightmap_rectchange[MAX_LIGHTMAPS];

static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

int poly_count;

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

void R_RenderDynamicLightmaps(msurface_t *fa);

void DrawGLPoly(glpoly_t *p, int tex_offset)
{
	poly_count++;

	glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][tex_offset]);
	glVertexPointer(3, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

void R_AddDynamicLights(msurface_t *surf)
{
	int lnum, i, smax, tmax, s, t, sd, td;
	float dist, rad, minlight;
	vec3_t impact, local;
	mtexinfo_t *tex;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum)))
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i = 0; i < 3; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0; t < tmax; t++)
		{
			td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (s = 0; s < smax; s++)
			{
				sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight)
					blocklights[t * smax + s] += (rad - dist) * 256;
			}
		}
	}
}

/*
 ===============
 R_BuildLightMap

 Combine and scale multiple lightmaps into the 8.8 format in blocklights
 ===============
 */
extern cvar_t gl_overbright;
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride)
{
	int smax, tmax, t, i, j, size, maps;
	byte *lightmap;
	unsigned scale, *bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->brushmodel->lightdata)
	{
		for (i = 0; i < size; i++)
			blocklights[i] = 255 * 256;
		goto store;
	}

// clear to no light
	for (i = 0; i < size; i++)
		blocklights[i] = 0;

// add all the lightmaps
	if (lightmap)
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			for (i = 0; i < size; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights(surf);

// bound, invert, and shift
store:
	bl = blocklights;
	for (i = 0; i < tmax; i++, dest += stride)
	{
		for (j = 0; j < smax; j++)
		{
			if (gl_overbright.value)
			{
				t = *bl++;
				t >>= 8;
				if (t > 255)
					t = 255;
				dest[j] = t;
			}
			else
			{
				t = *bl++;
				t >>= 7;
				if (t > 255)
					t = 255;
				// Baker: if hardware gamma shouldn't this go?
				dest[j] = 255 - gammatable[t];	// JPG 3.02 - t -> gammatable[t]
			}

		}
	}
}

/*
 ===============
 R_TextureAnimation

 Returns the proper texture for a given time and base texture
 ===============
 */
texture_t *R_TextureAnimation(texture_t *base)
{
	int relative, count;

	if (currententity->frame)
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
			Sys_Error("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
 ===============================================================================

 BRUSH MODELS

 ===============================================================================
 */

/*
 ===============
 R_UploadLightmap -- uploads the modified lightmap to opengl if necessary

 assumes lightmap texture is already bound
 ===============
 */
void R_UploadLightmap(int lmap)
{
	glRect_t *theRect;

	if (!lightmap_modified[lmap])
		return;

	lightmap_modified[lmap] = false;

	theRect = &lightmap_rectchange[lmap];
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_LUMINANCE, GL_UNSIGNED_BYTE,
			lightmaps + (lmap * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH * lightmap_bytes);
	theRect->l = BLOCK_WIDTH;
	theRect->t = BLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;
}

void R_BlendLightmaps(void)
{
	if (r_fullbright.value)
		return;

	glDepthMask(GL_FALSE); // don't bother writing Z

	if (gl_overbright.value)
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	else
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

	if (!r_lightmap.value)
		glEnable(GL_BLEND);

	for (int i = 0; i < MAX_LIGHTMAPS; i++)
	{
		glpoly_t *p = lightmap_polys[i];
		if (!p)
			continue;

		GL_Bind(lightmap_textures + i);

		// BengtQuake uploads lightmap here

		R_UploadLightmap(i); // BengtQuake way

		for (; p; p = p->chain)
		{
			// JPG - added r_waterwarp
			if ((p->flags & SURF_UNDERWATER) && r_waterwarp.value)
				DrawGLWaterPolyLightmap(p);
			else
				DrawGLPoly(p, 5);
		}
	}

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE); // back to normal Z buffering
}

void R_RenderDynamicLightmaps(msurface_t *fa)
{
	int maps, smax, tmax;
	byte *base;
	glRect_t *theRect;

	c_brush_polys++;

	if (fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB))
		return;

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

	if (fa->dlightframe == r_framecount || fa->cached_dlight)
	{	// dynamic this frame dynamic previously

		dynamic: if (r_dynamic.value)
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
			if (theRect->w + theRect->l < fa->light_s + smax)
				theRect->w = fa->light_s - theRect->l + smax;
			if (theRect->h + theRect->t < fa->light_t + tmax)
				theRect->h = fa->light_t - theRect->t + tmax;
			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap(fa, base, BLOCK_WIDTH * lightmap_bytes);
		}
	}
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

void DrawTextureChains(void)
{
	int i;
	msurface_t *s;
	texture_t *t;

	for (i = 0; i < cl.worldmodel->brushmodel->numtextures; i++)
	{
		t = cl.worldmodel->brushmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (i == skytexturenum)
			R_DrawSkyChain(s);
		else if (i == mirrortexturenum && r_mirroralpha.value < 1.0) // Baker 3.99: changed, max value is 1
		{
			R_MirrorChain(s);
			continue;
		}
		else
		{
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value < 1.0) // Baker 3.99: changed in event r_wateralpha is above max value of 1
				continue;	// draw translucent water later
			for (; s; s = s->texturechain)
				R_RenderBrushPoly(s);
		}

		t->texturechain = NULL;
	}
}

/*
 ================
 DrawGLWaterPoly

 Warp the vertex coordinates
 ================
 */
void DrawGLWaterPoly(glpoly_t *p)
{
	int i;
	float *v;
	vec3_t verts[p->numverts];

	GL_DisableMultitexture();

	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		verts[i][0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][2] = v[2];
	}

	glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][3]);
	glVertexPointer(3, GL_FLOAT, 0, &verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
}

void DrawGLWaterPolyLightmap(glpoly_t *p)
{
	int i;
	float *v;
	vec3_t verts[p->numverts];

	GL_DisableMultitexture();

	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
	{
		verts[i][0] = v[0] + 8 * sin(v[1] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][1] = v[1] + 8 * sin(v[0] * 0.05 + realtime) * sin(v[2] * 0.05 + realtime);
		verts[i][2] = v[2];
	}

	glTexCoordPointer(2, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][5]);
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

	t = R_TextureAnimation(fa->texinfo->texture);
	GL_Bind(t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys(fa);
		return;
	}

	if ((fa->flags & SURF_UNDERWATER) && r_waterwarp.value) // JPG - added r_waterwarp
		DrawGLWaterPoly(fa->polys);
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

void R_MirrorChain(msurface_t *s)
{
	if (mirror)
		return;
	mirror = true;
	mirror_plane = s->plane;
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
		DrawFullBrightTextures(clmodel->brushmodel->surfaces, clmodel->brushmodel->numsurfaces);

	glPopMatrix();

}

/*
 ===============================================================================

 WORLD MODEL

 ===============================================================================
 */

void R_RecursiveWorldNode(mnode_t *node)
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

	currententity = &ent;
	current_texture_num = -1;

	memset(lightmap_polys, 0, sizeof(lightmap_polys));

	R_RecursiveWorldNode(cl.worldmodel->brushmodel->nodes);

	DrawTextureChains();

	R_BlendLightmaps();

	if (gl_fullbright.value)
		DrawFullBrightTextures(cl.worldmodel->brushmodel->surfaces, cl.worldmodel->brushmodel->numsurfaces);
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

/*
 ===============================================================================

 LIGHTMAP ALLOCATION

 ===============================================================================
 */

// returns a texture number and the position inside it
int AllocBlock(int w, int h, int *x, int *y)
{
	int i, j, best, best2, texnum;

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (allocated[texnum][i + j] >= best)
					break;
				if (allocated[texnum][i + j] > best2)
					best2 = allocated[texnum][i + j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

//	Sys_Error("AllocBlock: full");
	return 0;
}

mvertex_t *r_pcurrentvertbase;
model_t *currentmodel;

int nColinElim;

void BuildSurfaceDisplayList(msurface_t *fa)
{
	int i, lindex, lnumverts;
	float *vec, s, t;
	medge_t *pedges, *r_pedge;
	glpoly_t *poly;

	// reconstruct the polygon
	pedges = currentmodel->brushmodel->edges;
	lnumverts = fa->numedges;

	// draw texture
	poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++)
	{
		lindex = currentmodel->brushmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}
	poly->numverts = lnumverts;
}

/*
 ========================
 GL_CreateSurfaceLightmap
 ========================
 */
void GL_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax, tmax;
	byte *base;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * lightmap_bytes);
}

/*
 ==================
 GL_BuildLightmaps

 Builds the lightmap texture
 with all the surfaces from all brush models
 ==================
 */
void GL_BuildLightmaps(void)
{
	int i, j;
	model_t *m;

	memset(allocated, 0, sizeof(allocated));

	r_framecount = 1; // no dlightcache

	if (!lightmap_textures)
	{
		lightmap_textures = texture_extension_number;
		texture_extension_number += MAX_LIGHTMAPS;
	}

	lightmap_bytes = 1;

	for (j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j]))
			break;
		if (m->name[0] == '*')
			continue;
		if (m->type != mod_brush)
			continue;
		r_pcurrentvertbase = m->brushmodel->vertexes;
		currentmodel = m;
		for (i = 0; i < m->brushmodel->numsurfaces; i++)
		{
			GL_CreateSurfaceLightmap(m->brushmodel->surfaces + i);
			if (m->brushmodel->surfaces[i].flags & SURF_DRAWTURB)
				continue;

			if (m->brushmodel->surfaces[i].flags & SURF_DRAWSKY)
				continue;

			BuildSurfaceDisplayList(m->brushmodel->surfaces + i);
		}
	}

	// TODO: check this
	GL_SelectTexture(GL_TEXTURE1);

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures + i);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
				lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes);
	}

	GL_SelectTexture(GL_TEXTURE0);
}
