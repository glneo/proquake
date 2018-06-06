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

#include <GLES2/gl2.h>

#include "quakedef.h"
#include "glquake.h"

extern particle_t *active_particles;
gltexture_t *particletexture; // little dot for particles

void GL_DrawParticles(void)
{
	vec3_t up, right;

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);

// setup
	glUseProgram(r_brush_program);

	glEnableVertexAttribArray(brushTexCoordsAttrIndex);
	glEnableVertexAttribArray(brushVertexAttrIndex);

// set uniforms
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, projectionMatrix.get());
	glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, modelViewMatrix.get());
	glUniform1i(texLoc, 0);

	GL_Bind(particletexture);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GLfloat texts[] = {
		0, 0,
		1, 0,
		0, 1,
	};

	GLfloat verts[3*3];

	glVertexAttribPointer(brushTexCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, texts);
	glVertexAttribPointer(brushVertexAttrIndex, 3, GL_FLOAT, GL_FALSE, 0, verts);

	for (particle_t *p = active_particles; p; p = p->next)
	{
		byte p_red = ((byte *)&d_8to24table[(int) p->color])[0];
		byte p_green = ((byte *)&d_8to24table[(int) p->color])[1];
		byte p_blue = ((byte *)&d_8to24table[(int) p->color])[2];
		byte p_alpha = CLAMP(0, r_particles_alpha.value, 1) * 255;

		// hack a scale up to keep particles from disappearing
		float scale = (p->org[0] - r_origin[0]) * vpn[0] +
		        (p->org[1] - r_origin[1]) * vpn[1] +
		        (p->org[2] - r_origin[2]) * vpn[2];
		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;

		glUniform4f(colorLoc, p_red / 255.0f, p_green / 255.0f, p_blue / 255.0f, p_alpha / 255.0f);
		verts[0] = p->org[0];
		verts[1] = p->org[1];
		verts[2] = p->org[2];
		verts[3] = p->org[0] + up[0] * scale;
		verts[4] = p->org[1] + up[1] * scale;
		verts[5] = p->org[2] + up[2] * scale;
		verts[6] = p->org[0] + right[0] * scale;
		verts[7] = p->org[1] + right[1] * scale;
		verts[8] = p->org[2] + right[2] * scale;
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	// clean up
	glDisableVertexAttribArray(brushTexCoordsAttrIndex);
	glDisableVertexAttribArray(brushVertexAttrIndex);

	glUseProgram(0);
}

static byte dottexture[8][8] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void GL_InitParticleTexture(void)
{
	int x, y;
	byte data[8][8][4];

	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}
	particletexture = TexMgr_LoadImage("particle", 8, 8, SRC_RGBA, (byte *)data, TEX_PERSIST | TEX_ALPHA | TEX_LINEAR);
}
