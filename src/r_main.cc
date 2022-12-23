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

#define BACKFACE_EPSILON 0.01

cvar_t r_wateralpha = { "r_wateralpha", "1", CVAR_ARCHIVE };
cvar_t r_dynamic = { "r_dynamic", "1" };
cvar_t r_novis = { "r_novis", "0" };
cvar_t r_interpolate_animation = { "r_interpolate_animation", "0", CVAR_ARCHIVE };
cvar_t r_interpolate_transform = { "r_interpolate_transform", "0", CVAR_ARCHIVE };
cvar_t r_interpolate_weapon = { "r_interpolate_weapon", "0", CVAR_ARCHIVE };
cvar_t r_truegunangle = { "r_truegunangle", "0", CVAR_ARCHIVE };  // Baker 3.80x - Optional "true" gun positioning on viewmodel
cvar_t r_drawviewmodel = { "r_drawviewmodel", "1", CVAR_ARCHIVE };  // Baker 3.80x - Save to config
cvar_t r_ringalpha = { "r_ringalpha", "0.4", CVAR_ARCHIVE }; // Baker 3.80x - gl_ringalpha
cvar_t r_fullbright = { "r_fullbright", "1", CVAR_ARCHIVE };
cvar_t r_lightmap = { "r_lightmap", "0" };
cvar_t r_waterwarp = { "r_waterwarp", "0", CVAR_ARCHIVE }; // Baker 3.60 - Save this to config now
cvar_t r_norefresh = { "r_norefresh", "0" };
cvar_t r_drawentities = { "r_drawentities", "1" };
cvar_t r_speeds = { "r_speeds", "0" };
cvar_t r_shadows = { "r_shadows", "0.3", CVAR_ARCHIVE };
cvar_t r_particles = { "r_particles", "1", CVAR_ARCHIVE };
cvar_t r_particles_alpha = { "r_particles_alpha", "1", CVAR_ARCHIVE };

// For draw stats
int c_brush_polys, c_alias_polys;

// view origin and direction
vec3_t vright, vpn, vup;

// screen size info
refdef_t r_refdef;

mleaf_t *r_viewleaf;

static mplane_t frustum[4];
static int r_visframecount; // bumped when going to a new PVS

int r_framecount;

mtexture_t *r_notexture_mip;

/*
================
R_ClearTextureChains -- ericw

clears texture chains for all textures used by the given model, and also
clears the lightmap chains
================
*/
void R_ClearTextureChains (brush_model_t *mod, texchain_t chain)
{
	size_t i;

	// set all chains to null
	for (i = 0 ; i < mod->numtextures; i++)
		if (mod->textures[i])
			mod->textures[i]->texturechains[chain] = NULL;
}

/* Returns the proper texture for a given time and base texture */
static mtexture_t *R_TextureAnimation(int frame, mtexture_t *base)
{
	// For no texture use the missing texture chain
	if (!base)
		return r_notexture_mip;

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

/* adds the given surface to its texture chain */
void R_ChainSurface(msurface_t *surf, texchain_t chain)
{
	mtexture_t *texture = R_TextureAnimation(0, surf->texinfo->texture);

	surf->texturechain = texture->texturechains[chain];
	texture->texturechains[chain] = surf;
}

/*
================
R_BackFaceCull -- johnfitz -- returns true if the surface is facing away from vieworg
================
*/
bool R_BackFaceCull (msurface_t *surf)
{
	double dot;

	if (surf->plane->type < 3)
		dot = r_refdef.vieworg[surf->plane->type] - surf->plane->dist;
	else
		dot = DotProduct (r_refdef.vieworg, surf->plane->normal) - surf->plane->dist;

	if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
		return true;

	return false;
}

/* mark surfaces based on PVS and rebuild texture chains */
void R_MarkSurfaces(void)
{
	brush_model_t *worldmodel = cl.worldmodel->brushmodel;

	// check this leaf for water portals
	// TODO: loop through all water surfs and use distance to leaf cullbox
	bool nearwaterportal = false;
	for (size_t i = 0; i < r_viewleaf->nummarksurfaces; i++)
		if (r_viewleaf->firstmarksurface[i]->flags & SURF_DRAWTURB)
			nearwaterportal = true;

	// choose vis data
	byte *vis;
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
		vis = Mod_NoVisPVS(worldmodel);
	else if (nearwaterportal)
		vis = Mod_FatPVS(r_refdef.vieworg, worldmodel);
	else
		vis = Mod_LeafPVS(r_viewleaf, worldmodel);

	r_visframecount++;

	// set all chains to null
	R_ClearTextureChains(worldmodel, chain_world);
	R_ClearLightmapPolys(worldmodel);

	// iterate through leaves, marking surfaces
	for (size_t i = 0; i < worldmodel->numleafs; i++)
	{
		mleaf_t *leaf = &worldmodel->leafs[i + 1];

		// Not potentially visible
		if (!(vis[i >> 3] & (1 << (i & 7))))
			continue;

		// Whole leaf out of view
		if (R_CullBox(leaf->bboxmin, leaf->bboxmax))
			continue;

		// add static models
		if (leaf->efrags)
			R_StoreEfrags(&leaf->efrags);

		if (leaf->contents == CONTENTS_SKY)
			continue;

		for (size_t j = 0; j < leaf->nummarksurfaces; j++)
		{
			msurface_t *surf = leaf->firstmarksurface[j];

			// Already marked
			if (surf->visframe == r_visframecount)
				continue;
			surf->visframe = r_visframecount;

			// Surface out of view
			if (R_CullBox(surf->mins, surf->maxs) || R_BackFaceCull(surf))
				continue;

			R_ChainSurface(surf, chain_world);
			R_RenderLightmaps(surf);
		}
	}

	R_UploadLightmaps(worldmodel);
}

/* Returns true if the box is completely outside the frustum */
bool R_CullBox(const vec3_t mins, const vec3_t maxs)
{
	for (int i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return true;

	return false;
}

/* Returns true if the sphere is completely outside the frustum */
#define PlaneDiff(point, plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
static bool R_CullSphere(const vec3_t centre, const float radius)
{
	for (int i = 0; i < 4; i++)
		if (PlaneDiff(centre, &frustum[i]) <= -radius)
			return true;

	return false;
}

bool R_CullForEntity(const entity_t *ent/*, vec3_t returned_center*/)
{
	vec3_t mins, maxs;

	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
		return R_CullSphere(ent->origin, ent->model->radius); // Angles turned; do sphere cull test

	// Angles all 0; do box cull test
	VectorAdd(ent->origin, ent->model->mins, mins); // Add entity origin and model mins to calc mins
	VectorAdd(ent->origin, ent->model->maxs, maxs); // Add entity origin and model maxs to calc maxs

//	if (returned_center)
//		LerpVector (mins, maxs, 0.5, returned_center);

	if (R_CullBox(mins, maxs))
		return true;

	return false;
}

static void R_DrawEntitiesOnList(void)
{
	if (!r_drawentities.value)
		return;

	for (int i = 0; i < cl_numvisedicts; i++)
	{
		entity_t *entity = cl_visedicts[i];

		if (R_CullForEntity(entity))
			continue;

		switch (entity->model->type)
		{
			case mod_alias:
				GL_DrawAliasModel(entity);
				break;
			case mod_brush:
				GL_DrawBrushModel(entity);
				break;
			case mod_sprite:
				GL_DrawSpriteModel(entity);
				break;
		}
	}
}

static void R_DrawViewModel(void)
{
	if (!r_drawviewmodel.value || /* view model disabled */
	    chase_active.value || /* in chase view */
	    !r_drawentities.value || /* entities disabled */
	    ((cl.items & IT_INVISIBILITY) && (r_ringalpha.value == 1.0f)) || /* invisible */
	    (cl.stats[STAT_HEALTH] <= 0)) /* dead */
		return;

	entity_t *entity = &cl.viewent;
	if (!entity->model)
		return;

	GL_DrawAliasModel(entity);
}

static int SignbitsForPlane(mplane_t *out)
{
	int bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

/*
 * turn forward towards side on the plane defined by forward and side
 * if angle = 90, the result will be equal to side
 * assumes side and forward are perpendicular, and normalized
 * to turn away from side, use a negative angle
 */
static void TurnVector(vec3_t out, vec3_t forward, vec3_t side, float angle)
{
	float scale_forward, scale_side;

	scale_forward = cos(DEG2RAD(angle));
	scale_side = sin(DEG2RAD(angle));

	out[0] = scale_forward * forward[0] + scale_side * side[0];
	out[1] = scale_forward * forward[1] + scale_side * side[1];
	out[2] = scale_forward * forward[2] + scale_side * side[2];
}

static void R_SetFrustum(void)
{
	int i;

	TurnVector(frustum[0].normal, vpn, vright, r_refdef.fov_x / 2 - 90); //left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - r_refdef.fov_x / 2); //right plane
	TurnVector(frustum[2].normal, vpn, vup, 90 - r_refdef.fov_y / 2); //bottom plane
	TurnVector(frustum[3].normal, vpn, vup, r_refdef.fov_y / 2 - 90); //top plane

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_refdef.vieworg, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

static void R_SetupFrame(void)
{
	r_framecount++;

	// build the transformation matrix for the given view angles
	AngleVectors(r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_viewleaf = Mod_PointInLeaf(r_refdef.vieworg, cl.worldmodel->brushmodel);

	V_SetContentsColor(r_viewleaf->contents);

	V_CalcBlend();

	r_refdef.fov_x = r_refdef.original_fov_x;
	r_refdef.fov_y = r_refdef.original_fov_y;
	if (r_waterwarp.value)
	{
		int contents = Mod_PointInLeaf(r_refdef.vieworg, cl.worldmodel->brushmodel)->contents;
		if (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
		{
			//variance is a percentage of width, where width = 2 * tan(fov / 2) otherwise the effect is too dramatic at high FOV and too subtle at low FOV.  what a mess!
			r_refdef.fov_x = atan(tan(DEG2RAD(r_refdef.fov_x) / 2) * (0.97 + sin(cl.time * 1.5) * 0.03)) * 2 / M_PI_DIV_180;
			r_refdef.fov_y = atan(tan(DEG2RAD(r_refdef.fov_y) / 2) * (1.03 - sin(cl.time * 1.5) * 0.03)) * 2 / M_PI_DIV_180;
		}
	}
}

void R_NewMap(void)
{
	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (size_t i = 0; i < cl.worldmodel->brushmodel->numleafs; i++)
		cl.worldmodel->brushmodel->leafs[i].efrags = NULL;

	GL_BuildLightmaps();

//	R_ClearTextureChains(cl.worldmodel->brushmodel, chain_world);
}

/* r_refdef must be set before the first call */
void R_RenderView(void)
{
	double time1 = 0.0;
	double time2 = 0.0;

	if (r_norefresh.value)
		return;

	if (!cl.worldmodel)
		Sys_Error("NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish();
		time1 = Sys_DoubleTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	// render normal view
	R_PushDlights(cl.worldmodel->brushmodel->nodes, cl.worldmodel->brushmodel->surfaces);
	R_AnimateLight();
	R_SetupFrame();
	R_SetFrustum();
	GL_Setup();
	R_MarkSurfaces();
	GL_DrawSurfaces(cl.worldmodel->brushmodel, chain_world);
	R_DrawEntitiesOnList();
	GL_DrawParticles();
	R_DrawViewModel();

	if (r_speeds.value)
	{
		glFinish();
		time2 = Sys_DoubleTime();
		double time = time2 - time1;
		Con_Printf("%3i ms (%d fps) %4i wpoly %4i epoly\n", (int)(time * 1000), (int)(1 / time), c_brush_polys, c_alias_polys);
	}
}

void R_Init(void)
{
	Cvar_RegisterVariable(&r_norefresh);
	Cvar_RegisterVariable(&r_lightmap);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_ringalpha);
	Cvar_RegisterVariable(&r_truegunangle);
	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&r_particles);
	Cvar_RegisterVariable(&r_particles_alpha);

	Cvar_RegisterVariable(&r_interpolate_animation);
	Cvar_RegisterVariable(&r_interpolate_transform);
	Cvar_RegisterVariable(&r_interpolate_weapon);

	r_notexture_mip = (mtexture_t *)Q_calloc(1, sizeof(mtexture_t));
	strcpy (r_notexture_mip->name, "notexture");
	r_notexture_mip->width = 32;
	r_notexture_mip->height = 32;
	r_notexture_mip->gltexture = notexture;
}
