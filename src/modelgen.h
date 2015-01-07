/*
 * Header file for model generation program
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

#define ALIAS_VERSION   6

#define ALIAS_ONSEAM    0x0020

// must match definition in spritegen.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum {
	ST_SYNC = 0,
	ST_RAND
} synctype_t;
#endif

typedef enum {
	ALIAS_SINGLE = 0,
	ALIAS_GROUP
} aliasframetype_t;

typedef enum {
	ALIAS_SKIN_SINGLE = 0,
	ALIAS_SKIN_GROUP
} aliasskintype_t;

typedef struct {
	int32_t ident;
	int32_t version;
	vec3_t scale;
	vec3_t scale_origin;
	float boundingradius;
	vec3_t eyeposition;
	int32_t numskins;
	int32_t skinwidth;
	int32_t skinheight;
	int32_t numverts;
	int32_t numtris;
	int32_t numframes;
	synctype_t synctype;
	int32_t flags;
	float size;
} mdl_t;

typedef struct {
	int32_t onseam;
	int32_t s;
	int32_t t;
} dstvert_t;

typedef struct dtriangle_s {
	int32_t facesfront;
	int32_t vertindex[3];
} dtriangle_t;

#define DT_FACES_FRONT 0x0010

typedef struct {
	byte v[3];
	byte lightnormalindex;
} dtrivertx_t;

typedef struct {
	dtrivertx_t bboxmin; // lightnormal isn't used
	dtrivertx_t bboxmax; // lightnormal isn't used
	char name[16]; // frame name from grabbing
} daliasframe_t;

typedef struct {
	int32_t numframes;
	dtrivertx_t bboxmin; // lightnormal isn't used
	dtrivertx_t bboxmax; // lightnormal isn't used
} daliasgroup_t;

typedef struct {
	int32_t numskins;
} daliasskingroup_t;

typedef struct {
	float interval;
} daliasinterval_t;

typedef struct {
	float interval;
} daliasskininterval_t;

typedef struct {
	aliasframetype_t type;
} daliasframetype_t;

typedef struct {
	aliasskintype_t type;
} daliasskintype_t;

#define IDPOLYHEADER (('O'<<24)+('P'<<16)+('D'<<8)+'I') // little-endian "IDPO"
