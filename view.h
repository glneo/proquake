/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl_view.h

// Baker hwgamma support

extern	cvar_t	v_gamma;
extern	cvar_t	v_contrast;
extern	cvar_t	gl_hwblend;

extern	cvar_t		vold_gamma;
// Baker end hwgamma support

extern	byte		gammatable[256];	// palette is sent through this
extern	byte		rampsold[3][256];
extern float v_blend[4];

extern cvar_t lcd_x;

void V_Init (void);
void V_RenderView (void);

void V_CalcBlend (void);

void SCR_DrawVolume (void);
void SCR_DrawCoords (void);

float V_CalcRoll (vec3_t angles, vec3_t velocity);


qboolean V_UpdatePalette_Hardware (void);


void V_UpdatePalette_Static (qboolean forced);



void BuildGammaTable (float g);			// JPG 3.02



