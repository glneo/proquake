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
#include "glquake.h"

viddef_t vid;

// gl_draw.c
void Scrap_Upload(void) { return; }
qpic_t *Draw_PicFromWad(const char *name) { return NULL; }
qpic_t *Draw_CachePic(const char *path) { return NULL; }
void Draw_Character(int x, int y, int num, float alpha) { return; }
void Draw_String(int x, int y, const char *str, float alpha) { return; }
void Draw_Pic(int x, int y, qpic_t *pic, float alpha) { return; }
void Draw_TransPic(int x, int y, qpic_t *pic, float alpha) { return; }
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation) { return; }
void Draw_PicTile(int x, int y, int w, int h, qpic_t *pic, float alpha) { return; }
void Draw_Fill(int x, int y, int w, int h, int c, float alpha) { return; }
void Draw_SetCanvas(canvastype newcanvas) { return; }
void GL_Begin2D(void) { return; }
void GL_End2D(void) { return; }
qpic_t *Draw_MakePic(const char *name, int width, int height, byte *data) { return NULL; }
void Draw_Init(void) { return; }

void R_DrawAliasModel(entity_t *ent) { return; }
void R_DrawBrushModel(entity_t *ent) { return; }
void R_DrawSpriteModel(entity_t *ent) { return; }
