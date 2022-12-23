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

#ifndef __MODEL_H
#define __MODEL_H

/*
 * d*_t structures are on-disk representations
 * m*_t structures are in-memory
 */

//#include "bspfile.h"
#define	MAX_MAP_HULLS 4
#define	MAXLIGHTMAPS 4
#define	MIPLEVELS 4
#define	MAX_MAP_LEAFS 8192
#define	CONTENTS_EMPTY -1
#define	CONTENTS_SOLID -2
#define	CONTENTS_WATER -3
#define	CONTENTS_SLIME -4
#define	CONTENTS_LAVA -5
#define	CONTENTS_SKY -6
#define	CONTENTS_ORIGIN -7
#define	CONTENTS_CLIP -8
#define	CONTENTS_CURRENT_0 -9
#define	CONTENTS_CURRENT_90 -10
#define	CONTENTS_CURRENT_180 -11
#define	CONTENTS_CURRENT_270 -12
#define	CONTENTS_CURRENT_UP -13
#define	CONTENTS_CURRENT_DOWN -14
#define	NUM_AMBIENTS 4
#define	AMBIENT_WATER 0
#define	AMBIENT_SKY 1
#define	AMBIENT_SLIME 2
#define	AMBIENT_LAVA 3
// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2
// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef unsigned int GLuint;

/*
 ==============================================================================

 BRUSH MODELS

 ==============================================================================
 */

#define LMBLOCK_WIDTH	256 // FIXME: make dynamic
#define LMBLOCK_HEIGHT	256

typedef struct glRect_s
{
	size_t x, y, w, h;
} glRect_t;

typedef struct lightmap_s
{
	gltexture_t *texture;
	std::vector<unsigned short> *indices;

	bool modified;
	glRect_t rectchange;

	// Height of current allocations at each width point
	size_t allocated[LMBLOCK_WIDTH];

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte data[LMBLOCK_HEIGHT][LMBLOCK_WIDTH];
} lightmap_t;

typedef struct {
	uint32_t fileofs;
	uint32_t filelen;
} mlump_t;

typedef struct {
	float mins[3], maxs[3];
	float origin[3];
	int32_t headnode[MAX_MAP_HULLS];
	int32_t visleafs; // not including the solid leaf 0
	int32_t firstface, numfaces;
} msubmodel_t;

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

typedef enum {
	chain_world = 0,
	chain_model = 1
} texchain_t;

typedef struct gltexture_s gltexture_t;
typedef struct mtexture_s
{
	char name[16];
	unsigned width, height;

	bool isskytexture;

	gltexture_t *gltexture;
	struct msurface_s *texturechains[2];    // for gl_texsort drawing

	int anim_total;                         // total tenths in sequence ( 0 = no)
	int anim_min, anim_max;                 // time for this frame min <=time< max
	struct mtexture_s *anim_next;            // in the animation sequence
	struct mtexture_s *alternate_anims;      // bmodels in frame 1 use these
//	unsigned offsets[MIPLEVELS];            // four mip maps stored
} mtexture_t;

#define	SURF_PLANEBACK          BIT(2)
#define	SURF_DRAWSKY            BIT(3)
//#define SURF_DRAWSPRITE         BIT(4)
#define SURF_DRAWTURB           BIT(5)
//#define SURF_DRAWTILED          BIT(6)
//#define SURF_DRAWBACKGROUND     BIT(7)
#define SURF_UNDERWATER         BIT(8)

typedef struct {
	unsigned short v[2];
} medge_t;

typedef struct {
	vec3_t vecs;
	float offset;
} mtexvec_t;

typedef struct {
	mtexvec_t vecs[2]; // [s/t]
	mtexture_t *texture;
	int flags;
} mtexinfo_t;

typedef struct pos_cord_s {
	float x, y, z;
	pos_cord_s operator+(const pos_cord_s& rhs)
	{
		pos_cord_s temp;
		temp.x = x + rhs.x;
		temp.y = y + rhs.y;
		temp.z = z + rhs.z;
		return temp;
	}
	pos_cord_s operator-(const pos_cord_s& rhs)
	{
		pos_cord_s temp;
		temp.x = x - rhs.x;
		temp.y = y - rhs.y;
		temp.z = z - rhs.z;
		return temp;
	}
	pos_cord_s operator*(const float& rhs)
	{
		pos_cord_s temp;
		temp.x = x * rhs;
		temp.y = y * rhs;
		temp.z = z * rhs;
		return temp;
	}
} pos_cord;

typedef struct {
	float s, t;
} tex_cord;

typedef struct msurface_s {
	int visframe; // should be drawn when node is crossed

	mplane_t *plane;
	int flags;

	int firstedge; // look up in model->surfedges[], negative numbers are backwards edges
	size_t numedges;

	short texturemins[2];
	short extents[2];

	float mins[3];
	float maxs[3];

	size_t light_s, light_t; // gl lightmap coordinates

	size_t numverts;
	std::vector<pos_cord> verts;
	std::vector<tex_cord> tex;
	tex_cord *light_tex;

	std::vector<unsigned short> *indices;
	GLuint indicesVBO;

	struct msurface_s *texturechain;

	mtexinfo_t *texinfo;

	// lighting info
	uint32_t dlightbits;
	lightmap_t *lightmap;
	byte styles[MAXLIGHTMAPS];
	int cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	bool cached_dlight; // true if dynamic light in cache
	byte *samples; // [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s {
	// common with leaf
	int contents; // 0, to differentiate from leafs
	int visframe; // node needs to be traversed if current

	vec3_t bboxmin; // for bounding box culling
	vec3_t bboxmax;

	// node specific
	mplane_t *plane;
	struct mnode_s *left_node;
	struct mnode_s *right_node;

	unsigned int firstsurface;
	unsigned int numsurfaces;
} mnode_t;

typedef struct mleaf_s {
	// common with node
	int contents; // will be a negative contents number
	int visframe; // node needs to be traversed if current

	vec3_t bboxmin; // for bounding box culling
	vec3_t bboxmax;

	// leaf specific
	byte *compressed_vis;
	efrag_t *efrags;

	size_t nummarksurfaces;
	msurface_t **firstmarksurface;

	int key; // BSP sequence number for leaf's contents
	byte ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct {
	int32_t planenum;
	int16_t children[2];	// negative numbers are contents
} mclipnode_t;

typedef struct {
	mclipnode_t *clipnodes;
	mplane_t *planes;
	int firstclipnode;
	int lastclipnode;
	vec3_t clip_mins;
	vec3_t clip_maxs;
	int available;
} hull_t;

typedef struct
{
	char *entities;

	size_t numplanes;
	mplane_t *planes;

	size_t numtextures;
	mtexture_t **textures;

	size_t numvertexes;
	mvertex_t *vertexes;

	size_t numvisdata;
	byte *visdata;

	size_t numtexinfo;
	mtexinfo_t *texinfo;

	byte *lightdata;

	size_t numsurfedges;
	int *surfedges;

	size_t numedges;
	medge_t *edges;

	size_t numsurfaces;
	msurface_t *surfaces;

	size_t nummarksurfaces;
	msurface_t **marksurfaces;

	size_t numleafs; // number of visible leafs, not counting 0
	mleaf_t *leafs;

	size_t numnodes;
	mnode_t *nodes;

	size_t numclipnodes;
	mclipnode_t *clipnodes;

	size_t numsubmodels;
	msubmodel_t *submodels;

	hull_t hulls[MAX_MAP_HULLS];

	GLuint vertsVBO;
	GLuint texVBO;
	GLuint light_texVBO;

	size_t lightmap_count;
	lightmap_t (*lightmaps)[10];
	size_t last_lightmap_allocated;

	int firstmodelsurface;
	int nummodelsurfaces;
} brush_model_t;

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
	struct gltexture_s *gltexture;
	float smax, tmax;
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

/*
 ==============================================================================

 ALIAS MODELS

 ==============================================================================
 */

typedef struct {
	vec3_t v;
	vec3_t normal;
} mtrivertx_t;

typedef struct {
	float interval;
	mtrivertx_t *poseverts;
	GLuint posevertsVBO;
} mpose_t;

typedef struct {
	size_t numposes;
	mpose_t *poses;
	size_t current_pose;
	double next_pose_time;
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
	maliasgroupframedesc_t *frames;
} maliasgroup_t;

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

	vec3_t eyeposition;

	float boundingradius;

	size_t numskins;
	size_t skinwidth;
	size_t skinheight;
	gltexture_t * (*gl_texturenum)[4];
	gltexture_t * (*gl_fbtexturenum)[4];

	size_t numverts;
	mstvert_t *texverts;
	GLuint texvertsVBO;

	size_t numtris;
	mtriangle_t *triangles;
	GLuint trianglesVBO;

	size_t numframes;
	maliasframedesc_t *frames;
} alias_model_t;

//===================================================================

// Whole model

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias
} modtype_t;

#define	EF_ROCKET       BIT(0)  // leave a trail
#define	EF_GRENADE      BIT(1)  // leave a trail
#define	EF_GIB          BIT(2)  // leave a trail
#define	EF_ROTATE       BIT(3)  // rotate (bonus items)
#define	EF_TRACER       BIT(4)  // green split trail
#define	EF_ZOMGIB       BIT(5)  // small blood trail
#define	EF_TRACER2      BIT(6)  // orange split trail + rotate
#define	EF_TRACER3      BIT(7)  // purple trail

#define	MOD_NOLERP      BIT(8)  // don't lerp when animating
#define	MOD_NOSHADOW    BIT(9)  // don't cast a shadow
#define	MOD_FBRIGHT     BIT(10) // when fullbrights are disabled render this model brighter
#define MOD_PLAYER      BIT(11)

// must match definition in modelgen.h and spritegen.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum {
	ST_SYNC = 0,
	ST_RAND
} synctype_t;
#endif

typedef struct model_s
{
	char name[MAX_QPATH];
	bool needload;

	int numframes;
	synctype_t synctype;

	uint32_t flags;

	// volume occupied by the model graphics
	vec3_t mins, maxs;
	float radius;

	// solid volume for clipping
	bool clipbox;
	vec3_t clipmins, clipmaxs;

	modtype_t type;
	union {
		alias_model_t *aliasmodel;
		sprite_model_t *spritemodel;
		brush_model_t *brushmodel;
	};
} model_t;

//============================================================================

void Mod_Init(void);
void Mod_ClearAll(void);
model_t *Mod_ForName(const char *name);
void Mod_TouchModel(const char *name);
model_t *Mod_FindName(const char *name);
mleaf_t *Mod_PointInLeaf(vec3_t p, brush_model_t *model);
byte *Mod_LeafPVS(mleaf_t *leaf, brush_model_t *model);
byte *Mod_NoVisPVS(brush_model_t *model);
byte *Mod_FatPVS(vec3_t org, brush_model_t *brushmodel);

bool Mod_CheckFullbrights (byte *pixels, int count);

#endif	/* __MODEL_H */
