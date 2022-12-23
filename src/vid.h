/*
 * Video driver definitions
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

#ifndef __VID_H
#define __VID_H

typedef struct vrect_s {
	unsigned int x, y, width, height;
} vrect_t;

typedef enum {
	MODE_NONE = -1,
	MODE_WINDOWED = 0,
	MODE_FULLSCREEN = 1
} modestate_t;

typedef struct {
	int modenum;
	modestate_t type;
	int width;
	int height;
	int bpp;
	int refreshrate;
	char modedesc[17];
} vmode_t;

typedef struct {
	unsigned x;
	unsigned y;
	unsigned width;
	unsigned height;
	float aspect;                   // width / height -- < 0 is taller than wide
	int recalc_refdef;              // if true, recalc vid-based stuff
	unsigned conwidth;
	unsigned conheight;
	modestate_t dispmode;
	int bpp;
	int dispfreq;
} viddef_t;

extern viddef_t vid; // global video state

#endif	/* __VID_H */
