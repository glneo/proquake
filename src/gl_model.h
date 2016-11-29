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

#ifndef __GL_MODEL_H
#define __GL_MODEL_H

#include "modelgen.h"
#include "spritegen.h"

/*
 * d*_t structures are on-disk representations
 * m*_t structures are in-memory
 */

typedef struct model_s model_t;

/*
 ==============================================================================

 BRUSH MODELS

 ==============================================================================
 */

typedef struct {
	vec3_t position;
} mvertex_t;

#define	SIDE_FRONT      0
#define	SIDE_BACK       1
#define	SIDE_ON         2

typedef struct mplane_s
{
	vec3_t normal;
	float dist;
	byte type; // for texture axis selection and fast side tests
	byte signbits; // signx + signy<<1 + signz<<1
	byte pad[2];
} mplane_t;

typedef struct texture_s
{
	char name[16];
	unsigned width, height;
	int gl_texturenum;
	struct msurface_s *texturechain;        // for gl_texsort drawing
	int anim_total;                         // total tenths in sequence ( 0 = no)
	int anim_min, anim_max;                 // time for this frame min <=time< max
	struct texture_s *anim_next;            // in the animation sequence
	struct texture_s *alternate_anims;      // bmodels in frame 1 use these
	unsigned offsets[MIPLEVELS];            // four mip maps stored
	int fullbright;
} texture_t;

#define	SURF_PLANEBACK          2
#define	SURF_DRAWSKY            4
#define SURF_DRAWSPRITE         8
#define SURF_DRAWTURB           0x10
#define SURF_DRAWTILED          0x20
#define SURF_DRAWBACKGROUND     0x40
#define SURF_UNDERWATER         0x80

typedef struct {
	unsigned short v[2];
	unsigned int cachededgeoffset;
} medge_t;

typedef struct {
	float vecs[2][4];
	float mipadjust;
	texture_t *texture;
	int flags;
} mtexinfo_t;

#define	VERTEXSIZE	7

typedef struct glpoly_s {
	struct glpoly_s *next;
	struct glpoly_s *chain;
	int numverts;
	int flags; // for SURF_UNDERWATER
	float verts[4][VERTEXSIZE]; // variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s {
	int visframe; // should be drawn when node is crossed

	mplane_t *plane;
	int flags;

	int firstedge; // look up in model->surfedges[], negative numbers
	int numedges; // are backwards edges

	short texturemins[2];
	short extents[2];

	int light_s, light_t; // gl lightmap coordinates

	glpoly_t *polys; // multiple if warped
	struct msurface_s *texturechain;

	mtexinfo_t *texinfo;

	// lighting info
	int dlightframe;
	int dlightbits;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	int cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	bool cached_dlight; // true if dynamic light in cache
	byte *samples; // [numstyles*surfsize]

	int draw_this_frame;

	bool overbright;
} msurface_t;

typedef struct mnode_s {
	// common with leaf
	int contents; // 0, to differentiate from leafs
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	struct mnode_s *parent;

	// node specific
	mplane_t *plane;
	struct mnode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mnode_t;

typedef struct mleaf_s {
	// common with node
	int contents; // wil be a negative contents number
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	struct mnode_s *parent;

	// leaf specific
	byte *compressed_vis;
	efrag_t *efrags;

	msurface_t **firstmarksurface;
	int nummarksurfaces;
	int key; // BSP sequence number for leaf's contents
	byte ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct {
	dclipnode_t *clipnodes;
	mplane_t *planes;
	int firstclipnode;
	int lastclipnode;
	vec3_t clip_mins;
	vec3_t clip_maxs;
	int available;
} hull_t;

/*
 ==============================================================================

 SPRITE MODELS

 ==============================================================================
 */
typedef struct
{
	int width;
	int height;
	float up, down, left, right;
	int gl_texturenum;
	float interval;
} mspriteframe_t;

typedef struct
{
	int numposes;
	mspriteframe_t *poses;
} mspriteframedesc_t;

typedef struct
{
	int type;
	int maxwidth;
	int maxheight;
	int beamlength;
	int numframes;
	mspriteframedesc_t *framedescs;
} sprite_model_t;

void R_DrawSpriteModel(entity_t *ent);
void Mod_LoadSpriteModel(model_t *mod, void *buffer);

/*
 ==============================================================================

 ALIAS MODELS

 ==============================================================================
 */
typedef struct {
	int firstpose;
	int numposes;
	float interval;
	vec3_t bboxmin;
	vec3_t bboxmax;
	char name[16];
} maliasframedesc_t;

typedef struct {
	vec3_t bboxmin;
	vec3_t bboxmax;
	int frame;
} maliasgroupframedesc_t;

typedef struct {
	int numframes;
	int intervals;
	maliasgroupframedesc_t frames[1];
} maliasgroup_t;

typedef struct {
	vec3_t v;
	vec3_t normal;
} mtrivertx_t;

typedef struct {
	float s;
	float t;
} mstvert_t;

typedef struct mtriangle_s {
	short vertindex[3];
} mtriangle_t;

/* some sane limits, these can be changed or removed if needed */
#define	MAX_SKINS 32
#define	MAX_SKIN_HEIGHT 1024
#define	MAX_SKIN_WIDTH 1024
#define	MAXALIASVERTS 4096
#define	MAXALIASFRAMES 256
#define	MAXALIASTRIS 4096

typedef struct
{
	vec3_t scale;
	vec3_t scale_origin;
	float boundingradius;
	vec3_t eyeposition;

	int numskins;
	int skinwidth;
	int skinheight;
	int (*gl_texturenum)[4];

	int numverts;
	mstvert_t *stverts[2];

	int numtris;
	mtriangle_t *triangles;
	int backstart;

	int numframes;
	maliasframedesc_t *frames;

	int numposes;
	mtrivertx_t *poseverts[1000];

	synctype_t synctype;
	int flags;
} alias_model_t;

//===================================================================

// Whole model

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias
} modtype_t;

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

#define NOCOLORMAP	256
#define MOD_PLAYER	512

typedef struct
{
	int bspversion;
	bool isworldmodel;

	char *entities;

	int numplanes;
	mplane_t *planes;

	int numtextures;
	texture_t **textures;

	int numvertexes;
	mvertex_t *vertexes;

	byte *visdata;

	int numnodes;
	mnode_t *nodes;

	int numtexinfo;
	mtexinfo_t *texinfo;

	int numsurfaces;
	msurface_t *surfaces;

	byte *lightdata;

	int numclipnodes;
	dclipnode_t *clipnodes;

	int numleafs; // number of visible leafs, not counting 0
	mleaf_t *leafs;

	int nummarksurfaces;
	msurface_t **marksurfaces;

	int numedges;
	medge_t *edges;

	int numsurfedges;
	int *surfedges;

	int numsubmodels;
	dmodel_t *submodels;

	hull_t hulls[MAX_MAP_HULLS];

	int firstmodelsurface;
	int nummodelsurfaces;
} brush_model_t;

typedef struct model_s
{
	char name[MAX_QPATH];
	bool needload; // bmodels and sprites don't cache normally

	modtype_t type;
	int numframes;
	synctype_t synctype;

	int flags;

	// volume occupied by the model graphics
	vec3_t mins, maxs;
	float radius;

	// solid volume for clipping
	bool clipbox;
	vec3_t clipmins, clipmaxs;

	union {
		alias_model_t *aliasmodel;
		sprite_model_t *spritemodel;
		brush_model_t *brushmodel;
	};
} model_t;

//============================================================================

void Mod_Init(void);
void Mod_ClearAll(void);
model_t *Mod_ForName(char *name, bool crash);
void Mod_TouchModel(char *name);
model_t *Mod_FindName(char *name);
mleaf_t *Mod_PointInLeaf(vec3_t p, brush_model_t *model);
byte *Mod_LeafPVS(mleaf_t *leaf, brush_model_t *model);

#endif	/* __GL_MODEL_H */
