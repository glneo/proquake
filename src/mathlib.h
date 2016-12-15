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

struct mplane_s;
extern vec3_t vec3_origin;

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,out) {out[0]=a[0]-b[0];out[1]=a[1]-b[1];out[2]=a[2]-b[2];}
#define VectorAdd(a,b,out) {out[0]=a[0]+b[0];out[1]=a[1]+b[1];out[2]=a[2]+b[2];}
#define VectorCopy(a,out) {out[0]=a[0];out[1]=a[1];out[2]=a[2];}
#define VectorClear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, out) ((out)[0] = -(a)[0], (out)[1] = -(a)[1], (out)[2] = -(a)[2])

void VectorMA(vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

int VectorCompare(vec3_t v1, vec3_t v2);
vec_t VectorLength(vec3_t v);
float RadiusFromBounds(vec3_t mins, vec3_t maxs);
void LerpVector(const vec3_t from, const vec3_t to, float frac, vec3_t out);

void CrossProduct(vec3_t v1, vec3_t v2, vec3_t cross);

void VectorInverse(vec3_t v);
void VectorScale(vec3_t in, vec_t scale, vec3_t out);

int ParseFloats(char *s, float *f, int *f_size);

float anglemod(float a);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

float VectorNormalize(vec3_t v);		// returns vector length

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod(double numer, double denom, int *quotient, int *rem);
fixed16_t Invert24To16(fixed16_t val);
int GreatestCommonDivisor(int i1, int i2);

void AngleVectors(vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

#endif /* __MATHLIB_H */