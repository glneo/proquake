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

#define TEX_NOFLAGS     0 // Baker: I use this to mark the absense of any flags
#define TEX_MIPMAP      2
#define TEX_ALPHA       4
#define	TEX_WORLD       128//R00k

#define	MAX_GLTEXTURES 1024

typedef struct
{
	unsigned int texnum;
	char identifier[MAX_QPATH];
	int width, height;
//	bool mipmap;
	unsigned short crc;  // Baker 3.80x - part of GL_LoadTexture: cache mismatch fix
	int texmode;	// Baker: 4.26 to all clearing of world textures
} gltexture_t;

// Engine internal vars
extern bool gl_mtexable;

extern gltexture_t gltextures[MAX_GLTEXTURES];
extern int numgltextures;

extern int texture_extension_number;

extern texture_t *r_notexture_mip;
extern int d_lightstylevalue[256]; // 8.8 fraction of base light value

extern bool envmap;

extern int current_texture_num;
extern int particletexture;
extern int playertextures;

extern int skytexturenum; // index in cl.loadmodel, not gl texture object

extern int mirrortexturenum; // quake texturenum, not gltexturenum
extern bool mirror;
extern mplane_t *mirror_plane;

extern int texture_mode;
extern int gl_lightmap_format;

typedef struct
{
	float x, y, z;
	float s, t;
	float r, g, b;
} glvert_t;

extern glvert_t glv;

extern int glx, gly, glwidth, glheight;

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0) //normalizing factor so player model works out to about 1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01

typedef enum
{
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t org;
	float color;
// drivers never touch the following fields
	struct particle_s *next;
	vec3_t vel;
	float ramp;
	float die;
	ptype_t type;
} particle_t;

//====================================================

extern int r_framecount;
extern int c_brush_polys, c_alias_polys;

// view origin
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

// screen size info
extern refdef_t r_refdef;
extern mleaf_t *r_viewleaf, *r_oldviewleaf;

extern int gl_solid_format;
extern int gl_alpha_format;

// rendering cvar stuffs
extern cvar_t r_norefresh;
extern cvar_t r_drawentities;
extern cvar_t r_drawworld;
extern cvar_t r_drawviewmodel; // client cvar
extern cvar_t r_speeds;
extern cvar_t r_waterwarp;
extern cvar_t r_fullbright;
extern cvar_t r_lightmap;
extern cvar_t r_shadows;
extern cvar_t r_mirroralpha;
extern cvar_t r_wateralpha;
extern cvar_t r_dynamic;
extern cvar_t r_novis;
extern cvar_t r_farclip;

// fenix@io.com: model interpolation
extern cvar_t r_interpolate_animation;
extern cvar_t r_interpolate_transform;
extern cvar_t r_interpolate_weapon;

// gl rendering cvars and stuff
extern cvar_t gl_clear;
extern cvar_t gl_cull;
extern cvar_t gl_poly;
extern cvar_t gl_smoothmodels;
extern cvar_t gl_affinemodels;
extern cvar_t gl_polyblend;

extern cvar_t gl_flashblend;
extern cvar_t gl_nocolors;
//extern	cvar_t	gl_doubleeyes;
extern cvar_t gl_playermip;
extern cvar_t gl_fullbright;
extern cvar_t r_ringalpha;

extern float r_world_matrix[16];

extern qpic_t *draw_disc;

// gl_alias.c
void R_DrawAliasModel(entity_t *ent);

// gl_draw.c
qpic_t *Draw_PicFromWad(char *name);
qpic_t *Draw_CachePic(char *path);
void Draw_Character(int x, int y, int num);
void Draw_String(int x, int y, char *str);
void Draw_Crosshair(void);
void Draw_AlphaPic(int x, int y, qpic_t *pic, float alpha);
void Draw_Pic(int x, int y, qpic_t *pic);
void Draw_TransPic(int x, int y, qpic_t *pic);
void Draw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation);
void Draw_ConsoleBackground(int lines);
void Draw_TileClear(int x, int y, int w, int h);
void Draw_AlphaFill(int x, int y, int w, int h, int c, float alpha);
void Draw_Fill(int x, int y, int w, int h, int c);
void Draw_FadeScreen(void);
void GL_Set2D(void);
void Draw_Init(void);

// gl_fullbright.c
int FindFullbrightTexture (byte *pixels, int num_pix);
void ConvertPixels (byte *pixels, int num_pixels);
void DrawFullBrightTextures(entity_t *ent);

// gl_light.c
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
void R_RotateForEntity(entity_t *ent);
bool R_CullBox(vec3_t mins, vec3_t maxs);
bool R_CullForEntity(const entity_t *ent);
void R_TranslatePlayerSkin(int playernum);
void R_NewMap(void);
void R_RenderView(void);
void GL_Init(void);
void GL_BeginRendering(int *x, int *y, int *width, int *height);
void GL_EndRendering(void);
void R_Init(void);

// gl_particle.c
void R_DrawParticles(void);

// gl_sprite.c
void R_DrawSpriteModel(entity_t *ent);

// gl_surface.c
void DrawGLPoly(glpoly_t *p, int tex_offset);
texture_t *R_TextureAnimation(int frame, texture_t *base);
void R_DrawWaterSurfaces(void);
void DrawGLWaterPoly(glpoly_t *p, int tex_offset);
void R_RenderBrushPoly(msurface_t *fa);
void R_DrawBrushModel(entity_t *ent);
void R_DrawWorld(void);
void R_MarkLeaves(void);

// gl_texture.c
void GL_Upload8(byte *data, int width, int height, int mode);
int GL_FindTexture(char *identifier);
void GL_FreeTextures(void);
int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode);
void GL_Bind(int texnum);
void GL_EnableMultitexture(void);
void GL_DisableMultitexture(void);
void R_InitTextures(void);
void R_InitParticleTexture(void);

// gl_vidsdl.c/*
void VID_Swap(void);
void VID_Init(unsigned char *palette);
void VID_Shutdown(void);

// gl_warp.c
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa);
void EmitWaterPolys(msurface_t *fa);
void EmitSkyPolys(msurface_t *fa);
void EmitBothSkyLayers(msurface_t *fa);
void R_DrawSkyChain(msurface_t *fa);
void R_InitSky(texture_t *mt, byte *src);

#endif /* __GLQUAKE_H */
