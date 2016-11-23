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
	int ident;
	int version;
	vec3_t scale;
	vec3_t scale_origin;
	float boundingradius;
	vec3_t eyeposition;
	int numskins;
	int skinwidth;
	int skinheight;
	int numverts;
	int numtris;
	int numframes;
	synctype_t synctype;
	int flags;
	float size;
} mdl_t;

typedef struct {
	int onseam;
	int s;
	int t;
} dstvert_t;

typedef struct dtriangle_s {
	int facesfront;
	int vertindex[3];
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
	int numframes;
	dtrivertx_t bboxmin; // lightnormal isn't used
	dtrivertx_t bboxmax; // lightnormal isn't used
} daliasgroup_t;

typedef struct {
	int numskins;
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
