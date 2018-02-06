/*
 * Client main loop
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

// these two are not intended to be set directly
cvar_t cl_name = { "_cl_name", "player", CVAR_ARCHIVE };
cvar_t cl_color = { "_cl_color", "0", CVAR_ARCHIVE };

cvar_t cl_shownet = { "cl_shownet", "0" }; // can be 0, 1, or 2
cvar_t cl_nolerp = { "cl_nolerp", "0" };
cvar_t cl_gameplayhack_monster_lerp = { "cl_gameplayhack_monster_lerp", "1" };

cvar_t lookspring = { "lookspring", "0", CVAR_ARCHIVE };
cvar_t lookstrafe = { "lookstrafe", "0", CVAR_ARCHIVE };
cvar_t sensitivity = { "sensitivity", "3", CVAR_ARCHIVE };

cvar_t m_pitch = { "m_pitch", "0.022", CVAR_ARCHIVE };
cvar_t m_yaw = { "m_yaw", "0.022", CVAR_ARCHIVE };
cvar_t m_forward = { "m_forward", "1", CVAR_ARCHIVE };
cvar_t m_side = { "m_side", "0.8", CVAR_ARCHIVE };

cvar_t pq_moveup = { "pq_moveup", "0", CVAR_ARCHIVE };

cvar_t pq_smoothcam = { "pq_smoothcam", "1", CVAR_ARCHIVE };

cvar_t cl_maxpitch = { "cl_maxpitch", "90", CVAR_ARCHIVE };
cvar_t cl_minpitch = { "cl_minpitch", "-90", CVAR_ARCHIVE };

cvar_t cl_bobbing = { "cl_bobbing", "0", CVAR_ARCHIVE};

cvar_t	cfg_unbindall = {"cfg_unbindall", "1", CVAR_ARCHIVE};

client_static_t cls;
client_state_t cl;

// FIXME: Dynamically allocate these
efrag_t cl_efrags[MAX_EFRAGS];
entity_t cl_entities[MAX_EDICTS];
entity_t cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t cl_dlights[MAX_DLIGHTS];

entity_t *cl_visedicts[MAX_VISEDICTS];
int cl_numvisedicts;

extern cvar_t scr_fov;
static float savedsensitivity;
static float savedfov;

void CL_ClearState(void)
{
	int i;

	if (!sv.active)
		Host_ClearMemory();

	// wipe the entire cl structure
	memset(&cl, 0, sizeof(cl));

	SZ_Clear(&cls.message);

	// clear other arrays
	memset(cl_efrags, 0, sizeof(cl_efrags));
	memset(cl_entities, 0, sizeof(cl_entities));
	memset(cl_dlights, 0, sizeof(cl_dlights));
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset(cl_temp_entities, 0, sizeof(cl_temp_entities));
	memset(cl_beams, 0, sizeof(cl_beams));

	// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];
	cl.free_efrags[i].entnext = NULL;
}

/*
 =====================
 CL_Disconnect

 Sends a disconnect message to the server
 This is also called on Host_Error, so it shouldn't cause any errors
 =====================
 */
void CL_Disconnect(void)
{
// stop sounds (especially looping!)
	S_StopAllSounds(true);

//	// This makes sure ambient sounds remain silent
//	cl.worldmodel = NULL;

// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback();
	else if (cls.state == ca_connected)
	{
		if (cls.demorecording)
			CL_Stop_f();

		Con_DPrintf("Sending clc_disconnect\n");
		SZ_Clear(&cls.message);
		MSG_WriteByte(&cls.message, clc_disconnect);
		NET_SendUnreliableMessage(cls.netcon, &cls.message);
		SZ_Clear(&cls.message);
		NET_Close(cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer(false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
	cl.intermission = 0; // Baker: So critical.  SCR_UpdateScreen uses this.
//	SCR_EndLoadingPlaque (); // Baker: NOOOOOO.  This shows between start demos.  We need this.

}

void CL_Disconnect_f(void)
{
	CL_Clear_Demos_Queue(); // disconnect is a very intentional action so clear out startdemos

	CL_Disconnect();
	if (sv.active)
		Host_ShutdownServer(false);
}

/*
 =====================
 CL_EstablishConnection

 Host should be either "local" or a net address to be passed on
 =====================
 */
void CL_EstablishConnection(char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect();

	cls.netcon = NET_Connect(host);
	if (!cls.netcon) // Baker 3.60 - Rook's Qrack port 26000 notification on failure
	{
		Con_Printf("\nsyntax: connect server:port (port is optional)\n"); //r00k added
		if (net_hostport != 26000)
			Con_Printf("\nTry using port 26000\n"); //r00k added
		Host_Error("connect failed");
	}

	Con_DPrintf("CL_EstablishConnection: connected to %s\n", host);

	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing

	MSG_WriteByte(&cls.message, clc_nop);	// JPG 3.40 - fix for NAT
}

/*
 =====================
 CL_SignonReply

 An svc_signonnum has been received, perform a client side setup
 =====================
 */
void CL_SignonReply(void)
{
	char str[8192];

	Con_DPrintf("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, "prespawn");

		// JPG 3.50
		if (cls.netcon && !cls.netcon->encrypt)
			cls.netcon->encrypt = 3;
		break;

	case 2:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, va("color %i %i\n", ((int)cl_color.value) >> 4, ((int)cl_color.value) & 15));

		MSG_WriteByte(&cls.message, clc_stringcmd);
		snprintf(str, sizeof(str), "spawn %s", cls.spawnparms);
		MSG_WriteString(&cls.message, str);

		break;

	case 3:
		MSG_WriteByte(&cls.message, clc_stringcmd);
		MSG_WriteString(&cls.message, "begin");
		Cache_Report();		// print remaining memory

		// JPG 3.50
		if (cls.netcon)
			cls.netcon->encrypt = 1;
		break;

	case 4:
		SCR_EndLoadingPlaque();		// allow normal screen updates
		break;
	}
}

/*
 =====================
 CL_NextDemo

 Called to play the next demo in the demo loop
 =====================
 */
void CL_NextDemo(void)
{
	char str[1024];

	if (cls.demonum == -1)
		return;		// don't play demos

//	SCR_BeginLoadingPlaque (); // Baker: Moved below

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			Con_DPrintf("No demos listed with startdemos\n");

			cls.demonum = -1;
			CL_Disconnect();	// JPG 1.05 - patch by CSR to fix crash
			return;
		}
	}

	SCR_BeginLoadingPlaque(); // Baker: Moved to AFTER we know demo will play
	snprintf(str, sizeof(str), "nextstartdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText(str);
	cls.demonum++;
}

/*
 ==============
 CL_PrintEntities_f
 ==============
 */
void CL_PrintEntities_f(void)
{
	entity_t *ent;
	int i;

	for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++)
	{
		Con_Printf("%3i:", i);
		if (!ent->model)
		{
			Con_Printf("EMPTY\n");
			continue;
		}
		Con_Printf("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n", ent->model->name, ent->frame, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
	}
}

/*
 ===============
 CL_AllocDlight
 ===============
 */
dlight_t *CL_AllocDlight(int key)
{
	int i;
	dlight_t *dl;

// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++)
		{
			if (dl->key == key)
			{
				memset(dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset(dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset(dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

/*
 ===============
 CL_DecayLights
 ===============
 */
void CL_DecayLights(void)
{
	int i;
	dlight_t *dl;
	float time;

	time = fabs(cl.time - cl.oldtime); // Baker: To make sure it stays forward oriented time

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= time * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
 ===============
 CL_LerpPoint

 Determines the fraction between the last two messages that the objects
 should be put at.
 ===============
 */
float CL_LerpPoint(void)
{
	float f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp.value || cls.timedemo || sv.active)
	{
		// Baker 3.75 demo rewind
		cl.time = cl.ctime = cl.mtime[0];
		return 1;
	}

	if (f > 0.1) // dropped packet, or start of demo
	{
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1f;
	}
	frac = (cl.ctime - cl.mtime[1]) / f;

	if (frac < 0)
	{
		if (frac < -0.01)
		{
			if (bumper_on)
				cl.ctime = cl.mtime[1];
			else
				cl.time = cl.ctime = cl.mtime[1];
		}
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
		{
			if (bumper_on)
				cl.ctime = cl.mtime[0];
			else
				cl.time = cl.ctime = cl.mtime[0]; // Here is where we get foobar'd
		}
		frac = 1;
	}

	return frac;
}

extern cvar_t pq_timer; // JPG - need this for CL_RelinkEntities

/*
 ===============
 CL_RelinkEntities
 ===============
 */
void CL_RelinkEntities(void)
{
	entity_t *ent;
	int i, j;
	float frac, f, d;
	vec3_t delta;
	vec3_t oldorg;
	dlight_t *dl;
	void CL_ClearInterpolation(entity_t *ent);
	void CL_EntityInterpolateOrigins(entity_t *ent);
	void CL_EntityInterpolateAngles(entity_t *ent);

// determine partial update time
	frac = CL_LerpPoint();

// JPG - check to see if we need to update the status bar
//	if (pq_timer.value && ((int)cl.time != (int)cl.oldtime))
//		Sbar_Changed();

	cl_numvisedicts = 0;

//
// interpolate player info
//
	for (i = 0; i < 3; i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);
	//PROQUAKE ADDITION --START
	if (cls.demoplayback || ((cl.last_angle_time > host_time && !(in_attack.state & 3)) && pq_smoothcam.value)) // JPG - check for last_angle_time for smooth chasecam!
	{
		// interpolate the angles
		for (j = 0; j < 3; j++)
		{
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;

			// JPG - I can't set cl.viewangles anymore since that messes up the demorecording.  So instead,
			// I'll set lerpangles (new variable), and view.c will use that instead.
			cl.lerpangles[j] = cl.mviewangles[1][j] + frac * d;
			if (cls.demoplayback)
				cl.viewangles[j] = cl.mviewangles[1][j] + frac * d;
		}
	}
	else
		VectorCopy(cl.viewangles, cl.lerpangles);
	//PROQUAKE ADDITION --END



// start on the entity after the world
	for (i = 1, ent = cl_entities + 1; i < cl.num_entities; i++, ent++)
	{
		if (!ent->model)
		{	// empty slot
			if (ent->forcelink)
				R_RemoveEfrags(ent);	// just became empty
			continue;
		}

// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0])
		{
			CL_ClearInterpolation(ent);

			ent->model = NULL;
			continue;
		}

		VectorCopy(ent->origin, oldorg);

		if (ent->forcelink)
		{	// the entity was not updated in the last message 
			// so move to the final spot
			VectorCopy(ent->msg_origins[0], ent->origin);
			VectorCopy(ent->msg_angles[0], ent->angles);
		}
		else
		{	// if the delta is large, assume a teleport and don't lerp
			f = frac;
			for (j = 0; j < 3; j++)
			{
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
				{
					f = 1;		// assume a teleportation, not a motion
				}
			}

			if (f >= 1)
				CL_ClearInterpolation(ent);

			// interpolate the origin and angles
			for (j = 0; j < 3; j++)
			{
				ent->origin[j] = ent->msg_origins[1][j] + f * delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f * d;
			}

		}

		CL_EntityInterpolateOrigins(ent);
		CL_EntityInterpolateAngles(ent);

// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
		{
			float bobjrotate = anglemod(100 * cl.ctime);
			ent->angles[1] = bobjrotate;
			if (cl_bobbing.value)
				ent->origin[2] += sin(bobjrotate / 90 * M_PI) * 5 + 5;
		}
		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles(ent);

		if (ent->effects & EF_MUZZLEFLASH)
		{
			vec3_t fv, rv, uv;

			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->origin[2] += 16;
			AngleVectors(ent->angles, fv, rv, uv);

			VectorMA(dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand() & 31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
		}
		if (ent->effects & EF_BRIGHTLIGHT)
		{
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->origin[2] += 16;
			dl->radius = 400 + (rand() & 31);
			dl->die = cl.time + 0.001;
		}
		if (ent->effects & EF_DIMLIGHT)
		{
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->radius = 200 + (rand() & 31);
			dl->die = cl.time + 0.001;
		}

		if (ent->model->flags & EF_GIB)
			R_RocketTrail(oldorg, ent->origin, BLOOD_TRAIL);
		else if (ent->model->flags & EF_ZOMGIB)
			R_RocketTrail(oldorg, ent->origin, SLIGHT_BLOOD_TRAIL);
		else if (ent->model->flags & EF_TRACER)
			R_RocketTrail(oldorg, ent->origin, TRACER1_TRAIL);
		else if (ent->model->flags & EF_TRACER2)
			R_RocketTrail(oldorg, ent->origin, TRACER2_TRAIL);
		else if (ent->model->flags & EF_ROCKET)
		{
			R_RocketTrail(oldorg, ent->origin, ROCKET_TRAIL);
			dl = CL_AllocDlight(i);
			VectorCopy(ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.01;
		}
		else if (ent->model->flags & EF_GRENADE)
			R_RocketTrail(oldorg, ent->origin, GRENADE_TRAIL);
		else if (ent->model->flags & EF_TRACER3)
			R_RocketTrail(oldorg, ent->origin, VOOR_TRAIL);

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active.value)
			continue;

		if (cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
	}

}

/*
 ===============
 CL_ReadFromServer

 Read all incoming data from the server
 ===============
 */
int CL_ReadFromServer(void)
{
	int ret;

	// Baker 3.75 - demo rewind
	cl.oldtime = cl.ctime;
	cl.time += host_frametime;
	if (!cls.demorewind || !cls.demoplayback)	// by joe
		cl.ctime += host_frametime;
	else
		cl.ctime -= host_frametime;
	// Baker 3.75 - end demo fast rewind

	do
	{
		ret = CL_GetMessage();
		if (ret == -1)
			Host_Error("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = realtime;
		CL_ParseServerMessage();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet.value)
		Con_Printf("\n");

	CL_RelinkEntities();
	CL_UpdateTEnts();

//
// bring the links up to date
//
	return 0;
}

/*
 =================
 CL_SendCmd
 =================
 */
void CL_SendCmd(void)
{
	usercmd_t cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
		// get basic movement from keyboard
		CL_BaseMove(&cmd);

		// allow mice or other external controllers to add to the move
		IN_Move(&cmd);

		// send the unreliable message
		CL_SendMove(&cmd);

	}

	if (cls.demoplayback)
	{
		SZ_Clear(&cls.message);
		return;
	}

// send the reliable message
	if (!cls.message.cursize)
		return;		// no message at all

	if (!NET_CanSendMessage(cls.netcon))
	{
		Con_DPrintf("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage(cls.netcon, &cls.message) == -1)
		Host_Error("CL_WriteToServer: lost server connection");

	SZ_Clear(&cls.message);
}

/* Saves the Sensitivity */
static void CL_SaveSensitivity_f(void)
{
	savedsensitivity = sensitivity.value;
}

/* Restores Sensitivity to saved level */
static void CL_RestoreSensitivity_f(void)
{
	if (!savedsensitivity)
	{
		Con_Printf("RestoreSensitivity: No saved SENSITIVITY to restore\n");
		return;
	}

	Cvar_SetValueQuick(&sensitivity, savedsensitivity);
}

/* display client position and angles */
void CL_Viewpos_f(void)
{
	Con_Printf("You are at xyz = %i %i %i, angles: %i %i %i\n",
	           (int)cl_entities[cl.viewentity].origin[0],
	           (int)cl_entities[cl.viewentity].origin[1],
	           (int)cl_entities[cl.viewentity].origin[2],
	           (int)cl.viewangles[PITCH],
	           (int)cl.viewangles[YAW],
	           (int)cl.viewangles[ROLL]);
}

/* display camera position and angles */
void CL_Campos_f(void)
{
	Con_Printf("Viewpos: (%i %i %i) %i %i %i\n",
	           (int)r_refdef.vieworg[0],
	           (int)r_refdef.vieworg[1],
	           (int)r_refdef.vieworg[2],
	           (int)r_refdef.viewangles[PITCH],
	           (int)r_refdef.viewangles[YAW],
	           (int)r_refdef.viewangles[ROLL]);
}

void CL_Init(void)
{
	SZ_Alloc(&cls.message, 1024);

	CL_InitInput();
	CL_InitTEnts();

	Cvar_RegisterVariable(&cl_name);
	Cvar_RegisterVariable(&cl_color);
	Cvar_RegisterVariable(&cl_upspeed);
	Cvar_RegisterVariable(&cl_forwardspeed);
	Cvar_RegisterVariable(&cl_backspeed);
	Cvar_RegisterVariable(&cl_sidespeed);
	Cvar_RegisterVariable(&cl_movespeedkey);
	Cvar_RegisterVariable(&cl_yawspeed);
	Cvar_RegisterVariable(&cl_pitchspeed);
	Cvar_RegisterVariable(&cl_anglespeedkey);
	Cvar_RegisterVariable(&cl_shownet);
	Cvar_RegisterVariable(&cl_nolerp);
	Cvar_RegisterVariable(&lookspring);
	Cvar_RegisterVariable(&lookstrafe);
	Cvar_RegisterVariable(&sensitivity);
	Cvar_RegisterVariable(&freelook);

	Cvar_RegisterVariable(&cl_maxpitch);
	Cvar_RegisterVariable(&cl_minpitch);

	Cvar_RegisterVariable(&m_pitch);
	Cvar_RegisterVariable(&m_yaw);
	Cvar_RegisterVariable(&m_forward);
	Cvar_RegisterVariable(&m_side);

	Cvar_RegisterVariable(&cl_gameplayhack_monster_lerp); // Hacks!
	Cvar_RegisterVariable(&cl_bobbing);

	Cvar_RegisterVariable(&pq_moveup);
	Cvar_RegisterVariable(&pq_smoothcam);

	Cvar_RegisterVariable (&cfg_unbindall);

	Cmd_AddCommand("entities", CL_PrintEntities_f);
	Cmd_AddCommand("disconnect", CL_Disconnect_f);

	Cmd_AddCommand("record", CL_Record_f);
	Cmd_AddCommand("stop", CL_Stop_f);
	Cmd_AddCommand("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand("nextstartdemo", CL_PlayDemo_NextStartDemo_f);
	Cmd_AddCommand("timedemo", CL_TimeDemo_f);

	Cmd_AddCommand("savesensitivity", CL_SaveSensitivity_f);
	Cmd_AddCommand("restoresensitivity", CL_RestoreSensitivity_f);

	Cmd_AddCommand("viewpos", CL_Viewpos_f);
	Cmd_AddCommand("campos", CL_Campos_f);
}
