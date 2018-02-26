/*
 * Coordinates spawning and killing of local servers
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

char host_worldname[MAX_QPATH];

/*

 A server can always be started, even if the system started out as a client
 to a remote system.

 A client can NOT be started if the system started as a dedicated server.

 Memory is cleared / released when a server or client begins, not when they end.

 */

quakeparms_t host_parms;

bool host_initialized;		// true if into command execution

double host_frametime;
double host_time;
double realtime; // without any filtering or bounding
int host_framecount;

int host_hunklevel;

int minimum_memory;

client_t *host_client; // current client

jmp_buf host_abortserver;

byte *host_colormap;

cvar_t host_timescale = { "host_timescale", "0" }; // scale server time (for slow motion or fast-forward)

cvar_t host_sleep = { "host_sleep", "0" };

cvar_t sys_ticrate = { "sys_ticrate", "0.05", false, true };

cvar_t fraglimit = { "fraglimit", "0", false, true };
cvar_t timelimit = { "timelimit", "0", false, true };
cvar_t teamplay = { "teamplay", "0", false, true };

cvar_t samelevel = { "samelevel", "0" };
cvar_t noexit = { "noexit", "0", false, true };
cvar_t skill = { "skill", "1" };						// 0 - 3
cvar_t deathmatch = { "deathmatch", "0" };			// 0, 1, or 2
cvar_t coop = { "coop", "0" };			// 0 or 1

cvar_t pausable = { "pausable", "1" };

cvar_t developer = { "developer", "0" };

cvar_t temp1 = { "temp1", "0" };

void Host_WriteConfig_f(void);

cvar_t proquake = { "proquake", "L33T" }; // JPG - added this

// JPG - spam protection.  If a client's msg's/second exceeds spam rate
// for an extended period of time, the client is spamming.  Clients are
// allowed a temporary grace of pq_spam_grace messages.  Once used up,
// this grace regenerates while the client shuts up at a rate of one
// message per pq_spam_rate seconds.
cvar_t pq_spam_rate = { "pq_spam_rate", "0" };  // Baker 3.80x - Set to default of 0; was 1.5 -- bad for coop
cvar_t pq_spam_grace = { "pq_spam_grace", "999" }; // Baker 3.80x - Set to default of 999; was 10 -- bad for coop

// Baker 3.99g - from Rook ... protect against players connecting and spamming before banfile can kick in
cvar_t pq_connectmute = { "pq_connectmute", "0", false, true };  // (value in seconds)

// JPG 3.20 - control muting of players that change colour/name
cvar_t pq_tempmute = { "pq_tempmute", "0" };  // Baker 3.80x - Changed default to 0; was 1 -- interfered with coop

// JPG 3.20 - optionally write player binds to server log
cvar_t pq_logbinds = { "pq_logbinds", "0" };

// JPG 3.11 - feature request from Slot Zero
cvar_t pq_showedict = { "pq_showedict", "0" };

// JPG 1.05 - translate dedicated server console output to plain text
cvar_t pq_dequake = { "pq_dequake", "1" };
cvar_t pq_maxfps = { "pq_maxfps", "72.0", true };	// Baker 3.80x - save this to config

bool isDedicated = false;

/*
 ================
 Host_EndGame
 ================
 */
void Host_EndGame(const char *message, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, message);
	vsnprintf(string, sizeof(string), message, argptr);
	va_end(argptr);
	Con_DPrintf("Host_EndGame: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(false);

	if (cls.state == ca_dedicated)
		Sys_Error("%s\n", string);	// dedicated servers exit

	if (cls.demonum != -1)
	{
// Baker: I don't think this fixes anything
//		CL_StopPlayback ();	// JPG 1.05 - patch by CSR to fix crash
		CL_NextDemo();
	}
	else
		CL_Disconnect();

	longjmp(host_abortserver, 1);
}

/* This shuts down both the client and server */
void Host_Error(const char *error, ...)
{
	va_list argptr;
	char string[1024];
	static bool inerror = false;

	if (inerror)
		Sys_Error("recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque();		// reenable screen updates

	va_start(argptr, error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);
	Con_Printf("Host_Error: %s\n", string);

	if (sv.active)
		Host_ShutdownServer(false);

	if (cls.state == ca_dedicated)
		Sys_Error("%s\n", string);	// dedicated servers exit

	CL_Disconnect();
	cls.demonum = -1;
	cl.intermission = 0; //johnfitz -- for errors during intermissions (changelevel with no map found, etc.)

	inerror = false;

	longjmp(host_abortserver, 1);
}

static void Host_FindMaxClients(void)
{
	svs.maxclients = 1;

	int cmdline_dedicated = COM_CheckParm("-dedicated");
	if (cmdline_dedicated)
	{
		cls.state = ca_dedicated;
		if (cmdline_dedicated != (com_argc - 1))
			svs.maxclients = atoi(com_argv[cmdline_dedicated + 1]);
		else
			svs.maxclients = 8;		// Default for -dedicated with no command line
	}
	else
		cls.state = ca_disconnected;

	int cmdline_listen = COM_CheckParm("-listen");
	if (cmdline_listen)
	{
		if (cls.state == ca_dedicated)
			Sys_Error("Only one of -dedicated or -listen can be specified");
		if (cmdline_listen != (com_argc - 1))
			svs.maxclients = atoi(com_argv[cmdline_listen + 1]);
		else
			svs.maxclients = 8;
	}

	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	if (cmdline_dedicated || cmdline_listen)
	{
		// Baker: only do this if -dedicated or listen
		// So, why does this need done at all since all we are doing is an allocation below.
		// Well, here is why ....
		// we really want the -dedicated and -listen parameters to operate as expected and
		// not be ignored.  This limit shouldn't be able to be changed in game if specified.
		// But we don't want to hurt our ability to play against the bots either.
		svs.maxclientslimit = svs.maxclients;
	}
	else
		svs.maxclientslimit = 16; // Baker: the new default for using neither -listen or -dedicated

	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;
	svs.clients = (struct client_s *)Hunk_AllocName(svs.maxclientslimit * sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetValueQuick(&deathmatch, 1.0);
	else
		Cvar_SetValueQuick(&deathmatch, 0.0);
}

char dequake[256];	// JPG 1.05

/*
 =======================
 Host_InitDeQuake

 JPG 1.05 - initialize the dequake array
 ======================
 */
void Host_InitDeQuake(void)
{
	int i;

	for (i = 1; i < 12; i++)
		dequake[i] = '#';
	dequake[9] = 9;
	dequake[10] = 10;
	dequake[13] = 13;
	dequake[12] = ' ';
	dequake[1] = dequake[5] = dequake[14] = dequake[15] = dequake[28] = '.';
	dequake[16] = '[';
	dequake[17] = ']';
	for (i = 0; i < 10; i++)
		dequake[18 + i] = '0' + i;
	dequake[29] = '<';
	dequake[30] = '-';
	dequake[31] = '>';
	for (i = 32; i < 128; i++)
		dequake[i] = i;
	for (i = 0; i < 128; i++)
		dequake[i + 128] = dequake[i];
	dequake[128] = '(';
	dequake[129] = '=';
	dequake[130] = ')';
	dequake[131] = '*';
	dequake[141] = '>';
}

void Host_WriteConfig(const char *cfgname)
{
	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if (!host_initialized || isDedicated)
		return;

	FILE *f = fopen(va("%s/%s", com_gamedir, cfgname), "w");
	if (!f)
	{
		Con_Printf("Couldn't write %s\n", cfgname);
		return;
	}
	//VID_SyncCvars (); //johnfitz -- write actual current mode to config file, in case cvars were messed with

	fprintf(f, "// Generated by %s %s \n", ENGINE_NAME, ENGINE_VERSION);
	fprintf(f, "//\n");
	fprintf(f, "//\n");

	fprintf(f, "// Your maps folder:  %s\\maps\n", com_gamedir);
	fprintf(f, "// Your demos folder:  %s\n", com_gamedir);
	fprintf(f, "// Your screenshots folder:  %s\n", com_gamedir);
	fprintf(f, "//\n");

	fprintf(f, "\n// Key Bindings\n\n");
	Key_WriteBindings(f);

	fprintf(f, "\n// Variables\n\n");
	Cvar_WriteVariables(f);

	fclose(f);

	snprintf(cls.recent_file, sizeof(cls.recent_file), "%s/config.cfg", com_gamedir);
}

/* Writes key bindings and archived cvars to config.cfg */
void Host_WriteConfiguration(void)
{
	// dedicated servers initialize the host but don't parse and set the config.cfg cvars
	if (host_initialized && !isDedicated) // 1999-12-24 logical correction by Maddes
		Host_WriteConfig("config.cfg");
}

/*
 ===============
 Host_WriteConfig_f

 Writes key bindings and ONLY archived cvars to a custom config file
 ===============
 */
void Host_WriteConfig_f(void)
{
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf("Usage: writeconfig <filename>\n");
		return;
	}

	strlcpy(name, Cmd_Argv(1), sizeof(name));
	COM_ForceExtension(name, ".cfg");

	Con_Printf("Writing %s\n", name);

	Host_WriteConfig(name);
}

/*
 =================
 SV_ClientPrintf

 Sends text across to be displayed
 FIXME: make this just a stuffed echo?
 =================
 */
void SV_ClientPrintf(const char *fmt, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->message, svc_print);
	MSG_WriteString(&host_client->message, string);
}

/* Sends text to all active clients */
void SV_BroadcastPrintf(const char *fmt, ...)
{
	va_list argptr;
	char string[1024];
	int i;

	va_start(argptr, fmt);
	vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	for (i = 0; i < svs.maxclients; i++)
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			MSG_WriteByte(&svs.clients[i].message, svc_print);
			MSG_WriteString(&svs.clients[i].message, string);
		}
}

/* Send text over to the client to be executed */
void Host_ClientCommands(const char *fmt, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, fmt);
	vsnprintf(string, sizeof(string), fmt, argptr);
	va_end(argptr);

	MSG_WriteByte(&host_client->message, svc_stufftext);
	MSG_WriteString(&host_client->message, string);
}

/*
 * Called when the player is getting totally kicked off the host
 * if (crash = true), don't bother sending signofs
 */
void SV_DropClient(bool crash)
{
	int saveSelf;

	// don't drop a client that's already been dropped!
	if (!host_client->active)
		return;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage(host_client->netconnection))
		{
			MSG_WriteByte(&host_client->message, svc_disconnect);
			NET_SendMessage(host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned)
		{
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram(pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf("Client %s removed\n", host_client->name);
	}

	// break the net connection
	NET_Close(host_client->netconnection);
	host_client->netconnection = NULL;

	// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

	// send notification to all clients
	for (int i = 0; i < svs.maxclients; i++)
	{
		if (!svs.clients[i].active)
			continue;
		MSG_WriteByte(&svs.clients[i].message, svc_updatename);
		MSG_WriteByte(&svs.clients[i].message, host_client - svs.clients);
		MSG_WriteString(&svs.clients[i].message, "");
		MSG_WriteByte(&svs.clients[i].message, svc_updatefrags);
		MSG_WriteByte(&svs.clients[i].message, host_client - svs.clients);
		MSG_WriteShort(&svs.clients[i].message, 0);
		MSG_WriteByte(&svs.clients[i].message, svc_updatecolors);
		MSG_WriteByte(&svs.clients[i].message, host_client - svs.clients);
		MSG_WriteByte(&svs.clients[i].message, 0);
	}
}

/* This only happens at the end of a game, not between levels */
void Host_ShutdownServer(bool crash)
{
	if (!sv.active)
		return;

	sv.active = false;

	if (cls.state == ca_connected)
		CL_Disconnect();

	// flush any pending messages - like the score!!!
	int count;
	double start = Sys_DoubleTime();
	do
	{
		count = 0;
		for (int i = 0; i < svs.maxclients; i++)
		{
			if (svs.clients[i].active && svs.clients[i].message.cursize)
			{
				if (NET_CanSendMessage(svs.clients[i].netconnection))
				{
					NET_SendMessage(svs.clients[i].netconnection, &svs.clients[i].message);
					SZ_Clear(&svs.clients[i].message);
				}
				else
				{
					NET_GetMessage(svs.clients[i].netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime() - start) > 3.0)
			break;
	} while (count);

	// make sure all the clients know we're disconnecting
	sizebuf_t buf;
	byte message[4];
	buf.data = message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (int i = 0; i < svs.maxclients; i++)
		if (svs.clients[i].active)
		{
			// FIXME: make host_client less global
			host_client = &svs.clients[i];
			SV_DropClient(crash);
		}

	// clear structures
	memset(&sv, 0, sizeof(sv));
	memset(svs.clients, 0, svs.maxclientslimit * sizeof(client_t));
}

/*
 * This clears all the memory used by both the client and server, but does
 * not reinitialize anything
 */
void Host_ClearMemory(void)
{
	Con_DPrintf("Clearing memory\n");
	Mod_ClearAll();
	if (host_hunklevel)
		Hunk_FreeToLowMark(host_hunklevel);

	cls.signon = 0;
	memset(&sv, 0, sizeof(sv));
	memset(&cl, 0, sizeof(cl));
}

//==============================================================================
//
// Host Frame
//
//==============================================================================

/* Returns false if the time is too short to run a frame */
static bool Host_FilterTime(double time)
{
	static double oldrealtime;
	double time_delta = realtime - oldrealtime;
	double fps = max(10, pq_maxfps.value);
	double time_per_frame = 1.0 / fps;

	if (!cls.capturedemo &&
	    !cls.timedemo &&
	    time_delta < time_per_frame)
	{
		if (host_sleep.value)
			Sys_Sleep(1); // Lower cpu

		return false; // not ready for host frame
	}

	host_frametime = time_delta;
	if (cls.demoplayback && cls.demospeed)
		host_frametime *= CLAMP(0, cls.demospeed, 20);
	oldrealtime = realtime;

	if (host_timescale.value > 0)
		host_frametime *= host_timescale.value;
	else
		// don't allow really long or short frames
		host_frametime = CLAMP(0.001, host_frametime, 0.1);

	return true;
}

/* Add them exactly as if they had been typed at the console */
void Host_GetConsoleCommands(void)
{
	char *cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput();
		if (!cmd)
			break;
		Cbuf_AddText(cmd);
	}
}

void Host_ServerFrame(void)
{
	// run the world state
	pr_global_struct->frametime = host_frametime;

	// set the time and clear the general datagram
	SV_ClearDatagram();

	// check for new clients
	SV_CheckForNewClients();

	// read client messages
	SV_RunClients();

	// move things around and think
	// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
		SV_Physics();

	// send all messages to the clients
	SV_SendClientMessages();
}

void Host_Frame(double time)
{
	if (setjmp(host_abortserver))
		return; // something bad happened, or the server disconnected

	// update time
	realtime += time;

	// decide the simulation time
	if (!Host_FilterTime(time))
	{
		// if we're not doing a frame, still check for lagged moves to send
		if (!sv.active && (cl.movemessages > 2))
			CL_SendLagMove();
		return; // don't run too fast, or packets will flood out
	}

	// keep the random time dependent
	rand();

	// get new key events
	Key_UpdateForDest();
	IN_UpdateInputMode();
	IN_SendKeyEvents();

	// polled controllers to add commands
	IN_Commands();

	// process console commands
	Cbuf_Execute();

	NET_Poll();

	// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd(); // This is where mouse input is read

	//-------------------
	// server operations
	//-------------------

	// check for commands typed to the host
	Host_GetConsoleCommands();

	if (sv.active)
		Host_ServerFrame();

	//-------------------
	// client operations
	//-------------------

	// if running the server remotely, send intentions now after
	// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd();

	host_time += host_frametime;

	// fetch results from server
	if (cls.state == ca_connected)
		CL_ReadFromServer();

	// update video
	SCR_UpdateScreen();

	// update audio
	if (cls.signon == SIGNONS)
		S_Update(r_origin, vpn, vright, vup);
	else
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	host_framecount++;
}

void Host_InitLocal(void)
{
	Host_InitCommands();

	Cvar_RegisterVariable(&host_timescale); //johnfitz

	Cvar_RegisterVariable(&host_sleep);

	Cvar_RegisterVariable(&sys_ticrate);

	Cvar_RegisterVariable(&fraglimit);
	Cvar_RegisterVariable(&timelimit);
	Cvar_RegisterVariable(&teamplay);
	Cvar_RegisterVariable(&samelevel);
	Cvar_RegisterVariable(&noexit);
	Cvar_RegisterVariable(&skill);
	Cvar_RegisterVariable(&developer);
	Cvar_RegisterVariable(&deathmatch);
	Cvar_RegisterVariable(&coop);

	Cvar_RegisterVariable(&pausable);

	Cvar_RegisterVariable(&temp1);

	Cmd_AddCommand("writeconfig", Host_WriteConfig_f);	// by joe

	Cvar_RegisterVariable(&proquake);		// JPG - added this so QuakeC can find it
	Cvar_RegisterVariable(&pq_spam_rate);	// JPG - spam protection
	Cvar_RegisterVariable(&pq_spam_grace);	// JPG - spam protection
	Cvar_RegisterVariable(&pq_connectmute);	// Baker 3.99g: from Rook, protection against repeatedly connecting + spamming
	Cvar_RegisterVariable(&pq_tempmute);	// JPG 3.20 - temporary muting
	Cvar_RegisterVariable(&pq_showedict);	// JPG 3.11 - feature request from Slot Zero
	Cvar_RegisterVariable(&pq_dequake);	// JPG 1.05 - translate dedicated console output to plain text
	Cvar_RegisterVariable(&pq_maxfps);		// JPG 1.05
	Cvar_RegisterVariable(&pq_logbinds);	// JPG 3.20 - log player binds

	Host_FindMaxClients();

	host_time = 1.0;		// so a think at time 0 won't get called

	Host_InitDeQuake();	// JPG 1.05 - initialize dequake array
}



//============================================================================

extern FILE *vcrFile;
#define	VCR_SIGNATURE 0x56435231 // "VCR1"

void Host_InitVCR(quakeparms_t *parms)
{
	int i, len, n;
	char *p;

	if (COM_CheckParm("-playback"))
	{
		if (com_argc != 2)
			Sys_Error("No other parameters allowed with -playback\n");

		vcrFile = Sys_FileOpenRead("quake.vcr");
		if (!vcrFile)
			Sys_Error("playback file not found\n");

		Sys_FileRead(vcrFile, &i, sizeof(int));
		if (i != VCR_SIGNATURE)
			Sys_Error("Invalid signature in vcr file\n");

		Sys_FileRead(vcrFile, &com_argc, sizeof(int));
		com_argv = (char **)Q_malloc(com_argc * sizeof(char *));
		com_argv[0] = parms->argv[0];
		for (i = 0; i < com_argc; i++)
		{
			Sys_FileRead(vcrFile, &len, sizeof(int));
			p = (char *)Q_malloc(len);
			Sys_FileRead(vcrFile, p, len);
			com_argv[i + 1] = p;
		}
		com_argc++; /* add one for arg[0] */
		parms->argc = com_argc;
		parms->argv = com_argv;
	}

	if ((n = COM_CheckParm("-record")) != 0)
	{
		vcrFile = Sys_FileOpenWrite("quake.vcr");

		i = VCR_SIGNATURE;
		Sys_FileWrite(vcrFile, &i, sizeof(int));
		i = com_argc - 1;
		Sys_FileWrite(vcrFile, &i, sizeof(int));
		for (i = 1; i < com_argc; i++)
		{
			if (i == n)
			{
				len = 10;
				Sys_FileWrite(vcrFile, &len, sizeof(int));
				Sys_FileWrite(vcrFile, (void *)"-playback", len);
				continue;
			}
			len = strlen(com_argv[i]) + 1;
			Sys_FileWrite(vcrFile, &len, sizeof(int));
			Sys_FileWrite(vcrFile, com_argv[i], len);
		}
	}

}

void Host_Init(quakeparms_t *parms)
{
	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else
		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm("-minmemory"))
		parms->memsize = minimum_memory;

	host_parms = *parms;

	if (parms->memsize < minimum_memory)
		Sys_Error("Only %4.1f megs of memory available, can't execute game", parms->memsize / (float) 0x100000);

	com_argc = parms->argc;
	com_argv = parms->argv;

	Memory_Init(parms->membase, parms->memsize);
	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	V_Init();
	Chase_Init();
	Host_InitVCR(parms);
	COM_Init(parms->basedir);
	Host_InitLocal();
	W_LoadWadFile("gfx.wad");
	Key_Init();
	Con_Init();
	M_Init();
	PR_Init();
	Mod_Init();
	NET_Init();
	SV_Init();

	Con_Printf("Exe: " __TIME__ " " __DATE__ "\n");
	Con_Printf("%4.1f megabyte heap\n", parms->memsize / (1024 * 1024.0));

	if (cls.state != ca_dedicated)
	{
		host_colormap = (byte *) COM_LoadHunkFile("gfx/colormap.lmp");
		if (!host_colormap)
			Sys_Error("Couldn't load gfx/colormap.lmp");

		VID_Init();
		GL_Init();
		TexMgr_LoadPalette();
		IN_Init();
		TexMgr_Init();
		R_Init();
		Draw_Init();
		SCR_Init();
		S_Init();
		Sbar_Init();
		CL_Init();
	}

	Cbuf_InsertText("exec quake.rc\n");

	Hunk_AllocName(0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark();

	host_initialized = true;

	Con_Printf("Host Initialized\n");
	Sys_Printf("========Quake Initialized=========\n");
}

/*
 ===============
 Host_Shutdown

 FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
 to run quit through here before the final handoff to the sys code.
 ===============
 */
void Host_Shutdown(void)
{
	static bool isdown = false;

	if (isdown)
	{
		printf("recursive shutdown\n");
		return;
	}
	isdown = true;

	// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration();

	if (con_initialized)
		History_Shutdown();

	NET_Shutdown();
	S_Shutdown();
	IN_Shutdown();

	if (cls.state != ca_dedicated)
	{
		VID_Shutdown();
	}
}
