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

typedef struct
{
	vrect_t vrect; // subwindow in video for refresh

	vec3_t vieworg;
	vec3_t viewangles;

	float original_fov_x, original_fov_y;
	float fov_x, fov_y;
} refdef_t;

// refresh
extern refdef_t r_refdef;
extern vec3_t vpn, vright, vup;

void R_RenderView(void); // must set r_refdef first
void R_NewMap(void);
void R_Init(void);

#endif /* __RENDER_H */
