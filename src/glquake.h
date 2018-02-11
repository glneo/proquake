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

#ifndef __GLQUAKE_H
#define __GLQUAKE_H

#ifdef OPENGLES
#include <GLES/gl.h>
#include <GLES/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include <math.h>

#include "render.h"

#include "gl_texmgr.h"

#define BACKFACE_EPSILON 0.01

extern int d_lightstylevalue[256]; // 8.8 fraction of base light value

extern int skytexturenum; // index in cl.loadmodel, not gl texture object

extern float gl_max_anisotropy;
extern bool gl_texture_NPOT;

extern int r_framecount;
extern int c_brush_polys, c_alias_polys;

extern float gldepthmin, gldepthmax;

// view origin
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

// screen size info
extern refdef_t r_refdef;
extern mleaf_t *r_viewleaf, *r_oldviewleaf;

// rendering cvar stuffs
extern cvar_t r_drawviewmodel;
extern cvar_t r_waterwarp;
extern cvar_t r_fullbright;
extern cvar_t r_lightmap;
extern cvar_t r_shadows;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;
extern cvar_t r_novis;

// fenix@io.com: model interpolation
extern cvar_t r_interpolate_animation;
extern cvar_t r_interpolate_transform;
extern cvar_t r_interpolate_weapon;

// gl rendering cvars and stuff
extern cvar_t gl_clear;
extern cvar_t gl_cull;
extern cvar_t gl_smoothmodels;
extern cvar_t gl_affinemodels;
extern cvar_t gl_polyblend;

extern cvar_t gl_flashblend;
//extern cvar_t	gl_doubleeyes;
extern cvar_t gl_playermip;
extern cvar_t gl_fullbright;
extern cvar_t r_ringalpha;

extern cvar_t vid_vsync;

extern qpic_t *draw_disc;

typedef enum {
	CANVAS_NONE,
	CANVAS_DEFAULT,
	CANVAS_CONSOLE,
	CANVAS_MENU,
	CANVAS_SBAR,
	CANVAS_WARPIMAGE,
	CANVAS_CROSSHAIR,
	CANVAS_BOTTOMLEFT,
	CANVAS_BOTTOMRIGHT,
	CANVAS_INVALID = -1
} canvastype;

// gl_alias.c
void R_DrawAliasModel(entity_t *ent);

// gl_draw.c
qpic_t *Draw_PicFromWad(const char *name);
qpic_t *Draw_CachePic(const char *path);
void Draw_Character(int x, int y, int num);
void Draw_String(int x, int y, const char *str);
void Draw_Pic(int x, int y, qpic_t *pic, float alpha);
void Draw_TransPic(int x, int y, qpic_t *pic);
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation);
void Draw_PicTile(int x, int y, int w, int h, qpic_t *pic, float alpha);
void Draw_Fill(int x, int y, int w, int h, int c, float alpha);
void Draw_SetCanvas(canvastype newcanvas);
void GL_Begin2D(void);
void GL_End2D(void);
qpic_t *Draw_MakePic(const char *name, int width, int height, byte *data);
void Draw_Init(void);

// gl_light.c
void R_ClearLightmapPolys();
void R_BlendLightmaps(void);
void R_BuildLightMap(msurface_t *surf, byte *dest, int stride);
void R_RenderDynamicLightmaps(msurface_t *fa);
void R_AnimateLight(void);
void R_RenderDlights(void);
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);
void R_PushDlights(void);
int R_LightPoint(vec3_t p);
void GL_BuildLightmaps(void);

// gl_main.c
void GL_RotateForEntity(entity_t *ent);
void GL_PolyBlend(void);
bool R_CullBox(vec3_t mins, vec3_t maxs);
bool R_CullForEntity(const entity_t *ent);
void GL_Setup(void);
void R_TranslatePlayerSkin(int playernum);
void R_NewMap(void);
void R_RenderView(void);
void GL_Init(void);
void GL_BeginRendering();
void GL_EndRendering(void);
void R_Clear(void);
void R_Init(void);

// gl_particle.c
void R_DrawParticles(void);
void R_InitParticleTexture(void);

// gl_sprite.c
void R_DrawSpriteModel(entity_t *ent);

// gl_surface.c
void DrawGLPoly(glpoly_t *p);
void DrawGLPolyLight(glpoly_t *p);
texture_t *R_TextureAnimation(int frame, texture_t *base);
void R_DrawWaterSurfaces(void);
void DrawTextureChains(brush_model_t *brushmodel);
void DrawGLWaterPoly(glpoly_t *p);
void DrawGLWaterPolyLight(glpoly_t *p);
void R_RenderBrushPoly(msurface_t *fa, int frame);
void R_DrawBrushModel(entity_t *ent);
void R_DrawWorld(void);
void R_MarkLeaves(void);

// gl_vidsdl.c
void VID_Swap(void);
void VID_Init(void);
void VID_Shutdown(void);

// gl_warp.c
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa);
void EmitWaterPolys(msurface_t *fa);
void EmitSkyPolys(msurface_t *fa);
void EmitBothSkyLayers(msurface_t *fa);
void R_DrawSkyChain(msurface_t *fa);
void R_InitSky(texture_t *mt);

#endif /* __GLQUAKE_H */
