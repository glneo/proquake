/*
 * Null vid component
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
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

texture_t *r_notexture_mip;

int r_framecount;

qpic_t *draw_disc; // also used on sbar

cvar_t gl_clear = { "gl_clear", "0" };
cvar_t r_truegunangle = { "r_truegunangle", "0", true };  // Baker 3.80x - Optional "true" gun positioning on viewmodel
cvar_t r_drawviewmodel = { "r_drawviewmodel", "1", true };  // Baker 3.80x - Save to config
cvar_t r_ringalpha = { "r_ringalpha", "0.4", true }; // Baker 3.80x - gl_ringalpha
cvar_t r_interpolate_transform = { "r_interpolate_transform", "0", true };

cvar_t cl_crosshaircentered = { "cl_crosshaircentered", "1", true };
cvar_t crosshair = { "crosshair", "1", true };

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	vid.width = 800;
	vid.height = 600;
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;

	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;
}
void GL_EndRendering(void) {}
void VID_Shutdown(void) {}
void VID_Init(unsigned char *palette) {}

void Draw_Init(void) {}
void Draw_Character(int x, int y, int num) {}
void Draw_DebugChar(char num) {}
void Draw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height) {}
void Draw_Pic(int x, int y, qpic_t *pic) {}
void Draw_TransPic(int x, int y, qpic_t *pic) {}
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation) {}
void Draw_ConsoleBackground(int lines) {}
void Draw_BeginDisc(void) {}
void Draw_EndDisc(void) {}
void Draw_TileClear(int x, int y, int w, int h) {}
void Draw_Fill(int x, int y, int w, int h, int c) {}
void Draw_AlphaFill(int x, int y, int w, int h, int c, float alpha) {}
void Draw_FadeScreen(void) {}
void Draw_String(int x, int y, char *str) {}
qpic_t *Draw_PicFromWad(char *name) { return NULL; }
qpic_t *Draw_CachePic(char *path) { return NULL; }
void Draw_AlphaPic(int x, int y, qpic_t *pic, float alpha) {}
void Draw_Crosshair(void) {}

void GL_Set2D(void) {}
void R_PushDlights(void) {}

void R_RenderView(void) {}

void R_Init(void) {}
int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode) { return 0; }
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa) {}
void ConvertPixels(byte *pixels, int num_pixels) {}
int FindFullbrightTexture(byte *pixels, int num_pix)  { return 0; }
void R_InitSky(texture_t *mt, byte *src) {}
void GL_FreeTextures(void) {}
void R_TranslatePlayerSkin(int playernum) {}
void R_NewMap(void) {}
