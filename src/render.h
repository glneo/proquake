/*
 * Public interface to refresh functions
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

#ifndef __RENDER_H
#define __RENDER_H

#define	TOP_RANGE       16 // soldier uniform colors
#define	BOTTOM_RANGE    96

//=============================================================================
#define ISTRANSPARENT(ent) ((ent)->istransparent && (ent)->transparency > 0 && (ent)->transparency < 1)

typedef struct efrag_s
{
	struct mleaf_s *leaf;
	struct efrag_s *leafnext;
	struct entity_s *entity;
	struct efrag_s *entnext;
} efrag_t;

typedef struct
{
	vec3_t origin;
	vec3_t angles;
	int modelindex;
	int frame;
	int colormap;
	int skin;
	int effects;
} entity_state_t;

typedef struct entity_s
{
	bool forcelink; // model changed

	int update_type;

	entity_state_t baseline; // to fill in defaults in updates

	double msgtime; // time of last update
	vec3_t msg_origins[2]; // last two updates (0 is newest)
	vec3_t origin;
	vec3_t msg_angles[2]; // last two updates (0 is newest)
	vec3_t angles;
	struct model_s *model; // NULL = no model
	struct efrag_s *efrag; // linked list of efrags
	int frame;
	float syncbase; // for client-side animations
	int colormap;
	int effects; // light, particals, etc
	size_t skinnum; // for Alias models
	int visframe; // last frame this entity was found in an active leaf

	int dlightframe; // dynamic lighting
	int dlightbits;

	// FIXME: could turn these into a union
	int trivial_accept;
	struct mnode_s *topnode; // for bmodels, first world node that splits bmodel, or NULL if not split

	int modelindex;

	// fenix@io.com: model animation interpolation
	float frame_start_time;
	float frame_interval;
	int lastpose, currpose;

	// fenix@io.com: model transform interpolation
	float translate_start_time;
	vec3_t lastorigin, currorigin;

	float rotate_start_time;
	vec3_t lastangles, currangles;

	bool monsterstep;

	float alpha;
} entity_t;

typedef struct
{
	vrect_t vrect; // subwindow in video for refresh

	vec3_t vieworg;
	vec3_t viewangles;

	float fov_x, fov_y;
} refdef_t;

// refresh

extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;

extern struct texture_s *r_notexture_mip;
extern struct texture_s *r_notexture_mip2;

void R_RenderView(void); // must set r_refdef first
void R_NewMap(void);
void R_Init(void);

#endif /* __RENDER_H */
