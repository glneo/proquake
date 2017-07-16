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

#ifndef __MATHLIB_H
#define __MATHLIB_H

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef	int fixed4_t;
typedef	int fixed8_t;
typedef	int fixed16_t;

#define DEG2RAD(a) ((a * M_PI) / 180.0f)

typedef struct mplane_s mplane_t;
extern vec3_t vec3_origin;

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
void CrossProduct(vec3_t v1, vec3_t v2, vec3_t cross);

#define VectorAdd(a,b,out) {out[0]=a[0]+b[0];out[1]=a[1]+b[1];out[2]=a[2]+b[2];}
#define VectorSubtract(a,b,out) {out[0]=a[0]-b[0];out[1]=a[1]-b[1];out[2]=a[2]-b[2];}
#define VectorCopy(a,out) {out[0]=a[0];out[1]=a[1];out[2]=a[2];}
#define VectorClear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, out) ((out)[0] = -(a)[0], (out)[1] = -(a)[1], (out)[2] = -(a)[2])
vec_t VectorLength(vec3_t v);
float VectorNormalize(vec3_t v);
void VectorInverse(vec3_t v);
void VectorScale(vec3_t in, vec_t scale, vec3_t out);
void LerpVector(const vec3_t from, const vec3_t to, float frac, vec3_t out);
void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int VectorCompare(vec3_t v1, vec3_t v2);
void VectorMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, mplane_t *p);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p) \
	(((p)->type < 3)? \
	( \
		((p)->dist <= (emins)[(p)->type])? \
			1 \
		: \
		( \
			((p)->dist >= (emaxs)[(p)->type])? \
				2 \
			: \
				3 \
		) \
	) \
	: \
		BoxOnPlaneSide( (emins), (emaxs), (p)))

float anglemod(float a);
float RadiusFromBounds(vec3_t mins, vec3_t maxs);
int ParseFloats(char *s, float *f, int *f_size);

#endif /* __MATHLIB_H */
