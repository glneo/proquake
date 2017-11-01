/*
 * Sky and water polygons
 *
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

#include "quakedef.h"
#include "glquake.h"

int skytexturenum;

gltexture_t *solidskytexture;
gltexture_t *alphaskytexture;
float speedscale;  // for top sky
float speedscale2; // and bottom sky

static msurface_t *warpface;

extern cvar_t gl_subdivide_size;

static void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i = 0; i < numverts; i++)
		for (j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

static unsigned RecursLevel;

static void SubdividePolygon(int numverts, float *verts)
{
	int i, j, k;
	vec3_t mins, maxs;
	float m;
	float *v;
	vec3_t front[64], back[64];
	int f, b;
	float dist[64];
	float frac;
	glpoly_t *poly;
	float s, t, subdivide;

	if (++RecursLevel > 128) // 16 seems enough and 512 might create stack overflow
		Sys_Error("excessive tree depth");

	if (numverts > 60)
		Sys_Error("excessive numverts %i", numverts);

	subdivide = gl_subdivide_size.value;

	if (subdivide < 32)
		subdivide = 32; // Avoid low subdivide values

	subdivide = max(1, gl_subdivide_size.value);
	BoundPoly(numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide * floor(m / subdivide + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy(verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy(v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon(f, front[0]);
		SubdividePolygon(b, back[0]);
		--RecursLevel;
		return;
	}

	poly = Hunk_Alloc(sizeof(glpoly_t) + (numverts - 4) * VERTEXSIZE * sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy(verts, poly->verts[i]);
		s = DotProduct(verts, warpface->texinfo->vecs[0]);
		t = DotProduct(verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}

	--RecursLevel;
}

/*
 ================
 GL_SubdivideSurface

 Breaks a polygon up along axial 64 unit
 boundaries so that turbulent and sky warps
 can be done reasonably.
 ================
 */
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa)
{
	vec3_t verts[64];
	int numverts;
	int i;
	int lindex;
	float *vec;

	warpface = fa;

	// convert edges back to a normal polygon
	numverts = 0;
	for (i = 0; i < fa->numedges; i++)
	{
		lindex = brushmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = brushmodel->vertexes[brushmodel->edges[lindex].v[0]].position;
		else
			vec = brushmodel->vertexes[brushmodel->edges[-lindex].v[1]].position;

		if (numverts >= 64)
			Sys_Error("excessive numverts %i", numverts);

		VectorCopy(vec, verts[numverts]);
		numverts++;
	}

	RecursLevel = 0;
	SubdividePolygon(numverts, verts[0]);
}

//=========================================================

// speed up sin calculations - Ed
float turbsin[] = {
	#include "gl_warp_sin.h"
};
#define	TURBSINSIZE	256
#define TURBSCALE ((float) TURBSINSIZE / (2 * M_PI))

/*
 =============
 EmitWaterPolys

 Does a water warp on the pre-fragmented glpoly_t chain
 =============
 */
void EmitWaterPolys(msurface_t *fa)
{
	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		struct {
			float s, t;
		} texcord[p->numverts];
		for (int i = 0; i < p->numverts; i++)
		{
			float os = p->verts[i][3];
			float ot = p->verts[i][4];

			texcord[i].s = os + turbsin[(int) ((ot * 0.125 + cl.time) * TURBSCALE) & 255];
			texcord[i].s *= (1.0 / 64);

			texcord[i].t = ot + turbsin[(int) ((os * 0.125 + cl.time) * TURBSCALE) & 255];
			texcord[i].t *= (1.0 / 64);

		}

		glTexCoordPointer(2, GL_FLOAT, 0, texcord);
		glVertexPointer(3, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][0]);
		glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
	}
}

void EmitSkyPolys(msurface_t *fa)
{
	speedscale = cl.time * 8;
	speedscale -= (int)speedscale & ~127;

	speedscale2 = cl.time * 16;
	speedscale2 -= (int)speedscale2 & ~127;

	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		struct {
			float s, t;
		} texcord[2][p->numverts];

		for (int i = 0; i < p->numverts; i++)
		{
			vec3_t dir;
			VectorSubtract(p->verts[i], r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			float length = VectorLength(dir);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			texcord[0][i].s = (speedscale + dir[0]) * (1.0 / 128);
			texcord[0][i].t = (speedscale + dir[1]) * (1.0 / 128);

			texcord[1][i].s = (speedscale2 + dir[0]) * (1.0 / 128);
			texcord[1][i].t = (speedscale2 + dir[1]) * (1.0 / 128);
		}

		glClientActiveTexture(GL_TEXTURE0);
		glTexCoordPointer(2, GL_FLOAT, 0, texcord[0]);
		glClientActiveTexture(GL_TEXTURE1);
		glTexCoordPointer(2, GL_FLOAT, 0, texcord[1]);

		glVertexPointer(3, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][0]);
		glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
	}
}

/*
 * Does a sky warp on the pre-fragmented glpoly_t chain
 * This will be called for brushmodels, the world
 * will have them chained together.
 */
void EmitBothSkyLayers(msurface_t *fa)
{
	GL_Bind(solidskytexture);
	GL_EnableMultitexture();
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	GL_Bind(alphaskytexture);

	EmitSkyPolys(fa);

	GL_DisableMultitexture();
}

void R_DrawSkyChain(msurface_t *fa)
{
	GL_Bind(solidskytexture);
	GL_EnableMultitexture();
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	GL_Bind(alphaskytexture);

	for (; fa; fa = fa->texturechain)
		EmitSkyPolys(fa);

	GL_DisableMultitexture();
}

//===============================================================

/*
 =============
 Sky_LoadTexture
 A sky texture is 256*128, with the left side being a masked overlay
 ==============
 */
void Sky_LoadTexture(texture_t *mt)
{
	static byte front_data[128 * 128];
	static byte back_data[128 * 128];
	char texturename[64];

	byte *src = (byte *)mt + mt->offsets[0];

	// extract back layer and upload
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
			back_data[(i * 128) + j] = src[i * 256 + j + 128];

	snprintf(texturename, sizeof(texturename), "%s_back", mt->name);
	solidskytexture = TexMgr_LoadImage(texturename, 128, 128, SRC_INDEXED, back_data, TEX_NOFLAGS);

	// extract front layer and upload
	for (int i = 0; i < 128; i++)
		for (int j = 0; j < 128; j++)
		{
			front_data[(i * 128) + j] = src[i * 256 + j];
			if (front_data[(i * 128) + j] == 0)
				front_data[(i * 128) + j] = 255;
		}

	snprintf(texturename, sizeof(texturename), "%s_front", mt->name);
	alphaskytexture = TexMgr_LoadImage(texturename, 128, 128, SRC_INDEXED, front_data, TEX_ALPHA);
}
