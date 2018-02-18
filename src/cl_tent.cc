/*
 * Client side temporary entities
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

int num_temp_entities;
entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t cl_beams[MAX_BEAMS];

void CL_ParseBeam(model_t *m)
{
	int ent;
	vec3_t start, end;
	beam_t *b;
	int i;

	if (!m)
		Sys_Error("Beam model not found");

	ent = MSG_ReadShort();

	start[0] = MSG_ReadCoord();
	start[1] = MSG_ReadCoord();
	start[2] = MSG_ReadCoord();

	end[0] = MSG_ReadCoord();
	end[1] = MSG_ReadCoord();
	end[2] = MSG_ReadCoord();

	// override any beam with the same entity
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			return;
		}

	// find a free beam
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy(start, b->start);
			VectorCopy(end, b->end);
			return;
		}
	}

	Con_Printf("Beam list overflow!\n");
}

void CL_ParseTEnt(void)
{
	int type;
	vec3_t pos;
	dlight_t *dl;
	int rnd;
	int colorStart, colorLength;

	type = MSG_ReadByte();
	switch (type)
	{
	case TE_WIZSPIKE: // spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 20, 30);
		S_StartSound(-1, 0, S_ForName("wizard/hit.wav"), pos, 1, 1);
		break;

	case TE_KNIGHTSPIKE: // spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 226, 20);
		S_StartSound(-1, 0, S_ForName("hknight/hit.wav"), pos, 1, 1);
		break;

	case TE_SPIKE: // spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();

		R_RunParticleEffect(pos, vec3_origin, 0, 10);

		if (rand() % 5)
			S_StartSound(-1, 0, S_ForName("weapons/tink1.wav"), pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, S_ForName("weapons/ric1.wav"), pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, S_ForName("weapons/ric2.wav"), pos, 1, 1);
			else
				S_StartSound(-1, 0, S_ForName("weapons/ric3.wav"), pos, 1, 1);
		}
		break;

	case TE_SUPERSPIKE: // super spike hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 0, 20);

		if (rand() % 5)
			S_StartSound(-1, 0, S_ForName("weapons/tink1.wav"), pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound(-1, 0, S_ForName("weapons/ric1.wav"), pos, 1, 1);
			else if (rnd == 2)
				S_StartSound(-1, 0, S_ForName("weapons/ric2.wav"), pos, 1, 1);
			else
				S_StartSound(-1, 0, S_ForName("weapons/ric3.wav"), pos, 1, 1);
		}
		break;

	case TE_GUNSHOT: // bullet hitting wall
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_RunParticleEffect(pos, vec3_origin, 0, 20);
		break;

	case TE_EXPLOSION: // rocket explosion
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_ParticleExplosion(pos);
		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		S_StartSound(-1, 0, S_ForName("weapons/r_exp3.wav"), pos, 1, 1);
		break;

	case TE_TAREXPLOSION: // tarbaby explosion
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_BlobExplosion(pos);

		S_StartSound(-1, 0, S_ForName("weapons/r_exp3.wav"), pos, 1, 1);
		break;

	case TE_LIGHTNING1: // lightning bolts
		CL_ParseBeam(Mod_ForName("progs/bolt.mdl"));
		break;

	case TE_LIGHTNING2: // lightning bolts
		CL_ParseBeam(Mod_ForName("progs/bolt2.mdl"));
		break;

	case TE_LIGHTNING3: // lightning bolts
		CL_ParseBeam(Mod_ForName("progs/bolt3.mdl"));
		break;

// PGM 01/21/97 
	case TE_BEAM: // grappling hook beam
		CL_ParseBeam(Mod_ForName("progs/beam.mdl"));
		break;
// PGM 01/21/97

	case TE_LAVASPLASH:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_LavaSplash(pos);
		break;

	case TE_TELEPORT:
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		R_TeleportSplash(pos);
		break;

	case TE_EXPLOSION2: // color mapped explosion
		pos[0] = MSG_ReadCoord();
		pos[1] = MSG_ReadCoord();
		pos[2] = MSG_ReadCoord();
		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();
		R_ParticleExplosion2(pos, colorStart, colorLength);
		dl = CL_AllocDlight(0);
		VectorCopy(pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		S_StartSound(-1, 0, S_ForName("weapons/r_exp3.wav"), pos, 1, 1);
		break;

	default:
		Sys_Error("bad type");
	}
}

static entity_t *CL_NewTempEntity(void)
{
	entity_t *ent;

	if (cl_numvisedicts == MAX_VISEDICTS)
		return NULL;

	if (num_temp_entities == MAX_TEMP_ENTITIES)
		return NULL;

	ent = &cl_temp_entities[num_temp_entities];
	memset(ent, 0, sizeof(*ent));
	num_temp_entities++;
	cl_visedicts[cl_numvisedicts] = ent;
	cl_numvisedicts++;

	ent->colormap = 0;
	return ent;
}

void CL_UpdateTEnts(void)
{
	int i, j;
	beam_t *b;
	vec3_t dist, org;
	float d;
	entity_t *ent;
	float yaw, pitch;
	float forward;

	num_temp_entities = 0;
	srand((int) (cl.ctime * 1000)); // freeze beams when paused

	// update lightning
	for (i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
		{
			VectorCopy(cl_entities[cl.viewentity].origin, b->start);
		}

		// calculate pitch and yaw
		VectorSubtract(b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{

			yaw = (int) (atan2f(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrtf(dist[0] * dist[0] + dist[1] * dist[1]);
			pitch = (int) (atan2f(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		// add new entities for the lightning
		VectorCopy(b->start, org);
		d = VectorNormalize(dist);
		while (d > 0)
		{
			ent = CL_NewTempEntity();
			if (!ent)
				return;
			VectorCopy(org, ent->origin);
			ent->model = b->model;
			ent->angles[0] = pitch;
			ent->angles[1] = yaw;
			ent->angles[2] = rand() % 360;

			// use j instead of using i twice, so we don't corrupt memory
			for (j = 0; j < 3; j++)
				org[j] += dist[j] * 30;
			d -= 30;
		}
	}
}
