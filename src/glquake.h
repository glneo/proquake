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
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "render.h"

#include "gl_texmgr.h"

#define BACKFACE_EPSILON 0.01

#define BUFFER_OFFSET(i) ((void*)(i))

extern int d_lightstylevalue[256]; // 8.8 fraction of base light value

extern int skytexturenum; // index in cl.loadmodel, not gl texture object

extern float gl_max_anisotropy;
extern bool gl_texture_NPOT;

extern int r_framecount;
extern int c_brush_polys, c_alias_polys;

extern Q_Matrix modelViewMatrix;
extern Q_Matrix projectionMatrix;

// view origin
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;

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
extern cvar_t r_particles;
extern cvar_t r_particles_alpha;

// fenix@io.com: model interpolation
extern cvar_t r_interpolate_animation;
extern cvar_t r_interpolate_transform;
extern cvar_t r_interpolate_weapon;

// gl rendering cvars and stuff
extern cvar_t gl_clear;
extern cvar_t gl_smoothmodels;
extern cvar_t gl_affinemodels;
extern cvar_t gl_polyblend;
extern cvar_t gl_overbright;

//extern cvar_t gl_flashblend;
//extern cvar_t	gl_doubleeyes;
extern cvar_t gl_playermip;
extern cvar_t gl_fullbright;
extern cvar_t r_ringalpha;

extern cvar_t vid_vsync;
extern cvar_t vid_gamma;
extern cvar_t vid_contrast;

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
void GL_UploadAliasVBOs(alias_model_t *aliasmodel);
void GL_DrawAliasModel(entity_t *ent);
void GL_CreateAliasShaders(void);

// gl_draw.c
void Scrap_Upload(void);
qpic_t *Draw_PicFromWad(const char *name);
qpic_t *Draw_CachePic(const char *path);
void Draw_Character(int x, int y, int num, float alpha);
void Draw_String(int x, int y, const char *str, float alpha);
void Draw_Pic(int x, int y, qpic_t *pic, float alpha);
void Draw_TransPic(int x, int y, qpic_t *pic, float alpha);
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, float alpha, int top, int bottom);
void Draw_PicTile(int x, int y, int w, int h, qpic_t *pic, float alpha);
void Draw_Fill(int x, int y, int w, int h, int c, float alpha);
void Draw_PolyBlend(void);
void Draw_SetCanvas(canvastype newcanvas);
void GL_Begin2D(void);
void GL_End2D(void);
qpic_t *Draw_MakePic(const char *name, int width, int height, byte *data);
void Draw_Init(void);

// gl_light.c
void R_ClearLightmapPolys(void);
void R_UploadLightmaps(void);
void R_ClearLightStyle(void);
void R_RenderDynamicLightmaps(msurface_t *fa);
void R_AnimateLight(void);
void R_RenderDlights(void);
void R_MarkLights(dlight_t *light, int bit, mnode_t *node);
void R_PushDlights(mnode_t *nodes, msurface_t *surfaces);
int R_LightPoint (vec3_t p);
void GL_BuildLightmaps(void);

// gl_main.c
void GL_RotateForEntity(entity_t *ent, Q_Matrix &matrix);
bool R_CullBox(const vec3_t mins, const vec3_t maxs);
bool R_CullForEntity(const entity_t *ent);
void GL_Setup(void);
void R_TranslatePlayerSkin(int playernum);
void GL_Init(void);
void GL_BeginRendering();
void GL_EndRendering(void);
void R_Clear(void);

// gl_particle.c
void GL_DrawParticles(void);
void GL_ParticleInit(void);

// gl_shader.c
GLint GL_GetAttribLocation(GLuint program, const char *name);
GLint GL_GetUniformLocation(GLuint program, const char *name);
GLuint LoadShader(const char *vertex_shader, int vertex_shader_len,
                  const char *fragment_shader, int fragment_shader_len);

// gl_sprite.c
void GL_DrawSpriteModel(entity_t *ent);

// gl_surface.c
void GL_DrawSurfaces(brush_model_t *brushmodel, texchain_t chain);
void GL_DrawBrushModel(entity_t *ent);
void GL_CreateBrushShaders(void);

// gl_vidsdl.c
void VID_Swap(void);
void VID_Init(void);
void VID_Shutdown(void);

// gl_warp.c
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa);

#endif /* __GLQUAKE_H */
