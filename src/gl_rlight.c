/*
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

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

int lightmap_bytes;		// 1, 2, or 4

int lightmap_textures;
unsigned blocklights[18 * 18];
int active_lightmaps;

static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte lightmaps[4 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

glpoly_t *lightmap_polys[MAX_LIGHTMAPS];
bool lightmap_modified[MAX_LIGHTMAPS];
glRect_t lightmap_rectchange[MAX_LIGHTMAPS];

int r_dlightframecount;

extern cvar_t gl_overbright;

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
				DrawGLWaterPoly(p, 5);
			else
				DrawGLPoly(p, 5);
		}
	}

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE); // back to normal Z buffering
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
	store: bl = blocklights;
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
				dest[j] = t;
			}

		}
	}
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

void R_AnimateLight(void)
{
	int i, j, k;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int) (cl.time * 10);
	for (j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}

		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}
}

/*
 =============================================================================

 DYNAMIC LIGHTS BLEND RENDERING

 =============================================================================
 */

void AddLightBlend(float r, float g, float b, float a2)
{
	float a;

	v_blend[3] = a = v_blend[3] + a2 * (1 - v_blend[3]);

	a2 = a2 / a;

	v_blend[0] = v_blend[1] * (1 - a2) + r * a2;
	v_blend[1] = v_blend[1] * (1 - a2) + g * a2;
	v_blend[2] = v_blend[2] * (1 - a2) + b * a2;
}

static float bubble_sin[17], bubble_cos[17];

void R_Init_FlashBlend_Bubble(void)
{
	int i;
	float a;

	for (i = 16; i >= 0; i--)
	{
		a = i / 16.0 * M_PI * 2;
		bubble_sin[i] = sin(a);
		bubble_cos[i] = cos(a);
	}
}

void R_RenderDlight(dlight_t *light)
{
	int i, j;
	vec3_t verts[18], v;
	vec4_t colors[18];
	float rad;

	static vec4_t color_start = { 0.2f, 0.1f, 0.0f, 1.0f };
	static vec4_t color_end = { 0.0f, 0.0f, 0.0f, 1.0f };

	rad = light->radius * 0.35;

	VectorSubtract(light->origin, r_origin, v);
	if (VectorLength(v) < rad)
	{	// view is inside the dlight
		AddLightBlend(1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	for (i = 0; i < 3; i++)
		verts[0][i] = light->origin[i] - vpn[i] * rad;
	memcpy(colors[0], color_start, sizeof(color_start));
	for (i = 16; i >= 0; i--)
	{
		for (j = 0; j < 3; j++)
			verts[i + 1][j] = light->origin[j] + vright[j] * bubble_cos[i] * rad + vup[j] * bubble_sin[i] * rad;
		memcpy(colors[i + 1], color_end, sizeof(color_end));
	}

	glColorPointer(4, GL_FLOAT, 0, &colors[0][0]);
	glVertexPointer(3, GL_FLOAT, 0, &verts[0][0]);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 18);
}

void R_RenderDlights(void)
{
	int i;
	dlight_t *l;

	if (!gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
						//  advanced yet for this frame
	glDepthMask(0);
	glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	l = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_RenderDlight(l);
	}

	glColor4ub(color_white[0], color_white[1], color_white[2], color_white[3]);
	glDisable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(1);
}

/*
 =============================================================================

 DYNAMIC LIGHTS

 =============================================================================
 */

void R_MarkLights(dlight_t *light, int bit, mnode_t *node)
{
	mplane_t *splitplane;
	float dist;
	msurface_t *surf;
	int i;

	if (node->contents < 0)
		return;

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;

	if (dist > light->radius)
	{
		R_MarkLights(light, bit, node->children[0]);
		return;
	}
	if (dist < -light->radius)
	{
		R_MarkLights(light, bit, node->children[1]);
		return;
	}

// mark the polygons
	surf = cl.worldmodel->brushmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights(light, bit, node->children[0]);
	R_MarkLights(light, bit, node->children[1]);
}

void R_PushDlights(void)
{
	int i;
	dlight_t *l;

	if (gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
						//  advanced yet for this frame
	l = cl_dlights;

	for (i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		R_MarkLights(l, 1 << i, cl.worldmodel->brushmodel->nodes);
	}
}

/*
 =============================================================================

 LIGHT SAMPLING

 =============================================================================
 */

mplane_t *lightplane;
vec3_t lightspot;

int RecursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end)
{
	int i, r, s, t, ds, dt, side, maps;
	unsigned scale;
	float front, back, frac;
	byte *lightmap;
	vec3_t mid;
	mplane_t *plane;
	msurface_t *surf;
	mtexinfo_t *tex;

	if (node->contents < 0)
		return -1;		// didn't hit anything

// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	if ((back < 0) == side)
		return RecursiveLightPoint(node->children[side], start, end);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

// go down front side	
	r = RecursiveLightPoint(node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something

	if ((back < 0) == side)
		return -1;		// didn't hit anuthing

// check for impact on this node
	VectorCopy(mid, lightspot);
	lightplane = plane;

	surf = cl.worldmodel->brushmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += dt * ((surf->extents[0] >> 4) + 1) + ds;

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
			}

			r >>= 8;
		}

		return r;
	}

// go down back side
	return RecursiveLightPoint(node->children[!side], mid, end);
}

int R_LightPoint(vec3_t p)
{
	vec3_t end;
	int r;

	if (!cl.worldmodel->brushmodel->lightdata)
		return 255;

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint(cl.worldmodel->brushmodel->nodes, p, end);

	if (r == -1)
		r = 0;

	return r;
}

/*
 ===============================================================================

 LIGHTMAP ALLOCATION

 ===============================================================================
 */

/* returns a texture number and the position inside it */
static int AllocBlock(int w, int h, int *x, int *y)
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

//	Sys_Error("full");
	return 0;
}

static void BuildSurfaceDisplayList(brush_model_t *brushmodel, int surface)
{
	int i, lindex, lnumverts;
	float *vec, s, t;
	medge_t *pedges, *r_pedge;
	glpoly_t *poly;

	msurface_t *fa = &brushmodel->surfaces[surface];

	// reconstruct the polygon
	pedges = brushmodel->edges;
	lnumverts = fa->numedges;

	// draw texture
	poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++)
	{
		lindex = brushmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = brushmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = brushmodel->vertexes[r_pedge->v[1]].position;
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

static void GL_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax, tmax;
	byte *base;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmaptexturenum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * lightmap_bytes);
}

/*
 * Builds the lightmap texture
 * with all the surfaces from all brush models
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
		for (i = 0; i < m->brushmodel->numsurfaces; i++)
		{
			if (m->brushmodel->surfaces[i].flags & (SURF_DRAWSKY | SURF_DRAWTURB))
				continue;
			GL_CreateSurfaceLightmap(&m->brushmodel->surfaces[i]);
			BuildSurfaceDisplayList(m->brushmodel, i);
		}
	}

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break; // no more used
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
}