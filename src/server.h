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

#ifndef __SERVER_H
#define __SERVER_H

typedef struct
{
	int maxclients;
	int maxclientslimit;
	struct client_s *clients; // [maxclients]
	int serverflags; // episode completion information
	bool changelevel_issued; // cleared when at SV_SpawnServer
} server_static_t;

//=============================================================================

typedef enum
{
	ss_loading, ss_active
} server_state_t;

typedef struct
{
	bool active;				// false if only a net client

	bool paused;
	bool loadgame;			// handle connections specially

	double time;

	int lastcheck;			// used by PF_checkclient
	double lastchecktime;

	char name[64];			// map name
	char modelname[64];		// maps/<name>.bsp, for model_precache[0]
	struct model_s *worldmodel;
	const char *model_precache[MAX_MODELS];	// NULL terminated
	struct model_s *models[MAX_MODELS];
	const char *sound_precache[MAX_SOUNDS];	// NULL terminated
	const char *lightstyles[MAX_LIGHTSTYLES];
	int num_edicts;
	int max_edicts;
	edict_t *edicts;			// can NOT be array indexed, because
						// edict_t is variable sized, but can
						// be used to reference the world ent
	server_state_t state;			// some actions are only valid during load

	sizebuf_t datagram;
	byte datagram_buf[MAX_DATAGRAM];

	sizebuf_t reliable_datagram;	// copied to all clients at end of frame
	byte reliable_datagram_buf[MAX_DATAGRAM];

	sizebuf_t signon;
	byte signon_buf[8192];

	// JPG - DON'T MODIFY ANYTHING ABOVE THIS POINT (CRMOD 6.X COMPATIBIILITY)

	unsigned long model_crc[MAX_MODELS];	// JPG - model checking
} server_t;

#define	NUM_PING_TIMES		16
#define	NUM_SPAWN_PARMS		16

typedef struct client_s
{
	bool active;				// false = client is free
	bool spawned;			// false = don't send datagrams
	bool dropasap;			// has been told to go to another level
	bool privileged;			// can execute any host command
	bool sendsignon;			// only valid before spawned

	double last_message;		// reliable messages must be sent
					// periodically

	struct qsocket_s *netconnection;	// communications handle

	usercmd_t cmd;				// movement
	vec3_t wishdir;			// intended motion calced from cmd

	sizebuf_t message;			// can be added to at any time,
						// copied and clear once per frame
	byte msgbuf[MAX_MSGLEN];
	edict_t *edict;				// EDICT_NUM(clientnum+1)
	char name[32];			// for printing to other people
	int colors;

	float ping_times[NUM_PING_TIMES];
	int num_pings;			// ping_times[num_pings%NUM_PING_TIMES]

// spawn parms are carried from level to level
	float spawn_parms[NUM_SPAWN_PARMS];

// client known data for deltas	
	int old_frags;

	// JPG - DON'T MODIFY ANYTHING ABOVE THIS POINT (CRMOD 6.X COMPATIBIILITY)

	// JPG - added spam_time to prevent spamming.  If time < spam_time then
	// the client is spamming and is silenced.
	double spam_time;

	// JPG 3.00 - prevent clients from rapidly changing their name/colour
	//            and doing a say or say_team
	double change_time;

	// JPG 3.30 - allow clients to connect if they don't have the map
	bool nomap;
} client_t;

//=============================================================================

// edict->movetype values
#define	MOVETYPE_NONE           0 // never moves
#define	MOVETYPE_ANGLENOCLIP    1
#define	MOVETYPE_ANGLECLIP      2
#define	MOVETYPE_WALK           3 // gravity
#define	MOVETYPE_STEP           4 // gravity, special edge handling
#define	MOVETYPE_FLY            5
#define	MOVETYPE_TOSS           6 // gravity
#define	MOVETYPE_PUSH           7 // no clip to world, push and crush
#define	MOVETYPE_NOCLIP         8
#define	MOVETYPE_FLYMISSILE     9 // extra size to monsters
#define	MOVETYPE_BOUNCE         10

// edict->solid values
#define	SOLID_NOT       0 // no interaction with other objects
#define	SOLID_TRIGGER   1 // touch on edge, but not blocking
#define	SOLID_BBOX      2 // touch on edge, block
#define	SOLID_SLIDEBOX  3 // touch on edge, but not an onground
#define	SOLID_BSP       4 // bsp clip, touch on edge, block

// edict->deadflag values
#define	DEAD_NO	        0
#define	DEAD_DYING      1
#define	DEAD_DEAD       2

#define	DAMAGE_NO       0
#define	DAMAGE_YES      1
#define	DAMAGE_AIM      2

// edict->flags
#define	FL_FLY			1
#define	FL_SWIM                 2
#define	FL_CONVEYOR             4
#define	FL_CLIENT               8
#define	FL_INWATER              16
#define	FL_MONSTER              32
#define	FL_GODMODE              64
#define	FL_NOTARGET             128
#define	FL_ITEM                 256
#define	FL_ONGROUND             512
#define	FL_PARTIALGROUND        1024 // not all corners are valid
#define	FL_WATERJUMP            2048 // player jumping out of water
#define	FL_JUMPRELEASED         4096 // for jump debouncing
#define FL_LOW_BANDWIDTH_CLIENT 8192 // Baker 3.99b: Slot Zero's anti-lag server/mod option for dialup users

// entity effects

#define	EF_BRIGHTFIELD  1
#define	EF_MUZZLEFLASH  2
#define	EF_BRIGHTLIGHT  4
#define	EF_DIMLIGHT     8
#define	EF_NODRAW       16
#define	EF_BLUE         64
#define	EF_RED          128
#define EF_MAYBE_DRAW   32768   // Baker 3.99b: Slot Zero's anti-lag server/mod option for dialup users

#define	SPAWNFLAG_NOT_EASY              256
#define	SPAWNFLAG_NOT_MEDIUM            512
#define	SPAWNFLAG_NOT_HARD              1024
#define	SPAWNFLAG_NOT_DEATHMATCH        2048

//============================================================================

extern cvar_t teamplay;
extern cvar_t skill;
extern cvar_t deathmatch;
extern cvar_t coop;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t sv_ipmasking;
extern cvar_t sv_maxvelocity;
extern cvar_t sv_gravity;
extern cvar_t sv_nostep;
extern cvar_t sv_friction;
extern cvar_t sv_edgefriction;
extern cvar_t sv_stopspeed;
extern cvar_t sv_maxspeed;
extern cvar_t sv_accelerate;
extern cvar_t sv_idealpitchscale;
extern cvar_t sv_aim;
extern cvar_t sv_altnoclip; //johnfitz

extern cvar_t sv_cullentities;

extern server_static_t svs; // persistant server info
extern server_t sv; // local server

extern client_t *host_client;
extern double host_time;

extern edict_t *sv_player;

//===========================================================

/* sv_main.c */
void SV_Init(void);
void SV_StartParticle(vec3_t org, vec3_t dir, int color, int count);
void SV_StartSound(edict_t *entity, int channel, const char *sample, int volume, float attenuation);
void SV_CheckForNewClients(void);
void SV_ClearDatagram(void);
void SV_WriteClientdataToMessage(edict_t *ent, sizebuf_t *msg);
void SV_SendClientMessages(void);
int SV_ModelIndex(const char *name);
void SV_SaveSpawnparms(void);
void SV_SpawnServer(char *server);

/* sv_move.c */
bool SV_CheckBottom(edict_t *ent);
bool SV_movestep(edict_t *ent, vec3_t move, bool relink);
void SV_MoveToGoal(void);

/* sv_phys.c */
void SV_Physics(void);

/* sv_user.c */
void SV_SetIdealPitch(void);
void SV_RunClients(void);

/* host.c */
void SV_DropClient(bool crash);
void SV_ClientPrintf(const char *fmt, ...);
void SV_BroadcastPrintf(const char *fmt, ...);

#endif /* __SERVER_H */
