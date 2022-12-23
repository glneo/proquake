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

#include <cstddef>

#include "quakedef.h"
#include "glquake.h"

#include "particle.fs.h"
#include "particle.vs.h"

extern particle_t *active_particles;

// shader program
static GLuint particle_program;

// uniforms used in vertex shader
static GLuint particleProjectionLoc;
static GLuint particleModelViewLoc;

// uniforms used in fragment shader
static GLuint particleAlphaLoc;

// attribs
static GLuint particleVertexAttrIndex;
static GLuint particleColorAttrIndex;

static GLuint particlesVBO;

#define ABSOLUTE_MAX_PARTICLES  8192
typedef struct {
	vec3_t vert;
	GLubyte color[3];
} particleData_t;
static particleData_t particleData[ABSOLUTE_MAX_PARTICLES];

void GL_DrawParticles(void)
{
	if (!active_particles)
		return;

	float particlesAlpha = CLAMP(0.0f, r_particles_alpha.value, 1.0f);

	int particleCount = 0;
	for (particle_t *p = active_particles; p; p = p->next)
	{
		byte p_red = ((byte *)&d_8to24table[p->color])[0];
		byte p_green = ((byte *)&d_8to24table[p->color])[1];
		byte p_blue = ((byte *)&d_8to24table[p->color])[2];

		particleData_t *particle = &particleData[particleCount];
		particle->color[0] = p_red;
		particle->color[1] = p_green;
		particle->color[2] = p_blue;
		particle->vert[0] = p->org[0];
		particle->vert[1] = p->org[1];
		particle->vert[2] = p->org[2];

		particleCount++;
	}

	// setup
	glEnable(GL_BLEND);
	glUseProgram(particle_program);

	// set uniforms
	glUniformMatrix4fv(particleProjectionLoc, 1, GL_FALSE, projectionMatrix.get());
	glUniformMatrix4fv(particleModelViewLoc, 1, GL_FALSE, modelViewMatrix.get());
	glUniform1f(particleAlphaLoc, particlesAlpha);

	// set attributes
	glBindBuffer(GL_ARRAY_BUFFER, particlesVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(particleData_t) * particleCount, &particleData[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(particleVertexAttrIndex);
	glVertexAttribPointer(particleVertexAttrIndex, 3, GL_FLOAT, GL_FALSE, sizeof(particleData_t), BUFFER_OFFSET(offsetof(particleData_t, vert)));
	glEnableVertexAttribArray(particleColorAttrIndex);
	glVertexAttribPointer(particleColorAttrIndex, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(particleData_t), BUFFER_OFFSET(offsetof(particleData_t, color)));

	// draw
//	Con_Printf("Drawing %d particles\n", particleCount);
	glDrawArrays(GL_POINTS, 0, particleCount);

	// cleanup
	glDisableVertexAttribArray(particleColorAttrIndex);
	glDisableVertexAttribArray(particleVertexAttrIndex);
	glDisable(GL_BLEND);
}

static void GL_CreateParticleShaders(void)
{
	// generate program
	particle_program = LoadShader((const char *)particle_vs, particle_vs_len,
	                              (const char *)particle_fs, particle_fs_len);

	// get attribute locations
	particleVertexAttrIndex = GL_GetAttribLocation(particle_program, "Vert");
	particleColorAttrIndex = GL_GetAttribLocation(particle_program, "Color");

	// get uniform locations
	particleProjectionLoc = GL_GetUniformLocation(particle_program, "Projection");
	particleModelViewLoc = GL_GetUniformLocation(particle_program, "ModelView");
	particleAlphaLoc = GL_GetUniformLocation(particle_program, "Alpha");
}

void GL_ParticleInit(void)
{
	GL_CreateParticleShaders();

	glGenBuffers(1, &particlesVBO);
}
