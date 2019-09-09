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

extern cvar_t gl_subdivide_size;

static void BoundPoly(int numverts, vec3_t *verts, vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	for (int i = 0; i < numverts; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			if (verts[i][j] < mins[j])
				mins[j] = verts[i][j];
			if (verts[i][j] > maxs[j])
				maxs[j] = verts[i][j];
		}
	}
}

static void SubdividePolygon(msurface_t *warpface, int numverts, vec3_t verts[])
{
	vec3_t mins, maxs;
	vec3_t front[64], back[64];

	if (numverts > 60)
		Sys_Error("excessive numverts %i", numverts);

	float subdivide = gl_subdivide_size.value;

	BoundPoly(numverts, verts, mins, maxs);

	for (int i = 0; i < 3; i++)
	{
		float m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide * floor(m / subdivide + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		float dist[64 * 3];
		for (int j = 0; j < numverts; j++)
			dist[j] = verts[j][i] - m;

		// wrap cases
		dist[numverts] = dist[0];
		VectorCopy(verts[0], verts[numverts]);

		int f = 0;
		int b = 0;
		for (int j = 0; j < numverts; j++)
		{
			if (dist[j] >= 0)
			{
				VectorCopy(verts[j], front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy(verts[j], back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				// clip point
				float frac = dist[j] / (dist[j] - dist[j + 1]);
				for (int k = 0; k < 3; k++)
				{
					front[f][k] = verts[j][k] + frac * (verts[j + 1][k] - verts[j][k]);
					back[b][k]  = verts[j][k] + frac * (verts[j + 1][k] - verts[j][k]);
				}
				f++;
				b++;
			}
		}

		SubdividePolygon(warpface, f, front);
		SubdividePolygon(warpface, b, back);
		return;
	}

	glpoly_t *poly = (glpoly_t *)Q_malloc(sizeof(glpoly_t));
	poly->verts = (vec3_t *)Q_malloc(sizeof(*poly->verts) * numverts);
	poly->tex = (tex_cord *)Q_malloc(sizeof(*poly->tex) * numverts);
	poly->light_tex = (tex_cord *)Q_malloc(sizeof(*poly->light_tex) * numverts);
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (int i = 0; i < numverts; i++)
	{
		VectorCopy(verts[i], poly->verts[i]);
		float s = DotProduct(verts[i], warpface->texinfo->vecs[0]);
		float t = DotProduct(verts[i], warpface->texinfo->vecs[1]);
		poly->tex[i].s = s;
		poly->tex[i].t = t;
	}
}

/*
 * Breaks a polygon up along axial 64 unit
 * boundaries so that turbulent and sky warps
 * can be done reasonably.
 */
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa)
{
	vec3_t verts[64];
	int i;

	// convert edges back to a normal polygon
	for (i = 0; i < fa->numedges; i++)
	{
		vec_t *vec;
		int lindex = brushmodel->surfedges[fa->firstedge + i];
		if (lindex > 0)
			vec = brushmodel->vertexes[brushmodel->edges[lindex].v[0]].position;
		else
			vec = brushmodel->vertexes[brushmodel->edges[-lindex].v[1]].position;

		if (i >= 64)
			Sys_Error("excessive numedges %i", i);

		VectorCopy(vec, verts[i]);
	}

	SubdividePolygon(fa, i, verts);
}

//=========================================================

// speed up sin calculations - Ed
float turbsin[] = {
	#include "gl_warp_sin.h"
};
#define	TURBSINSIZE 256
#define TURBSCALE ((float)TURBSINSIZE / (2 * M_PI))

/* Does a water warp on the pre-fragmented glpoly_t chain */
void EmitWaterPolys(msurface_t *fa)
{
	for (glpoly_t *p = fa->polys; p; p = p->next)
	{
//		tex_cord texcord[p->numverts];
		tex_cord texcord[100];
		for (int i = 0; i < p->numverts; i++)
		{
			float os = p->tex[i].s;
			float ot = p->tex[i].t;

			texcord[i].s = os + turbsin[(int) ((ot * 0.125 + cl.time) * TURBSCALE) & 255];
			texcord[i].s *= (1.0 / 64);

			texcord[i].t = ot + turbsin[(int) ((os * 0.125 + cl.time) * TURBSCALE) & 255];
			texcord[i].t *= (1.0 / 64);
		}

		glTexCoordPointer(2, GL_FLOAT, 0, texcord);
		glVertexPointer(3, GL_FLOAT, 0, &p->verts[0][0]);
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
//		tex_cord texcord[2][p->numverts];
		tex_cord texcord[2][100];

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

		glVertexPointer(3, GL_FLOAT, 0, &p->verts[0][0]);
		glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
	}
}

void R_DrawSkyChain(msurface_t *fa)
{
	GL_Bind(solidskytexture);

	GL_EnableMultitexture();

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	GL_Bind(alphaskytexture);

	for (; fa; fa = fa->texturechain)
		EmitSkyPolys(fa);

	GL_DisableMultitexture();
}

//===============================================================

/* A sky texture is 256*128, with the left side being a masked overlay */
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
