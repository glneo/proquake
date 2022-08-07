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
#include "glquake.h"

#define LMBLOCK_WIDTH	256 // FIXME: make dynamic
#define LMBLOCK_HEIGHT	256

typedef struct glRect_s
{
	unsigned short l, t, w, h;
} glRect_t;

typedef struct lightmap_s
{
	gltexture_t *texture;
	std::vector<unsigned short> indices;

	bool modified;
	glRect_t rectchange;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte *data;	//[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
} lightmap_t;

#define MAX_SANITY_LIGHTMAPS (1u<<20)
size_t lightmap_count;
lightmap_t *lightmaps;

size_t last_lightmap_allocated;
static int allocated[LMBLOCK_WIDTH];





int d_lightstylevalue[256]; // 8.8 fraction of base light value

int r_dlightframecount;

byte color_white[4] = { 255, 255, 255, 255 };
byte color_black[4] = { 0, 0, 0, 255 };

void R_ClearLightmapPolys(void)
{
	for (size_t i = 0; i < lightmap_count; i++)
		lightmaps[i].indices.clear();
}

void R_ClearLightStyle(void)
{
	for (size_t i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264; // normal light value
}

/* uploads the modified lightmap to opengl if necessary */
static void R_UploadLightmap(lightmap_t *lightmap)
{
	glRect_t *theRect = &lightmap->rectchange;
	byte *data = lightmap->data + (theRect->t * LMBLOCK_WIDTH);

#ifdef OPENGLES
	GLenum format = GL_LUMINANCE;
#else
	GLenum format = GL_RED;
#endif

	GL_BindToUnit(GL_TEXTURE0, lightmap->texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0,
			0,             theRect->t,
			LMBLOCK_WIDTH, theRect->h,
			format, GL_UNSIGNED_BYTE,
			data);

	theRect->l = LMBLOCK_WIDTH;
	theRect->t = LMBLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;

	lightmap->modified = false;
}

void R_UploadLightmaps(void)
{
	for (size_t i = 0; i < lightmap_count; i++)
	{
		lightmap_t *lightmap = &lightmaps[i];

		if (!lightmap->modified)
			continue;

		R_UploadLightmap(lightmap);
	}
}

static void R_AddDynamicLights(msurface_t *surf, unsigned int *dest)
{
	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	int size = smax * tmax;

	if (size > LMBLOCK_WIDTH * LMBLOCK_HEIGHT)
		Sys_Error("Out of bounds light extent");

	mtexinfo_t *tex = surf->texinfo;

	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & BIT(lnum)))
			continue;		// not lit by this light

		float rad = cl_dlights[lnum].radius;
		float dist = DotProduct(cl_dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		float minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		vec3_t impact, local;
		for (int i = 0; i < 3; i++)
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (int t = 0; t < tmax; t++)
		{
			int td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (int s = 0; s < smax; s++)
			{
				int sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight)
					dest[t * smax + s] += (rad - dist) * 256;
			}
		}
	}
}

/* Combine and scale multiple lightmaps into the 8.8 format in blocklights */
static void R_BuildLightMap(msurface_t *surf, byte *dest)
{
	surf->cached_dlight = (surf->dlightframe == r_framecount);

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	int size = smax * tmax;

	if (size > LMBLOCK_WIDTH * LMBLOCK_HEIGHT)
		Sys_Error("Out of bounds light extent");

	byte *lightmap = surf->samples;

	unsigned int blocklights[LMBLOCK_WIDTH * LMBLOCK_HEIGHT];

	// set to full bright if no light data
	if (!lightmap)
	{
		for (int i = 0; i < size; i++)
			blocklights[i] = 255 * 256;
		goto store;
	}

	// clear to no light
	for (int i = 0; i < size; i++)
		blocklights[i] = 0;

	// add all the lightmaps
	if (lightmap)
		for (int lmap = 0; lmap < MAXLIGHTMAPS && surf->styles[lmap] != 255; lmap++)
		{
			unsigned int scale = d_lightstylevalue[surf->styles[lmap]];
			surf->cached_light[lmap] = scale; // 8.8 fraction
			for (int i = 0; i < size; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size; // skip to next lightmap
		}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights(surf, blocklights);

	// bound, invert, and shift
store:
	for (int i = 0; i < tmax; i++)
	{
		for (int j = 0; j < smax; j++)
		{
			int t = blocklights[j + (i * smax)];
			t >>= 8;
			if (t > 255)
				t = 255;
			dest[j + (i * LMBLOCK_WIDTH)] = t;
		}
	}
}

void R_RenderDynamicLightmaps(msurface_t *fa)
{
	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	lightmap_t *lightmap = &lightmaps[fa->lightmapnum];

	lightmap->indices.insert(std::end(lightmap->indices), std::begin(fa->indices), std::end(fa->indices));

	// check for lightmap modification
	for (int maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	// dynamic this frame or dynamic previously
	if (fa->dlightframe == r_framecount || fa->cached_dlight)
		goto dynamic;

	return;

dynamic:
	if (!r_dynamic.value)
		return;

	lightmap->modified = true;

	glRect_t *theRect = &lightmap->rectchange;
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
	int smax = (fa->extents[0] >> 4) + 1;
	int tmax = (fa->extents[1] >> 4) + 1;
	if (theRect->w + theRect->l < fa->light_s + smax)
		theRect->w = fa->light_s - theRect->l + smax;
	if (theRect->h + theRect->t < fa->light_t + tmax)
		theRect->h = fa->light_t - theRect->t + tmax;

	byte *base = lightmap->data;
	base += fa->light_t * LMBLOCK_WIDTH + fa->light_s;
	R_BuildLightMap(fa, base);
}

void R_AnimateLight(void)
{
	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	int i = (int)(cl.time * 10);
	for (int j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}

		int k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}
}

/*
 =============================================================================

 DYNAMIC LIGHTS

 =============================================================================
 */

static void R_MarkLights(dlight_t *light, int bit, mnode_t *node, msurface_t *surfaces)
{
	if (node->contents < 0)
		return;

	mplane_t *splitplane = node->plane;
	float dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;
	if (dist > light->radius)
	{
		R_MarkLights(light, bit, node->children[0], surfaces);
		return;
	}
	if (dist < -light->radius)
	{
		R_MarkLights(light, bit, node->children[1], surfaces);
		return;
	}

	// mark the polygons
	for (size_t i = 0; i < node->numsurfaces; i++)
	{
		msurface_t *surf = &surfaces[node->firstsurface + i];
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= BIT(bit);
	}

	R_MarkLights(light, bit, node->children[0], surfaces);
	R_MarkLights(light, bit, node->children[1], surfaces);
}

void R_PushDlights(mnode_t *nodes, msurface_t *surfaces)
{
	r_dlightframecount = r_framecount + 1;	// because the count hasn't
						//  advanced yet for this frame

	for (int i = 0; i < MAX_DLIGHTS; i++)
	{
		if (cl_dlights[i].die < cl.time || !cl_dlights[i].radius)
			continue;

		R_MarkLights(&cl_dlights[i], i, nodes, surfaces);
	}
}

/*
 =============================================================================

 LIGHT SAMPLING

 =============================================================================
 */

mplane_t		*lightplane;
vec3_t			lightspot;

static int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	size_t			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	unsigned	scale;

	if (node->contents < 0)
		return -1;		// didn't hit anything

// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);

	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;

// go down front side
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something

	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing

// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;

	surf = cl.worldmodel->brushmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
			continue; // no lightmaps for sky or water

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += dt * ((surf->extents[0]>>4)+1) + ds;

			for (size_t maps = 0; maps < MAXLIGHTMAPS; maps++)
			{
				byte style = surf->styles[maps];
				if (style == 255)
					break;

				scale = d_lightstylevalue[style];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1);
			}

			r >>= 8;
		}

		return r;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

int R_LightPoint (vec3_t p)
{
	vec3_t		end;
	int			r;

	if (!cl.worldmodel->brushmodel->lightdata)
		return 255;

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint (cl.worldmodel->brushmodel->nodes, p, end);

	if (r == -1)
		r = 0;

	float dynamiclight = 0;
	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			vec3_t dist;
			VectorSubtract(p, cl_dlights[lnum].origin, dist);
			float add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0)
				dynamiclight += add;
		}
	}

	return r + dynamiclight;
}

/*
 ===============================================================================

 LIGHTMAP ALLOCATION

 ===============================================================================
 */

/* returns a texture number and the position inside it */
static int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;

	// ericw -- rather than searching starting at lightmap 0 every time,
	// start at the last lightmap we allocated a surface in.
	// This makes AllocBlock much faster on large levels (can shave off 3+ seconds
	// of load time on a level with 180 lightmaps), at a cost of not quite packing
	// lightmaps as tightly vs. not doing this (uses ~5% more lightmaps)
	for (size_t texnum = last_lightmap_allocated; texnum < MAX_SANITY_LIGHTMAPS ; texnum++)
	{
		if (texnum == lightmap_count)
		{
			lightmap_count++;
			lightmaps = (lightmap_t *) realloc(lightmaps, sizeof(*lightmaps) * lightmap_count);
			memset(&lightmaps[texnum], 0, sizeof(lightmaps[texnum]));
			lightmaps[texnum].data = (byte *) calloc(1, 4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT);
			//as we're only tracking one texture, we don't need multiple copies of allocated any more.
			memset(allocated, 0, sizeof(allocated));
		}
		best = LMBLOCK_HEIGHT;

		for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[i+j] >= best)
					break;
				if (allocated[i+j] > best2)
					best2 = allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LMBLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			allocated[*x + i] = best + h;

		last_lightmap_allocated = texnum;
		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0; //johnfitz -- shut up compiler
}

static void BuildSurfaceLightTex(brush_model_t *brushmodel, msurface_t *fa)
{
	size_t lnumverts = fa->numedges;
	fa->light_tex = (tex_cord *)Q_malloc(sizeof(*fa->light_tex) * lnumverts);
	for (size_t i = 0; i < lnumverts; i++)
	{
		// lightmap texture coordinates
		float s = DotProduct (fa->verts[i], fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= LMBLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		float t = DotProduct (fa->verts[i], fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= LMBLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		fa->light_tex[i].s = s;
		fa->light_tex[i].t = t;
	}
}

static void GL_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;

	surf->lightmapnum = AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);

	byte *base = lightmaps[surf->lightmapnum].data;
	base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s);
	R_BuildLightMap(surf, base);
}

/*
 * Builds the lightmap texture
 * with all the surfaces from all brush models
 */
void GL_BuildLightmaps(void)
{
	memset(allocated, 0, sizeof(allocated));

	r_framecount = 1; // no dlightcache

	/* null out array (the gltexture objects themselves were already freed by Mod_ClearAll) */
	for (size_t i = 0; i < lightmap_count; i++)
		lightmaps[i].texture = NULL;

	for (int i = 1; i < MAX_MODELS; i++)
	{
		model_t *m = cl.model_precache[i];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		if (m->type != mod_brush)
			continue;

		typedef struct vert_s {
			float x, y, z;
		} vert_t;
		std::vector<vert_t> verts;
		std::vector<tex_cord> tex;
		std::vector<tex_cord> light_tex;

		brush_model_t *brushmodel = m->brushmodel;
		for (int j = 0; j < brushmodel->numsurfaces; j++)
		{
			msurface_t *fa = &brushmodel->surfaces[j];
			if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
			{
				GL_SubdivideSurface(brushmodel, fa); // cut up polygon for warps
				// these do not have light maps, just alloc the space anyway
				fa->light_tex = (tex_cord *)Q_malloc(sizeof(*fa->light_tex) * fa->numedges);
			}
			else
			{
				GL_CreateSurfaceLightmap(fa);
				BuildSurfaceLightTex(brushmodel, fa);
			}

			size_t current_size = verts.size();

			for (size_t k = 0; k < fa->numverts; k++)
			{
				verts.push_back({fa->verts[k][0], fa->verts[k][1], fa->verts[k][2]});
				tex.push_back(fa->tex[k]);
				light_tex.push_back(fa->light_tex[k]);
			}

			if (fa->indices.empty())
			{
				for (size_t i = 2; i < fa->numverts; i++)
				{
					fa->indices.push_back(current_size);
					fa->indices.push_back(current_size + i - 1);
					fa->indices.push_back(current_size + i);
				}
			}
			else
			{
				for (size_t i = 0; i < fa->indices.size(); i++)
					fa->indices[i] += current_size;
			}

			glGenBuffers(1, &fa->indicesVBO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fa->indicesVBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(fa->indices[0]) * fa->indices.size(), fa->indices.data(), GL_STATIC_DRAW);
		}

//		glGenBuffers(1, &brushmodel->vertsVBO);
		glBindBuffer(GL_ARRAY_BUFFER, brushmodel->vertsVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(verts[0]) * verts.size(), verts.data(), GL_STATIC_DRAW);

//		glGenBuffers(1, &brushmodel->texVBO);
		glBindBuffer(GL_ARRAY_BUFFER, brushmodel->texVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(tex[0]) * tex.size(), tex.data(), GL_STATIC_DRAW);

//		glGenBuffers(1, &brushmodel->light_texVBO);
		glBindBuffer(GL_ARRAY_BUFFER, brushmodel->light_texVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(light_tex[0]) * light_tex.size(), light_tex.data(), GL_STATIC_DRAW);
	}

	// upload all lightmaps that were filled
	for (size_t i = 0; i < lightmap_count; i++)
	{
		lightmap_t *lightmap = &lightmaps[i];

		lightmap->rectchange.l = LMBLOCK_WIDTH;
		lightmap->rectchange.t = LMBLOCK_HEIGHT;
		lightmap->rectchange.w = 0;
		lightmap->rectchange.h = 0;

		static char name[16];
		snprintf(name, 16, "lightmap%03zu", i);
		byte *data = lightmap->data;
		lightmap->texture = TexMgr_LoadImage(name, LMBLOCK_WIDTH, LMBLOCK_HEIGHT,
		                                        SRC_LIGHTMAP, data, TEX_LINEAR | TEX_NOPICMIP);

		lightmap->modified = false;
	}
}
