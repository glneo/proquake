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

#ifndef __SBAR_H
#define __SBAR_H

extern unsigned int sb_lines; // scan lines to draw

void Sbar_Draw (void); // called every frame by screen

void Sbar_IntermissionOverlay (void); // called each frame after the level has been completed
void Sbar_FinaleOverlay (void);

void Sbar_Init (void);

#endif	/* __SBAR_H */
