/*
 * Server code for moving users
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

edict_t *sv_player;

extern cvar_t sv_friction;
cvar_t sv_edgefriction = { "edgefriction", "2" };
extern cvar_t sv_stopspeed;

static vec3_t forward, right, up;

vec3_t wishdir;
float wishspeed;

// world
float *angles;
float *origin;
float *velocity;

bool onground;

usercmd_t cmd;

cvar_t sv_idealpitchscale = { "sv_idealpitchscale", "0.8" };

cvar_t sv_altnoclip = { "sv_altnoclip", "1" }; //don't save to config ... no reason to do so

cvar_t sv_maxspeed = { "sv_maxspeed", "320", false, true };
cvar_t sv_accelerate = { "sv_accelerate", "10" };

#define	MAX_FORWARD 6
void SV_SetIdealPitch(void)
{
	float z[MAX_FORWARD];
	int i, dir, steps;

	if (!((int) sv_player->v.flags & FL_ONGROUND))
		return;

	float angleval = sv_player->v.angles[YAW] * M_PI * 2 / 360;
	float sinval = sinf(angleval);
	float cosval = cosf(angleval);

	for (i = 0; i < MAX_FORWARD; i++)
	{
		vec3_t top, bottom;

		top[0] = sv_player->v.origin[0] + cosval * (i + 3) * 12;
		top[1] = sv_player->v.origin[1] + sinval * (i + 3) * 12;
		top[2] = sv_player->v.origin[2] + sv_player->v.view_ofs[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		trace_t tr = SV_Move(top, vec3_origin, vec3_origin, bottom, 1, sv_player);
		if (tr.allsolid)
			return;	// looking at a wall, leave ideal the way is was

		if (tr.fraction == 1)
			return;	// near a dropoff

		z[i] = top[2] + tr.fraction * (bottom[2] - top[2]);
	}

	dir = 0;
	steps = 0;
	for (int j = 1; j < i; j++)
	{
		int step = z[j] - z[j - 1];
		if (step > -ON_EPSILON && step < ON_EPSILON)
			continue;

		if (dir && (step - dir > ON_EPSILON || step - dir < -ON_EPSILON))
			return; // mixed changes

		steps++;
		dir = step;
	}

	if (!dir)
	{
		sv_player->v.idealpitch = 0;
		return;
	}

	if (steps < 2)
		return;
	sv_player->v.idealpitch = -dir * sv_idealpitchscale.value;
}

static void SV_UserFriction(void)
{
	float *vel, speed, newspeed, control, friction;
	vec3_t start, stop;
	trace_t trace;

	vel = velocity;

	speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = origin[0] + vel[0] / speed * 16;
	start[1] = stop[1] = origin[1] + vel[1] / speed * 16;
	start[2] = origin[2] + sv_player->v.mins[2];
	stop[2] = start[2] - 34;

	trace = SV_Move(start, vec3_origin, vec3_origin, stop, true, sv_player);

	if (trace.fraction == 1.0)
		friction = sv_friction.value * sv_edgefriction.value;
	else
		friction = sv_friction.value;

	// apply friction
	control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
	newspeed = speed - host_frametime * control * friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}

static void SV_Accelerate(void)
{
	int i;
	float addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct(velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv_accelerate.value * host_frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishdir[i];
}

static void SV_AirAccelerate(vec3_t wishveloc)
{
	float addspeed, wishspd, accelspeed, currentspeed;

	wishspd = VectorNormalize(wishveloc);
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct(velocity, wishveloc);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
//	accelspeed = sv_accelerate.value * host_frametime;
	accelspeed = sv_accelerate.value * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (int i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishveloc[i];
}

static void DropPunchAngle(void)
{
	float len = VectorNormalize(sv_player->v.punchangle);

	len -= 10 * host_frametime;
	if (len < 0)
		len = 0;
	VectorScale(sv_player->v.punchangle, len, sv_player->v.punchangle);
}

static void SV_WaterMove(void)
{
	int i;
	vec3_t wishvel;
	float speed, newspeed, wishspeed, addspeed, accelspeed;

	// user intentions
	AngleVectors(sv_player->v.v_angle, forward, right, up);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;

	if (!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += cmd.upmove;

	wishspeed = VectorLength(wishvel);
	if (wishspeed > sv_maxspeed.value)
	{
		VectorScale(wishvel, sv_maxspeed.value / wishspeed, wishvel);
		wishspeed = sv_maxspeed.value;
	}
	wishspeed *= 0.7;

	// water friction
	speed = VectorLength(velocity);
	if (speed)
	{
		newspeed = speed - host_frametime * speed * sv_friction.value;
		if (newspeed < 0)
			newspeed = 0;
		VectorScale(velocity, newspeed / speed, velocity);
	}
	else
	{
		newspeed = 0;
	}

	// water acceleration
	if (!wishspeed)
		return;

	addspeed = wishspeed - newspeed;
	if (addspeed <= 0)
		return;

	VectorNormalize(wishvel);
	accelspeed = sv_accelerate.value * wishspeed * host_frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i = 0; i < 3; i++)
		velocity[i] += accelspeed * wishvel[i];
}

static void SV_WaterJump(void)
{
	if (sv.time > sv_player->v.teleport_time || !sv_player->v.waterlevel)
	{
		sv_player->v.flags = (int) sv_player->v.flags & ~FL_WATERJUMP;
		sv_player->v.teleport_time = 0;
	}
	sv_player->v.velocity[0] = sv_player->v.movedir[0];
	sv_player->v.velocity[1] = sv_player->v.movedir[1];
}

/* new, alternate noclip. old noclip is still handled in SV_AirMove */
static void SV_NoclipMove(void)
{
	AngleVectors(sv_player->v.v_angle, forward, right, up);

	velocity[0] = forward[0] * cmd.forwardmove + right[0] * cmd.sidemove;
	velocity[1] = forward[1] * cmd.forwardmove + right[1] * cmd.sidemove;
	velocity[2] = forward[2] * cmd.forwardmove + right[2] * cmd.sidemove;
	velocity[2] += cmd.upmove * 2; //doubled to match running speed

	if (VectorLength(velocity) > sv_maxspeed.value)
	{
		VectorNormalize(velocity);
		VectorScale(velocity, sv_maxspeed.value, velocity);
	}
}

static void SV_AirMove(void)
{
	int i;
	vec3_t wishvel;
	float fmove, smove;

	AngleVectors(sv_player->v.angles, forward, right, up);

	fmove = cmd.forwardmove;
	smove = cmd.sidemove;

	// hack to not let you back into teleporter
	if (sv.time < sv_player->v.teleport_time && fmove < 0)
		fmove = 0;

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;

	if ((int) sv_player->v.movetype != MOVETYPE_WALK)
		wishvel[2] = cmd.upmove;
	else
		wishvel[2] = 0;

	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);
	if (wishspeed > sv_maxspeed.value)
	{
		VectorScale(wishvel, sv_maxspeed.value / wishspeed, wishvel);
		wishspeed = sv_maxspeed.value;
	}

	if (sv_player->v.movetype == MOVETYPE_NOCLIP)
	{	// noclip
		VectorCopy(wishvel, velocity);
	}
	else if (onground)
	{
		SV_UserFriction();
		SV_Accelerate();
	}
	else
	{	// not on ground, so little effect on velocity
		SV_AirAccelerate(wishvel);
	}
}

/*
 * the move fields specify an intended velocity in pix/sec
 * the angle fields specify an exact angular motion in degrees
 */
static void SV_ClientThink(void)
{
	vec3_t v_angle;

	if (sv_player->v.movetype == MOVETYPE_NONE)
		return;

	onground = (int) sv_player->v.flags & FL_ONGROUND;

	origin = sv_player->v.origin;
	velocity = sv_player->v.velocity;

	DropPunchAngle();

	// if dead, behave differently
	if (sv_player->v.health <= 0)
		return;

	// angles
	// show 1/3 the pitch angle and all the roll angle
	cmd = host_client->cmd;
	angles = sv_player->v.angles;

	VectorAdd(sv_player->v.v_angle, sv_player->v.punchangle, v_angle);
	angles[ROLL] = V_CalcRoll(sv_player->v.angles, sv_player->v.velocity) * 4;
	if (!sv_player->v.fixangle)
	{
		angles[PITCH] = -v_angle[PITCH] / 3;
		angles[YAW] = v_angle[YAW];
	}

	if ((int) sv_player->v.flags & FL_WATERJUMP)
	{
		SV_WaterJump();
		return;
	}
	// walk
	//johnfitz -- alternate noclip
	if (sv_player->v.movetype == MOVETYPE_NOCLIP && sv_altnoclip.value)
		SV_NoclipMove();
	else if (sv_player->v.waterlevel >= 2 && sv_player->v.movetype != MOVETYPE_NOCLIP)
		SV_WaterMove();
	else
		SV_AirMove();
	//johnfitz
}

static void SV_ReadClientMove(usercmd_t *move)
{
	int i, bits;
	vec3_t angle;

	// read ping time
	host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] = sv.time - MSG_ReadFloat();
	host_client->num_pings++;

	// read current angles
	if (host_client->netconnection->mod == MOD_PROQUAKE) // JPG - precise aim for ProQuake!!
	{
		for (i = 0; i < 3; i++)
			angle[i] = MSG_ReadPreciseAngle();
	}
	else
	{
		for (i = 0; i < 3; i++)
			angle[i] = MSG_ReadAngle();
	}

	VectorCopy(angle, host_client->edict->v.v_angle);

	// read movement
	move->forwardmove = MSG_ReadShort();
	move->sidemove = MSG_ReadShort();
	move->upmove = MSG_ReadShort();

	// read buttons
	bits = MSG_ReadByte();
	host_client->edict->v.button0 = bits & 1;
	host_client->edict->v.button2 = (bits & 2) >> 1;

	if ((i = MSG_ReadByte()))
		host_client->edict->v.impulse = i;
}

/* Returns false if the client should be killed */
static bool SV_ReadClientMessage(void)
{
	int ret, cmd;
	char *s;

	do
	{
		nextmsg: ret = NET_GetMessage(host_client->netconnection);
		if (ret == -1)
		{
			Sys_Printf("SV_ReadClientMessage: NET_GetMessage failed\n");
			return false;
		}
		if (!ret)
			return true;

		MSG_BeginReading();

		while (true)
		{
			if (!host_client->active)
				return false;	// a command caused an error

			if (msg_badread)
			{
				Sys_Printf("SV_ReadClientMessage: badread\n");
				return false;
			}

			cmd = MSG_ReadChar();

			switch (cmd)
			{
			case -1:
				goto nextmsg;
				// end of message

			default:
				Sys_Printf("SV_ReadClientMessage: unknown command char\n");
				return false;

			case clc_nop:
//				Sys_Printf ("clc_nop\n");
				break;

			case clc_stringcmd:
				s = MSG_ReadString();
				if (host_client->privileged)
					ret = 2;
				else
					ret = 0;
				if (!strncasecmp(s, "status", 6) ||
				    !strncasecmp(s, "god", 3) ||
				    !strncasecmp(s, "notarget", 8) ||
				    !strncasecmp(s, "fly", 3) ||
				    !strncasecmp(s, "name", 4) ||
				    !strncasecmp(s, "noclip", 6) ||
				    !strncasecmp(s, "say", 3) ||
				    !strncasecmp(s, "say_team", 8) ||
				    !strncasecmp(s, "tell", 4) ||
				    !strncasecmp(s, "color", 5) ||
				    !strncasecmp(s, "kill", 4) ||
				    !strncasecmp(s, "pause", 5) ||
				    !strncasecmp(s, "spawn", 5) ||
				    !strncasecmp(s, "begin", 5) ||
				    !strncasecmp(s, "prespawn", 8) ||
				    !strncasecmp(s, "kick", 4) ||
				    !strncasecmp(s, "ping", 4) ||
				    !strncasecmp(s, "give", 4) ||
				    !strncasecmp(s, "ban", 3) ||
				    !strncasecmp(s, "qcexec", 6))
					ret = 1;

				if (ret == 2)
					Cbuf_InsertText(s);
				else if (ret == 1)
					Cmd_ExecuteString(s, src_client);
				else
					Con_DPrintf("%s tried to %s\n", host_client->name, s);
				break;

			case clc_disconnect:
//				Sys_Printf ("SV_ReadClientMessage: client disconnected\n");
				return false;

			case clc_move:
				SV_ReadClientMove(&host_client->cmd);
				break;
			}
		}
	} while (ret == 1);

	return true;
}

void SV_RunClients(void)
{
	host_client = svs.clients;
	for (int i = 0; i < svs.maxclients; i++)
	{
		if (!host_client->active)
			continue;

		sv_player = host_client->edict;

		if (!SV_ReadClientMessage())
		{
			SV_DropClient(false); // client misbehaved...
			continue;
		}

		if (!host_client->spawned)
		{
			// clear client movement until a new packet is received
			memset(&host_client->cmd, 0, sizeof(host_client->cmd));
			continue;
		}

		// always pause in single player if in console or menus
		if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
			SV_ClientThink();

		host_client++;
	}
}
