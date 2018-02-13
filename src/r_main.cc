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

cvar_t r_norefresh = { "r_norefresh", "0" };
cvar_t r_drawentities = { "r_drawentities", "1" };
cvar_t r_speeds = { "r_speeds", "0" };
cvar_t r_shadows = { "r_shadows", "0" };

// For draw stats
int c_brush_polys, c_alias_polys;

// view origin and direction
vec3_t r_origin, vright, vpn, vup;

// screen size info
refdef_t r_refdef;

mleaf_t *r_viewleaf, *r_oldviewleaf;

static mplane_t frustum[4];
static int r_visframecount; // bumped when going to a new PVS

static void R_RecursiveWorldNode(mnode_t *node)
{
	int c, side;
	mplane_t *plane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	double dot;

	if (node->contents == CONTENTS_SOLID)
		return; // solid

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

static void R_MarkLeaves(void)
{
	byte *vis, solid[4096];
	mnode_t *node;
	extern cvar_t gl_nearwater_fix;
	bool nearwaterportal = false;

	// Check if near water to avoid HOMs when crossing the surface
	if (gl_nearwater_fix.value)
	{
		msurface_t **mark = r_viewleaf->firstmarksurface;

		for (int i = 0; i < r_viewleaf->nummarksurfaces; i++)
		{
			if (mark[i]->flags & SURF_DRAWTURB)
			{
				nearwaterportal = true;
				// Con_SafePrintf("R_MarkLeaves: nearwaterportal, surfs=%d\n", r_viewleaf->nummarksurfaces);
				break;
			}
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
		vis = SV_FatPVS(r_origin, cl.worldmodel);
	}
	else
	{
		vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel->brushmodel);
	}

	for (int i = 0; i < cl.worldmodel->brushmodel->numleafs; i++)
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

void R_DrawWorld(void)
{
	R_ClearLightmapPolys();

	R_MarkLeaves();

	R_RecursiveWorldNode(cl.worldmodel->brushmodel->nodes);

	DrawTextureChains(cl.worldmodel->brushmodel);

	R_BlendLightmaps();
}

/* Returns true if the box is completely outside the frustum */
bool R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return true;

	return false;
}

/* Returns true if the box is completely outside the frustum */
static bool R_CullBoxA(const vec3_t emins, const vec3_t emaxs)
{
	int i;
	mplane_t *p;
	for (i = 0; i < 4; i++)
	{
		p = frustum + i;
		switch (p->signbits)
		{
		default:
		case 0:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}

/* Returns true if the sphere is completely outside the frustum */
#define PlaneDiff(point, plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
static bool R_CullSphere(const vec3_t centre, const float radius)
{
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++)
	{
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

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

	return R_CullBoxA(mins, maxs);
}

static void R_DrawEntitiesOnList(void)
{
	if (!r_drawentities.value)
		return;

	for (int i = 0; i < cl_numvisedicts; i++)
	{
		entity_t *entity = cl_visedicts[i];

		switch (entity->model->type)
		{
			case mod_alias:
				R_DrawAliasModel(entity);
				break;
			case mod_brush:
				R_DrawBrushModel(entity);
				break;
			case mod_sprite:
				R_DrawSpriteModel(entity);
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

	R_DrawAliasModel(entity);
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
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

static void R_SetupFrame(void)
{
	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.vieworg, r_origin);
	AngleVectors(r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel->brushmodel);

	V_SetContentsColor(r_viewleaf->contents);

	V_CalcBlend();

	c_brush_polys = 0;
	c_alias_polys = 0;
}

void R_NewMap(void)
{
	int i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->brushmodel->numleafs; i++)
		cl.worldmodel->brushmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();

	GL_BuildLightmaps();

	// identify sky texture
	skytexturenum = -1;

	for (i = 0; i < cl.worldmodel->brushmodel->numtextures; i++)
	{
		if (!cl.worldmodel->brushmodel->textures[i])
			continue;
		if (!strncmp(cl.worldmodel->brushmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		cl.worldmodel->brushmodel->textures[i]->texturechain = NULL;
	}
}

/* r_refdef must be set before the first call */
void R_RenderView(void)
{
	double time1, time2;

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

	R_Clear();

	// render normal view
	R_AnimateLight();
	R_SetupFrame();
	R_SetFrustum();
	GL_Setup();
	R_DrawWorld();
	R_DrawEntitiesOnList();
	R_DrawParticles();
	R_DrawViewModel();
	GL_PolyBlend();

	if (r_speeds.value)
	{
		glFinish();
		time2 = Sys_DoubleTime();
		Con_Printf("%3i ms  %4i wpoly %4i epoly\n", (int) ((time2 - time1) * 1000), c_brush_polys, c_alias_polys);
	}
}

/* For program optimization */
static void R_TimeRefresh_f(void)
{
	int i;
	float start, stop, time;

	if (cls.state != ca_connected)
		return;

	glFinish();

	start = Sys_DoubleTime();
	for (i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView();
	}

	glFinish();
	stop = Sys_DoubleTime();
	time = stop - start;
	Con_Printf("%f seconds (%f fps)\n", time, 128.0 / time);

	GL_EndRendering();
}

void R_Init(void)
{
	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);

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


	Cvar_RegisterVariable(&r_interpolate_animation);
	Cvar_RegisterVariable(&r_interpolate_transform);
	Cvar_RegisterVariable(&r_interpolate_weapon);

	Cvar_RegisterVariable(&gl_clear);

	R_InitParticles();
	R_InitParticleTexture();
}
