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

#include <stack>
#include <cfloat>

#include "quakedef.h"
#include "glquake.h"

typedef struct vertex_s {
	vec3_t vert;
	vertex_s(vec3_t in_vert)
	{
		vert[0] = in_vert[0];
		vert[1] = in_vert[1];
		vert[2] = in_vert[2];
	}
	float& operator[](int index)
	{
	    return vert[index];
	}
} vertex_t;

static bool FindSubdevidePlane(std::vector<vertex_t> verts, size_t *axis, float *location)
{
	constexpr float subdivide = 64;

	for (size_t j = 0; j < 3; j++)
	{
		// Find bounds for this plane
		float min = FLT_MAX;
		float max = -FLT_MAX;
		for (vertex_t &vert : verts)
		{
			if (vert[j] < min)
				min = vert[j];
			if (vert[j] > max)
				max = vert[j];
		}

		// Find middle plane
		float m = (min + max) * 0.5;

		// Snap to next subdivide multiple
		m = subdivide * floor(m / subdivide + 0.5);

		// Too close to an edge
		if ((max - m < 8) || (m - min < 8))
			continue;

		// Otherwise we found our subdivide plane
		*axis = j;
		*location = m;
		return true;
	}

	return false;
}

/*
 * Breaks a polygon up along axial 64 unit
 * boundaries so that turbulent and sky warps
 * can be done reasonably.
 */
void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa)
{
	// convert verts to vector
	std::vector<vertex_t> vertsx;
	for (size_t i = 0; i < fa->numedges; i++)
		vertsx.push_back(fa->verts[i]);

	std::stack<std::vector<vertex_t>> poly_stack;
	// Start stack with top level polygon
	poly_stack.push(vertsx);

	while (!poly_stack.empty())
	{
		std::vector<vertex_t> verts = poly_stack.top();
		poly_stack.pop();

		size_t numverts = verts.size();
		if (numverts > 60)
			Sys_Error("excessive numverts %zu", numverts);

		size_t plane;
		float location;
		bool found = FindSubdevidePlane(verts, &plane, &location);

		// No more cuts to make
		if (!found)
		{
			// Add our new verts
			size_t new_total = fa->numverts + numverts;
			fa->verts = (vec3_t   *)Q_realloc(fa->verts, sizeof(*fa->verts) * new_total);
			fa->tex   = (tex_cord *)Q_realloc(fa->tex,   sizeof(*fa->tex)   * new_total);
			for (size_t i = 0; i < numverts; i++)
			{
				size_t warpindex = fa->numverts + i;
				VectorCopy(verts[i], fa->verts[warpindex]);
				fa->tex[warpindex].s = DotProduct(verts[i], fa->texinfo->vecs[0]);
				fa->tex[warpindex].t = DotProduct(verts[i], fa->texinfo->vecs[1]);
			}

			for (size_t i = 2; i < numverts; i++)
			{
				fa->indices.push_back(fa->numverts);
				fa->indices.push_back(fa->numverts + i - 1);
				fa->indices.push_back(fa->numverts + i);
			}

			fa->numverts = new_total;

			continue;
		}

		// cut it
		std::vector<float> dist(numverts + 1);
		for (size_t j = 0; j < numverts; j++)
			dist[j] = verts[j][plane] - location;

		// wrap cases
		dist[numverts] = dist[0];
		verts.push_back(verts[0]);

		std::vector<vertex_t> front;
		std::vector<vertex_t> back;
		for (size_t j = 0; j < numverts; j++)
		{
			if (dist[j] >= 0)
				front.push_back(verts[j]);
			if (dist[j] <= 0)
				back.push_back(verts[j]);
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ((dist[j] > 0) != (dist[j + 1] > 0))
			{
				// clip point
				vec3_t temp_front;
				vec3_t temp_back;
				float frac = dist[j] / (dist[j] - dist[j + 1]);
				for (size_t k = 0; k < 3; k++)
				{
					temp_front[k] = verts[j][k] + frac * (verts[j + 1][k] - verts[j][k]);
					temp_back[k]  = verts[j][k] + frac * (verts[j + 1][k] - verts[j][k]);
				}
				front.push_back(temp_front);
				back.push_back(temp_back);
			}
		}

		poly_stack.push(front);
		poly_stack.push(back);
	}
}
