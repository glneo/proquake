/*
 * Chase camera code
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

cvar_t chase_back = { "chase_back", "100" };
cvar_t chase_up = { "chase_up", "16" };
cvar_t chase_right = { "chase_right", "0" };

cvar_t chase_yaw = { "chase_yaw", "0" };
cvar_t chase_roll = { "chase_roll", "0" };
cvar_t chase_pitch = { "chase_pitch", "45" };

cvar_t chase_active = { "chase_active", "0" };

static void Chase_TraceLine(vec3_t start, vec3_t end, vec3_t impact)
{
	trace_t trace;

	memset(&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck(cl.worldmodel->brushmodel->hulls, 0, 0, 1, start, end, &trace);

	VectorCopy(trace.endpos, impact);
}

void Chase_Update(void)
{
	vec3_t forward, up, right, dest, stop, chase_dest;

	if ((int) chase_active.value != 2)
	{
		// if can't see player, reset
		AngleVectors(cl.lerpangles, forward, right, up);

		// calc exact destination
		for (int i = 0; i < 3; i++)
			chase_dest[i] = r_refdef.vieworg[i] -
			                (forward[i] * chase_back.value) -
					(right[i] * chase_right.value);
		chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;

		// find the spot the player is looking at
		VectorMA(r_refdef.vieworg, 4096, forward, dest);
		Chase_TraceLine(r_refdef.vieworg, dest, stop);
		// calculate pitch to look at the same spot from camera
		VectorSubtract(stop, r_refdef.vieworg, stop);

		float dist = max(1, DotProduct(stop, forward));
		r_refdef.viewangles[PITCH] = -180 / M_PI * atan2f(stop[2], dist);
		r_refdef.viewangles[YAW] -= chase_yaw.value;

		Chase_TraceLine(r_refdef.vieworg, chase_dest, stop);
		if (stop[0] != 0 || stop[1] != 0 || stop[2] != 0)
		{
			// update the camera destination to where we hit the wall
			VectorCopy(stop, chase_dest);
			// this prevents the camera from poking into the wall by rounding off the traceline
			LerpVector(r_refdef.vieworg, chase_dest, 0.8f, chase_dest);
		}
		// move towards destination
		VectorCopy(chase_dest, r_refdef.vieworg);
	}
	else
	{
		chase_dest[0] = r_refdef.vieworg[0] + chase_back.value;
		chase_dest[1] = r_refdef.vieworg[1] + chase_right.value;
		chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;

		Chase_TraceLine(r_refdef.vieworg, chase_dest, stop);
		if (VectorLength(stop) != 0)
			VectorCopy(stop, chase_dest);

		VectorCopy(chase_dest, r_refdef.vieworg);
		r_refdef.viewangles[ROLL] = chase_roll.value;
		r_refdef.viewangles[YAW] = chase_yaw.value;
		r_refdef.viewangles[PITCH] = chase_pitch.value;
	}
}

void Chase_Init(void)
{
	Cvar_RegisterVariable(&chase_back);
	Cvar_RegisterVariable(&chase_up);
	Cvar_RegisterVariable(&chase_right);

	Cvar_RegisterVariable(&chase_pitch);
	Cvar_RegisterVariable(&chase_yaw);
	Cvar_RegisterVariable(&chase_roll);

	Cvar_RegisterVariable(&chase_active);
}
