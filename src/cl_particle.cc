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

#include "quakedef.h"
#include "glquake.h"

#define DEFAULT_NUM_PARTICLES   2048
#define ABSOLUTE_MIN_PARTICLES  512
#define ABSOLUTE_MAX_PARTICLES  8192

static const int ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static const int ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static const int ramp3[8] = { 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x00, 0x00 };

cvar_t r_particles = { "r_particles", "1", CVAR_ARCHIVE };
cvar_t r_particles_alpha = { "r_particles_alpha", "1", CVAR_ARCHIVE };

particle_t *active_particles, *free_particles;

particle_t *particles;
int r_numparticles;

#define NUMVERTEXNORMALS 162

const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
	#include "anorms.h"
};

vec3_t avelocities[NUMVERTEXNORMALS];

static particle_t *R_GetParticle()
{
	particle_t *p;

	if (!free_particles)
		return NULL;

	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	return p;
}

void R_EntityParticles(entity_t *ent)
{
	static const float dist = 64;
	static const float beamlength = 16;

	for (int i = 0; i < NUMVERTEXNORMALS; i++)
	{
		float angle;
		angle = cl.time * avelocities[i][0];
		float sy = sinf(angle);
		float cy = cosf(angle);
		angle = cl.time * avelocities[i][1];
		float sp = sinf(angle);
		float cp = cosf(angle);
//		angle = cl.time * avelocities[i][2];
//		float sr = sinf(angle);
//		float cr = cosf(angle);

		vec3_t forward;
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		particle_t *p;
		if (!(p = R_GetParticle()))
			return;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;

		p->org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist + forward[0] * beamlength;
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist + forward[1] * beamlength;
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist + forward[2] * beamlength;
	}
}

void R_ClearParticles(void)
{
	free_particles = &particles[0];
	active_particles = NULL;

	for (int i = 0; i < r_numparticles; i++)
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;
}

void R_ReadPointFile_f(void)
{
	FILE *f;
	vec3_t org;
	char name[MAX_OSPATH];

	snprintf(name, sizeof(name), "maps/%s.pts", sv.name);

	COM_FOpenFile(name, &f);
	if (!f)
	{
		Con_Printf("couldn't open %s\n", name);
		return;
	}

	Con_Printf("Reading %s...\n", name);

	int c = 0;
	while (fscanf(f, "%f %f %f\n", &org[0], &org[1], &org[2]) == 3)
	{
		particle_t *p;
		if (!(p = R_GetParticle()))
		{
			Con_Printf("Not enough free particles\n");
			break;
		}

		p->die = 99999;
		p->color = (~(c++)) & 15;
		p->type = pt_static;
		VectorCopy(vec3_origin, p->vel);
		VectorCopy(org, p->org);
	}

	fclose(f);
	Con_Printf("%i points read\n", c);
}

/* Parse an effect out of the server message */
void R_ParseParticleEffect(void)
{
	vec3_t org, dir;

	for (int i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord();
	for (int i = 0; i < 3; i++)
		dir[i] = MSG_ReadChar() * (1.0 / 16);

	int count = MSG_ReadByte();
	if (count == 255) // rocket explosion
		count = 1024;

	int color = MSG_ReadByte();

	R_RunParticleEffect(org, dir, color, count);
}

void R_ParticleExplosion(vec3_t org)
{
	for (int i = 0; i < 1024; i++)
	{
		particle_t *p;
		if (!(p = R_GetParticle()))
			return;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand() & 3;
		if (i & 1)
		{
			p->type = pt_explode;
			for (int j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for (int j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength)
{
	int colorMod = 0;

	for (int i = 0; i < 512; i++)
	{
		particle_t *p;
		if (!(p = R_GetParticle()))
			return;

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;
		p->type = pt_blob;

		for (int j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + ((rand() % 32) - 16);
			p->vel[j] = (rand() % 512) - 256;
		}
	}
}

void R_BlobExplosion(vec3_t org)
{
	for (int i = 0; i < 1024; i++)
	{
		particle_t *p;
		if (!(p = R_GetParticle()))
			return;

		p->die = cl.time + 1 + (rand() & 8) * 0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand() % 6;
			for (int j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
			for (int j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
	for (int i = 0; i < count; i++)
	{
		particle_t *p;
		if (!(p = R_GetParticle()))
			return;

		if (count == 1024) // rocket explosion
		{
			p->die = cl.time + 5;
			p->color = ramp1[0];
			p->ramp = rand() & 3;
			if (i & 1)
			{
				p->type = pt_explode;
				for (int j = 0; j < 3; j++)
				{
					p->org[j] = org[j] + ((rand() % 32) - 16);
					p->vel[j] = (rand() % 512) - 256;
				}
			}
			else
			{
				p->type = pt_explode2;
				for (int j = 0; j < 3; j++)
				{
					p->org[j] = org[j] + ((rand() % 32) - 16);
					p->vel[j] = (rand() % 512) - 256;
				}
			}
		}
		else
		{
			p->die = cl.time + 0.1 * (rand() % 5);
			p->color = (color & ~7) + (rand() & 7);
			p->type = pt_slowgrav;

			for (int j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() & 15) - 8);
				p->vel[j] = dir[j] * 15;
			}
		}
	}
}

void R_LavaSplash(vec3_t org)
{
	for (int i = -16; i < 16; i++)
	{
		for (int j = -16; j < 16; j++)
		{
			for (int k = 0; k < 1; k++)
			{
				particle_t *p;
				if (!(p = R_GetParticle()))
					return;

				p->die = cl.time + 2 + (rand() & 31) * 0.02;
				p->color = 224 + (rand() & 7);
				p->type = pt_slowgrav;

				vec3_t dir;
				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalize(dir);
				float vel = 50 + (rand() & 63);
				VectorScale(dir, vel, p->vel);
			}
		}
	}
}

void R_TeleportSplash(vec3_t org)
{
	for (int i = -16; i < 16; i += 4)
	{
		for (int j = -16; j < 16; j += 4)
		{
			for (int k = -24; k < 32; k += 4)
			{
				particle_t *p;
				if (!(p = R_GetParticle()))
					return;

				p->die = cl.time + 0.2 + (rand() & 7) * 0.02;
				p->color = 7 + (rand() & 7);
				p->type = pt_slowgrav;

				vec3_t dir;
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);

				VectorNormalize(dir);
				float vel = 50 + (rand() & 63);
				VectorScale(dir, vel, p->vel);
			}
		}
	}
}

void R_RocketTrail(vec3_t start, vec3_t end, int type)
{
	vec3_t vec;
	static int tracercount;

	VectorSubtract(end, start, vec);
	float len = VectorNormalize(vec);

	while (len > 0)
	{
		len -= 3;

		particle_t *p;
		if (!(p = R_GetParticle()))
			return;

		VectorCopy(vec3_origin, p->vel);
		p->die = cl.time + 2;

		switch (type)
		{
		case GRENADE_TRAIL:
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (int j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;

		case BLOOD_TRAIL:
			p->type = pt_grav;
			p->color = 67 + (rand() & 3);
			for (int j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;

		case SLIGHT_BLOOD_TRAIL:
			p->type = pt_grav;
			p->color = 67 + (rand() & 3);
			for (int j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			len -= 3;
			break;

		case TRACER1_TRAIL:
		case TRACER2_TRAIL:
			p->die = cl.time + 0.5;
			p->type = pt_static;
			p->color = (type == TRACER1_TRAIL) ? 52 + ((tracercount & 4) << 1) : 230 + ((tracercount & 4) << 1);
			tracercount++;

			VectorCopy(start, p->org) ;
			if (tracercount & 1)
			{
				p->vel[0] = 30 * vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 * vec[0];
			}
			break;

		case VOOR_TRAIL:
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die = cl.time + 0.3;
			for (int j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() & 15) - 8);
			break;

		case ROCKET_TRAIL:
			p->ramp = (rand() & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (int j = 0; j < 3; j++)
				p->org[j] = start[j] + ((rand() % 6) - 3);
			break;
		}

		VectorAdd(start, vec, start);
	}
}

void CL_RunParticles(void)
{
	extern cvar_t sv_gravity;

	float frametime = cl.time - cl.oldtime;
	float time3 = frametime * 15;
	float time2 = frametime * 10; // 15;
	float time1 = frametime * 5;
	float grav = frametime * sv_gravity.value * 0.05;
	float dvel = 4 * frametime;

	// free dead particles
	while (active_particles && active_particles->die < cl.time)
	{
		particle_t *kill = active_particles;
		active_particles = active_particles->next;
		kill->next = free_particles;
		free_particles = kill;
	}

	for (particle_t *p = active_particles; p; p = p->next)
	{
		// free dead particles
		while (p->next && p->next->die < cl.time)
		{
			particle_t *kill = p->next;
			p->next = p->next->next;
			kill->next = free_particles;
			free_particles = kill;
		}

		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;

		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int) p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >= 8)
				p->die = -1;
			else
				p->color = ramp1[(int) p->ramp];
			for (int i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >= 8)
				p->die = -1;
			else
				p->color = ramp2[(int) p->ramp];
			for (int i = 0; i < 3; i++)
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (int i = 0; i < 3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			for (int i = 0; i < 2; i++)
				p->vel[i] -= p->vel[i] * dvel;
			p->vel[2] -= grav;
			break;

		case pt_grav:
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
	}
}

void R_InitParticles(void)
{
	int i;

	/* generate some random particle velocities */
	for (int i = 0; i < NUMVERTEXNORMALS; i++)
		for (int j = 0; j < 3; j++)
			avelocities[i][j] = (rand() & 255) * 0.01;

	if ((i = COM_CheckParm("-particles")) && i + 1 < com_argc)
	{
		r_numparticles = atoi(com_argv[i + 1]);
		r_numparticles = CLAMP(ABSOLUTE_MIN_PARTICLES, r_numparticles, ABSOLUTE_MAX_PARTICLES);
	}
	else
	{
		r_numparticles = DEFAULT_NUM_PARTICLES;
	}

	particles = (particle_t *) Hunk_AllocName(r_numparticles * sizeof(particle_t), "particles");

	Cmd_AddCommand("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable(&r_particles);
	Cvar_RegisterVariable(&r_particles_alpha);
}
