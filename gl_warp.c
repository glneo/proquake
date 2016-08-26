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

extern model_t *loadmodel;

int skytexturenum;

int solidskytexture;
int alphaskytexture;
float speedscale;		// for top sky and bottom sky

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
		Sys_Error("SubdividePolygon: excessive tree depth");

	if (numverts > 60)
		Sys_Error("SubdividePolygon: excessive numverts %i", numverts);

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
			Sys_Error("GL_SubdivideSurface: excessive numverts %i", numverts);

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
	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
		struct {
			float s, t;
		} texcord[p->numverts];
		for (int i = 0; i < p->numverts; i++)
		{
			vec3_t dir;
			VectorSubtract(p->verts[i], r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			float length = VectorLength(dir);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			texcord[i].s = (speedscale + dir[0]) * (1.0 / 128);
			texcord[i].t = (speedscale + dir[1]) * (1.0 / 128);
		}

		glTexCoordPointer(2, GL_FLOAT, 0, texcord);
		glVertexPointer(3, GL_FLOAT, VERTEXSIZE * sizeof(float), &p->verts[0][0]);
		glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
	}
}

/*
 ===============
 EmitBothSkyLayers

 Does a sky warp on the pre-fragmented glpoly_t chain
 This will be called for brushmodels, the world
 will have them chained together.
 ===============
 */
void EmitBothSkyLayers(msurface_t *fa)
{
	GL_DisableMultitexture();

	GL_Bind(solidskytexture);
	speedscale = cl.time * 8;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys(fa);

	glEnable(GL_BLEND);
	GL_Bind(alphaskytexture);
	speedscale = cl.time * 16;
	speedscale -= (int) speedscale & ~127;

	EmitSkyPolys(fa);

	glDisable(GL_BLEND);
}

void R_DrawSkyChain(msurface_t *s)
{
	msurface_t *fa;

	GL_DisableMultitexture();

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = cl.time * 8;
	speedscale -= (int) speedscale & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys(fa);

	glEnable(GL_BLEND);

	GL_Bind(alphaskytexture);
	speedscale = cl.time * 16;
	speedscale -= (int) speedscale & ~127;

	for (fa = s; fa; fa = fa->texturechain)
		EmitSkyPolys(fa);

	glDisable(GL_BLEND);

}

//===============================================================

/*
 =============
 R_InitSky

 A sky texture is 256*128, with the right side being a masked overlay
 ==============
 */
void R_InitSky(texture_t *mt, byte *src)
{
	int i, j, p, scaledx;
	byte fixedsky[256 * 128];
	unsigned trans[128 * 128];
	unsigned transpix;
	int r, g, b;
	unsigned *rgba;

	if (mt->width * mt->height != sizeof(fixedsky))
	{
		Con_DPrintf("\002R_InitSky: ");
		Con_DPrintf("non-standard sky texture '%s' (%dx%d, should be 256x128)\n", mt->name, mt->width, mt->height);

		// Resize sky texture to correct size
		memset(fixedsky, 0, sizeof(fixedsky));

		for (i = 0; i < 256; ++i)
		{
			scaledx = i * mt->width / 256 * mt->height;

			for (j = 0; j < 128; ++j)
				fixedsky[i * 128 + j] = src[scaledx + j * mt->height / 128];
		}

		src = fixedsky;
	}

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}

	((byte *) &transpix)[0] = r / (128 * 128);
	((byte *) &transpix)[1] = g / (128 * 128);
	((byte *) &transpix)[2] = b / (128 * 128);
	((byte *) &transpix)[3] = 0;

	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	GL_Bind(solidskytexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (i = 0; i < 128; i++)
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j];
			if (p == 0)
				trans[(i * 128) + j] = transpix;
			else
				trans[(i * 128) + j] = d_8to24table[p];
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

