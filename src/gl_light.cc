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

#define MAX_SANITY_LIGHTMAPS (1u<<20)

int d_lightstylevalue[MAX_LIGHTSTYLES]; // 8.8 fraction of base light value

static void R_ClearLightStyles(void)
{
	for (size_t i = 0; i < MAX_LIGHTSTYLES; i++)
		d_lightstylevalue[i] = 264; // normal light value
}

void R_ClearLightmapPolys(brush_model_t *brushmodel)
{
	for (size_t i = 0; i < brushmodel->lightmap_count; i++)
	{
		lightmap_t *lightmap = &((*brushmodel->lightmaps)[i]);
		lightmap->indices->clear();
	}
}

/* uploads the modified lightmaps if necessary */
void R_UploadLightmaps(brush_model_t *brushmodel)
{
	for (size_t i = 0; i < brushmodel->lightmap_count; i++)
	{
		lightmap_t *lightmap = &((*brushmodel->lightmaps)[i]);

		if (!lightmap->modified)
			continue;

		glRect_t *theRect = &lightmap->rectchange;
		byte *data = (byte *)lightmap->data;

#ifdef OPENGLES
		GLenum format = GL_LUMINANCE;
#else
		GLenum format = GL_RED;
#endif

		// Not previously uploaded
		if (!lightmap->texture)
		{
			static char name[16];
			snprintf(name, 16, "lightmap%03zu", i);
			lightmap->texture = TexMgr_LoadImage(name, LMBLOCK_WIDTH, LMBLOCK_HEIGHT,
			                                     SRC_LIGHTMAP, data, TEX_LINEAR | TEX_NOPICMIP);
		}
		else
		{
			static char name[16];
			snprintf(name, 16, "lightmap%03zu", i);
			lightmap->texture = TexMgr_LoadImage(name, LMBLOCK_WIDTH, LMBLOCK_HEIGHT,
						             SRC_LIGHTMAP, data, TEX_OVERWRITE | TEX_LINEAR | TEX_NOPICMIP);

//			GL_BindToUnit(GL_TEXTURE0, lightmap->texture);
//			glTexSubImage2D(GL_TEXTURE_2D, 0,
//					theRect->x             , theRect->y             ,
//					theRect->w - theRect->x, theRect->h - theRect->y,
//					format, GL_UNSIGNED_BYTE,
//					data);
		}

		theRect->x = LMBLOCK_WIDTH;
		theRect->y = LMBLOCK_HEIGHT;
		theRect->w = 0;
		theRect->h = 0;
		lightmap->modified = false;
	}
}

static void R_AddDynamicLights(msurface_t *surf, unsigned int *dest)
{
	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;

	mtexinfo_t *tex = surf->texinfo;

	for (int lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & BIT(lnum)))
			continue; // not lit by this light

		dlight_t *dlight = &cl_dlights[lnum];

		float rad = dlight->radius;
		float dist = DotProduct(dlight->origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		float minlight = dlight->minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		vec3_t impact;
		for (int i = 0; i < 3; i++)
			impact[i] = dlight->origin[i] - surf->plane->normal[i] * dist;

		float local_s = DotProduct (impact, tex->vecs[0].vecs) + tex->vecs[0].offset;
		float local_t = DotProduct (impact, tex->vecs[1].vecs) + tex->vecs[1].offset;

		local_s -= surf->texturemins[0];
		local_t -= surf->texturemins[1];

		for (int t = 0; t < tmax; t++)
		{
			int td = local_t - t * 16;
			if (td < 0)
				td = -td;
			for (int s = 0; s < smax; s++)
			{
				int sd = local_s - s * 16;
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

	surf->dlightbits = 0;
}

unsigned int blocklights[LMBLOCK_WIDTH * LMBLOCK_HEIGHT];

/* Combine and scale multiple lightmaps into the 8.8 format in blocklights */
static void R_BuildLightMap(msurface_t *surf)
{
	lightmap_t *lightmap = surf->lightmap;

	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;
	size_t size = smax * tmax;

	if (size > LMBLOCK_WIDTH * LMBLOCK_HEIGHT)
		Sys_Error("Out of bounds light extent");

	byte *surfacemap = surf->samples;

	// set to full bright if no light data
	if (!surfacemap)
	{
		for (size_t t = surf->light_t; t < (surf->light_t + tmax); t++)
			for (size_t s = surf->light_s; s < (surf->light_s + smax); s++)
				lightmap->data[t][s] = 255;
		return;
	}

	// clear to no light
	for (size_t i = 0; i < size; i++)
		blocklights[i] = 0;

	// add all the lightmaps
	for (size_t maps = 0; maps < MAXLIGHTMAPS; maps++)
	{
		byte style = surf->styles[maps];
		if (style == 255)
			break;

		unsigned int scale = d_lightstylevalue[style];
		surf->cached_light[maps] = scale; // 8.8 fraction
		for (size_t i = 0; i < size; i++)
			blocklights[i] += surfacemap[i] * scale;
		surfacemap += size; // skip to next lightmap
	}

	// add all the dynamic lights if needed
	surf->cached_dlight = !!surf->dlightbits;
	if (surf->dlightbits)
		R_AddDynamicLights(surf, blocklights);

	// bound, invert, and shift
	unsigned int *base_pointer = blocklights;
	for (int t = 0; t < tmax; t++)
	{
		for (int s = 0; s < smax; s++)
		{
			unsigned int l = *base_pointer++;
			byte light = (byte)((l >> 8) & 0xff);
			lightmap->data[t + surf->light_t][s + surf->light_s] = light;
		}
	}

	// Area modified
	glRect_t *theRect = &lightmap->rectchange;
	theRect->x = min(theRect->x, surf->light_s);
	theRect->y = min(theRect->y, surf->light_t);
	theRect->w = max(theRect->w, surf->light_s + smax);
	theRect->h = max(theRect->h, surf->light_t + tmax);
	lightmap->modified = true;
}

void R_RenderLightmaps(msurface_t *fa)
{
	if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		return;

	lightmap_t *lightmap = fa->lightmap;

	lightmap->indices->insert(std::end(*lightmap->indices), std::begin(*fa->indices), std::end(*fa->indices));

	bool dynamic = false;

	// check for lightmap modification
	for (int maps = 0; maps < MAXLIGHTMAPS; maps++)
	{
		byte style = fa->styles[maps];
		if (style == 255)
			break;

		if (d_lightstylevalue[style] != fa->cached_light[maps])
		{
			// one of the styles changed
			dynamic = true;
			break;
		}
	}

	// dlights this frame or last frame
	if (fa->dlightbits || fa->cached_dlight)
		dynamic = true;

	if(!dynamic || !r_dynamic.value)
		return;

	R_BuildLightMap(fa);
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
		R_MarkLights(light, bit, node->left_node, surfaces);
		return;
	}
	if (dist < -light->radius)
	{
		R_MarkLights(light, bit, node->right_node, surfaces);
		return;
	}

	// mark the polygons
	for (size_t i = 0; i < node->numsurfaces; i++)
	{
		msurface_t *surf = &surfaces[node->firstsurface + i];
		surf->dlightbits |= BIT(bit);
	}

	R_MarkLights(light, bit, node->left_node, surfaces);
	R_MarkLights(light, bit, node->right_node, surfaces);
}

void R_PushDlights(mnode_t *nodes, msurface_t *surfaces)
{
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
	{
		mnode_t *node_side = (side == 0) ? node->left_node : node->right_node;
		return RecursiveLightPoint (node_side, start, end);
	}


	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;

// go down front side
	mnode_t *node_side = (side == 0) ? node->left_node : node->right_node;
	r = RecursiveLightPoint (node_side, start, mid);
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

		s = DotProduct (mid, tex->vecs[0].vecs) + tex->vecs[0].offset;
		t = DotProduct (mid, tex->vecs[1].vecs) + tex->vecs[1].offset;

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
	mnode_t *node_other_side = (side != 0) ? node->left_node : node->right_node;
	return RecursiveLightPoint (node_other_side, mid, end);
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

/* returns a lightmap and the position inside it */
static lightmap_t *AllocBlock (brush_model_t *brushmodel, size_t w, size_t h, size_t *x, size_t *y)
{
	// ericw -- rather than searching starting at lightmap 0 every time,
	// start at the last lightmap we allocated a surface in.
	// This makes AllocBlock much faster on large levels (can shave off 3+ seconds
	// of load time on a level with 180 lightmaps), at a cost of not quite packing
	// lightmaps as tightly vs. not doing this (uses ~5% more lightmaps)
	for (size_t lmapnum = brushmodel->last_lightmap_allocated; lmapnum < MAX_SANITY_LIGHTMAPS ; lmapnum++)
	{
//		if (lmapnum == brushmodel->lightmap_count)
//		{
////			brushmodel->lightmaps = (lightmap_t **)Q_realloc(brushmodel->lightmaps, sizeof(lightmap_t *) * (brushmodel->lightmap_count + 1));
//			brushmodel->lightmaps[lmapnum] = (lightmap_t *)Q_malloc(sizeof(lightmap_t));
//			lightmap_t *lightmap = brushmodel->lightmaps[lmapnum];
//			lightmap->texture = NULL;
//			lightmap->indices = new std::vector<unsigned short>();
//			lightmap->modified = false;
//			lightmap->rectchange.x = LMBLOCK_WIDTH;
//			lightmap->rectchange.y = LMBLOCK_HEIGHT;
//			lightmap->rectchange.w = 0;
//			lightmap->rectchange.h = 0;
//			memset(lightmap->allocated, 0, sizeof(lightmap->allocated));
//			lightmap->data = (byte *)Q_malloc(LMBLOCK_WIDTH * LMBLOCK_HEIGHT);
//			brushmodel->lightmap_count++;
//		}
		lightmap_t *lightmap = &((*brushmodel->lightmaps)[lmapnum]);

		size_t best = LMBLOCK_HEIGHT;

		for (size_t i = 0; i < LMBLOCK_WIDTH - w; i++)
		{
			size_t local_best = 0;

			size_t j = 0;
			for (j = 0; j < w; j++)
			{
				if (lightmap->allocated[i + j] >= best)
					break;
				if (lightmap->allocated[i + j] > local_best)
					local_best = lightmap->allocated[i+j];
			}
			if (j == w)
			{
				// this is a valid spot
				best = local_best;
				*x = i;
				*y = local_best;
			}
		}

		if (best + h > LMBLOCK_HEIGHT)
			continue;

		for (size_t i = 0; i < w; i++)
			lightmap->allocated[*x + i] = *y + h;

		brushmodel->last_lightmap_allocated = lmapnum;
		return lightmap;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}

static void BuildSurfaceLightTex(brush_model_t *brushmodel, msurface_t *fa)
{
	fa->light_tex = (tex_cord *)Q_malloc(sizeof(*fa->light_tex) * fa->numverts);
	for (size_t i = 0; i < fa->numverts; i++)
	{
		// lightmap texture coordinates
		vec3_t temp = { fa->verts[i].x, fa->verts[i].y, fa->verts[i].z };

		float s = DotProduct (temp, fa->texinfo->vecs[0].vecs) + fa->texinfo->vecs[0].offset;
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= LMBLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		float t = DotProduct (temp, fa->texinfo->vecs[1].vecs) + fa->texinfo->vecs[1].offset;
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= LMBLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		fa->light_tex[i].s = s;
		fa->light_tex[i].t = t;
	}
}

static void GL_CreateSurfaceLightmap(brush_model_t *brushmodel, msurface_t *surf)
{
	size_t smax = (surf->extents[0] >> 4) + 1;
	size_t tmax = (surf->extents[1] >> 4) + 1;

	if (surf->lightmap)
		Sys_Error("Uncleared lightmap");

	surf->lightmap = AllocBlock(brushmodel, smax, tmax, &surf->light_s, &surf->light_t);

	surf->dlightbits = 0; // no dlights for base lightmap
	surf->cached_dlight = 0;

	R_BuildLightMap(surf);
}

static void GL_ClearLightMaps(brush_model_t *brushmodel)
{
	brushmodel->lightmaps = (lightmap_t (*)[10])Q_malloc(sizeof(lightmap_t) * 10);
	for (size_t lmapnum = 0; lmapnum < 10; lmapnum++)
	{
		lightmap_t *lightmap = &((*brushmodel->lightmaps)[lmapnum]);
		lightmap->texture = NULL;
		lightmap->indices = new std::vector<unsigned short>();
		lightmap->modified = false;
		lightmap->rectchange.x = LMBLOCK_WIDTH;
		lightmap->rectchange.y = LMBLOCK_HEIGHT;
		lightmap->rectchange.w = 0;
		lightmap->rectchange.h = 0;
		memset(lightmap->allocated, 0, sizeof(lightmap->allocated));
		memset(lightmap->data, 0, sizeof(lightmap->data));
//		lightmap->data = (byte *)Q_malloc(LMBLOCK_WIDTH * LMBLOCK_HEIGHT);
		brushmodel->lightmap_count++;
	}

	brushmodel->last_lightmap_allocated = 0;
}

void GL_CreateSurfaceLists(brush_model_t *brushmodel)
{
	brushmodel->lightmap_count = 0;
	R_ClearLightStyles();
	GL_ClearLightMaps(brushmodel);

	typedef struct vert_s {
		float x, y, z;
	} vert_t;
	std::vector<vert_t> verts;
	std::vector<tex_cord> tex;
	std::vector<tex_cord> light_tex;

	for (size_t j = 0; j < brushmodel->numsurfaces; j++)
	{
		msurface_t *fa = &brushmodel->surfaces[j];

		size_t current_size = verts.size();

		if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
		{
			GL_SubdivideSurface(brushmodel, fa); // cut up polygon for warps
			for (size_t i = 0; i < fa->indices->size(); i++)
				(*fa->indices)[i] += current_size;
			glGenBuffers(1, &fa->indicesVBO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fa->indicesVBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof((*fa->indices)[0]) * fa->indices->size(), fa->indices->data(), GL_STATIC_DRAW);
		}
		else
		{
			for (size_t i = 2; i < fa->numverts; i++)
			{
				fa->indices->push_back(current_size);
				fa->indices->push_back(current_size + i - 1);
				fa->indices->push_back(current_size + i);
			}
			GL_CreateSurfaceLightmap(brushmodel, fa);
			BuildSurfaceLightTex(brushmodel, fa);
		}

		for (size_t k = 0; k < fa->numverts; k++)
		{
			verts.push_back({fa->verts[k].x, fa->verts[k].y, fa->verts[k].z});
			tex.push_back(fa->tex[k]);
			if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
				light_tex.push_back({0, 0});
			else
				light_tex.push_back(fa->light_tex[k]);
		}
	}

	// upload all lightmaps that were filled
	R_UploadLightmaps(brushmodel);

	glGenBuffers(1, &brushmodel->vertsVBO);
	glBindBuffer(GL_ARRAY_BUFFER, brushmodel->vertsVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts[0]) * verts.size(), verts.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &brushmodel->texVBO);
	glBindBuffer(GL_ARRAY_BUFFER, brushmodel->texVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tex[0]) * tex.size(), tex.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &brushmodel->light_texVBO);
	glBindBuffer(GL_ARRAY_BUFFER, brushmodel->light_texVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(light_tex[0]) * light_tex.size(), light_tex.data(), GL_STATIC_DRAW);
}

/*
 * Builds the lightmap texture
 * with all the surfaces from all brush models
 */
void GL_BuildLightmaps(void)
{
//	R_ClearLightStyles();
////	GL_ClearLightMaps();
//
//	for (int i = 1; i < MAX_MODELS; i++)
//	{
//		model_t *m = cl.model_precache[i];
//		if (!m)
//			break;
//		if (m->name[0] == '*')
//			continue;
//		if (m->type != mod_brush)
//			continue;
//
//		std::vector<tex_cord> light_tex;
//
//		brush_model_t *brushmodel = m->brushmodel;
//		for (int j = 0; j < brushmodel->numsurfaces; j++)
//		{
//			msurface_t *fa = &brushmodel->surfaces[j];
//			if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
//				continue;
//
//			GL_CreateSurfaceLightmap(brushmodel, fa);
//			BuildSurfaceLightTex(brushmodel, fa);
//
//			for (size_t k = 0; k < fa->numverts; k++)
//				if (fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
//					light_tex.push_back({0, 0});
//				else
//					light_tex.push_back(fa->light_tex[k]);
//		}
//
//		glBindBuffer(GL_ARRAY_BUFFER, brushmodel->light_texVBO);
//		glBufferData(GL_ARRAY_BUFFER, sizeof(light_tex[0]) * light_tex.size(), light_tex.data(), GL_STATIC_DRAW);
//	}

	// upload all lightmaps that were filled
//	R_UploadLightmaps();
}
