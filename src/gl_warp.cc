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

static bool FindSubdevidePlane(std::vector<pos_cord> verts, size_t *axis, float *location)
{
	constexpr float subdivide = 64;

	for (size_t j = 0; j < 3; j++)
	{
		// Find bounds for this plane
		float min = FLT_MAX;
		float max = -FLT_MAX;
		for (pos_cord &vert : verts)
		{
			float value;
			if (j == 0)
				value = vert.x;
			if (j == 1)
				value = vert.y;
			if (j == 2)
				value = vert.z;
			if (value < min)
				min = value;
			if (value > max)
				max = value;
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
	std::vector<pos_cord> vertsx;
	for (size_t i = 0; i < fa->numverts; i++)
		vertsx.push_back(fa->verts[i]);

	std::stack<std::vector<pos_cord>> poly_stack;
	// Start stack with top level polygon
	poly_stack.push(vertsx);

	while (!poly_stack.empty())
	{
		std::vector<pos_cord> verts = poly_stack.top();
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
			fa->verts.resize(new_total);
			fa->tex.resize(new_total);
			for (size_t i = 0; i < numverts; i++)
			{
				size_t warpindex = fa->numverts + i;
				fa->verts[warpindex] = verts[i];
				vec3_t temp = {verts[i].x, verts[i].y, verts[i].z};
				fa->tex[warpindex].s = DotProduct(temp, fa->texinfo->vecs[0].vecs);
				fa->tex[warpindex].t = DotProduct(temp, fa->texinfo->vecs[1].vecs);
			}

			for (size_t i = 2; i < numverts; i++)
			{
				fa->indices->push_back(fa->numverts);
				fa->indices->push_back(fa->numverts + i - 1);
				fa->indices->push_back(fa->numverts + i);
			}

			fa->numverts = new_total;

			continue;
		}

		// cut it
		std::vector<float> dist(numverts + 1);
		for (size_t j = 0; j < numverts; j++)
		{
			float vert;
			if (plane == 0)
				vert = verts[j].x;
			if (plane == 1)
				vert = verts[j].y;
			if (plane == 2)
				vert = verts[j].z;
			dist[j] = vert - location;
		}

		// wrap cases
		dist[numverts] = dist[0];
		verts.push_back(verts[0]);

		std::vector<pos_cord> front;
		std::vector<pos_cord> back;
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
				float frac = dist[j] / (dist[j] - dist[j + 1]);
				front.push_back(verts[j] + ((verts[j + 1] - verts[j]) * frac));
				back.push_back(verts[j] + ((verts[j + 1] - verts[j]) * frac));
			}
		}

		poly_stack.push(front);
		poly_stack.push(back);
	}
}
