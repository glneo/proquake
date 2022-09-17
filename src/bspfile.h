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

#ifndef __BSPFILE_H
#define __BSPFILE_H

// upper design bounds
#define	MAX_MAP_HULLS           4

#define	MAX_MAP_MODELS          256
#define	MAX_MAP_BRUSHES         4096
#define	MAX_MAP_ENTITIES        1024
#define	MAX_MAP_ENTSTRING       65536

#define	MAX_MAP_PLANES          32767
#define	MAX_MAP_NODES           32767 // because negative shorts are contents
#define	MAX_MAP_CLIPNODES       32767
#define	MAX_MAP_LEAFS           8192
#define	MAX_MAP_VERTS           65535
#define	MAX_MAP_FACES           65535
#define	MAX_MAP_MARKSURFACES    65535
#define	MAX_MAP_TEXINFO         4096
#define	MAX_MAP_EDGES           256000
#define	MAX_MAP_SURFEDGES       512000
#define	MAX_MAP_MIPTEX          0x200000
#define	MAX_MAP_LIGHTING        0x100000
#define	MAX_MAP_VISIBILITY      0x100000

// key / value pair sizes
#define	MAX_KEY         32
#define	MAX_VALUE       1024

//=============================================================================

#define BSPVERSION      29

typedef struct {
	float mins[3], maxs[3];
	float origin[3];
	int32_t headnode[MAX_MAP_HULLS];
	int32_t visleafs; // not including the solid leaf 0
	int32_t firstface, numfaces;
} dmodel_t;

enum lump_types {
	LUMP_ENTITIES = 0,
	LUMP_PLANES,
	LUMP_TEXTURES,
	LUMP_VERTEXES,
	LUMP_VISIBILITY,
	LUMP_NODES,
	LUMP_TEXINFO,
	LUMP_FACES,
	LUMP_LIGHTING,
	LUMP_CLIPNODES,
	LUMP_LEAFS,
	LUMP_MARKSURFACES,
	LUMP_EDGES,
	LUMP_SURFEDGES,
	LUMP_MODELS,

	LUMP_MAX,
};

typedef struct {
	uint32_t fileofs, filelen;
} dlump_t;

typedef struct {
	int32_t version;
	dlump_t lumps[LUMP_MAX];
} dheader_t;

typedef struct {
	int32_t nummiptex;
	int32_t dataofs[4]; // [nummiptex]
} dmiptexlump_t;

#define	MIPLEVELS	4
typedef struct miptex_s {
	char name[16];
	uint32_t width, height;
	uint32_t offsets[MIPLEVELS]; // four mip maps stored
} dmiptex_t;

typedef struct {
	float point[3];
} dvertex_t;

// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

typedef struct {
	float normal[3];
	float dist;
	int32_t type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

#define	CONTENTS_EMPTY		-1
#define	CONTENTS_SOLID		-2
#define	CONTENTS_WATER		-3
#define	CONTENTS_SLIME		-4
#define	CONTENTS_LAVA		-5
#define	CONTENTS_SKY		-6
#define	CONTENTS_ORIGIN		-7		// removed at csg time
#define	CONTENTS_CLIP		-8		// changed to contents_solid

#define	CONTENTS_CURRENT_0		-9
#define	CONTENTS_CURRENT_90		-10
#define	CONTENTS_CURRENT_180	-11
#define	CONTENTS_CURRENT_270	-12
#define	CONTENTS_CURRENT_UP		-13
#define	CONTENTS_CURRENT_DOWN	-14

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct {
	int32_t planenum;
	int16_t children[2];	// negative numbers are -(leafs+1), not nodes
	int16_t mins[3];		// for sphere culling
	int16_t maxs[3];
	uint16_t firstface;
	uint16_t numfaces;	// counting both sides
} dnode_t;

typedef struct {
	int32_t planenum;
	int16_t children[2];	// negative numbers are contents
} dclipnode_t;

typedef struct {
	float vecs[2][4];		// [s/t][xyz offset]
	int32_t miptex;
	int32_t flags;
} dtexinfo_t;
#define	TEX_SPECIAL		1		// sky or slime, no lightmap or 256 subdivision

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct {
	uint16_t v[2];		// vertex numbers
} dedge_t;

#define	MAXLIGHTMAPS	4
typedef struct {
	int16_t planenum;
	int16_t side;

	int32_t firstedge;		// we must support > 64k edges
	int16_t numedges;
	int16_t texinfo;

// lighting info
	byte styles[MAXLIGHTMAPS];
	int32_t lightofs;		// start of [numstyles*surfsize] samples
} dface_t;

#define	AMBIENT_WATER	0
#define	AMBIENT_SKY		1
#define	AMBIENT_SLIME	2
#define	AMBIENT_LAVA	3

#define	NUM_AMBIENTS    4 // automatic ambient sounds

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct {
	int32_t contents;
	int32_t visofs; // -1 = no visibility info

	int16_t mins[3]; // for frustum culling
	int16_t maxs[3];

	uint16_t firstmarksurface;
	uint16_t nummarksurfaces;

	byte ambient_level[NUM_AMBIENTS];
} dleaf_t;

#endif	/* __BSPFILE_H */
