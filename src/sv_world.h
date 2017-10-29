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

#ifndef __WORLD_H
#define __WORLD_H

#define	MOVE_NORMAL     0
#define	MOVE_NOMONSTERS 1
#define	MOVE_MISSILE    2

typedef struct areanode_s
{
	int axis; // -1 = leaf node
	float dist;
	struct areanode_s *children[2];
	link_t trigger_edicts;
	link_t solid_edicts;
} areanode_t;

typedef struct
{
	vec3_t normal;
	float dist;
} plane_t;

typedef struct
{
	bool allsolid; // if true, plane is not valid
	bool startsolid; // if true, the initial point was in a solid area
	bool inopen, inwater;
	float fraction; // time completed, 1.0 = didn't hit anything
	vec3_t endpos; // final position
	plane_t plane; // surface normal at impact
	edict_t *ent; // entity the surface is on
} trace_t;

#define	AREA_DEPTH      4
#define	AREA_NODES      32

void SV_ClearWorld(void);
void SV_UnlinkEdict(edict_t *ent);
void SV_LinkEdict(edict_t *ent, bool touch_triggers);
int SV_PointContents(vec3_t p);
edict_t *SV_TestEntityPosition(edict_t *ent);
bool SV_RecursiveHullCheck(hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);
trace_t SV_Move(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict);

#endif /* __WORLD_H */
