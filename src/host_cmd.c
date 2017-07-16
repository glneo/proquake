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

extern cvar_t pausable;

cvar_t cl_confirmquit = { "cl_confirmquit", "1", true }; // Baker 3.60

// JPG - added these for spam protection
extern cvar_t pq_spam_rate;
extern cvar_t pq_spam_grace;
extern cvar_t pq_connectmute; // Baker 3.99f: from Rook

// JPG 3.20 - control muting of players that change colour/name
extern cvar_t pq_tempmute;

// JPG - feature request from Slot
extern cvar_t pq_showedict;

// JPG 3.20 - optionally remove '\r'
extern cvar_t pq_removecr;

// JPG 3.20 - optionally write player binds to server log
extern cvar_t pq_logbinds;

int current_skill;

void Mod_Print(void);
extern void M_Menu_Quit_f(void);

// The "just get out" version
void Host_Quit(void)
{
	CL_Disconnect();
	Host_ShutdownServer(false);

	Sys_Quit();
}

void Host_Quit_f(void)
{
	if (cl_confirmquit.value)
	{
		if ((key_dest != key_console && !con_forcedup) && cls.state != ca_dedicated)
		{
			M_Menu_Quit_f();
			return;
		}
	}

	Host_Quit();
}

//==============================================================================
//johnfitz -- dynamic gamedir stuff
//==============================================================================

// Declarations shared with common.c:
typedef struct
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char filename[MAX_OSPATH];
	int handle;
	int numfiles;
	packfile_t *files;
} pack_t;

typedef struct searchpath_s
{
	char filename[MAX_OSPATH];
	pack_t *pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

extern bool com_modified;
extern searchpath_t *com_searchpaths;
pack_t *COM_LoadPackFile(char *packfile);

// Kill all the search packs until the game path is found. Kill it, then return
// the next path to it.
void KillGameDir(searchpath_t *search)
{
	searchpath_t *search_killer;
	while (search)
	{
		if (*search->filename)
		{
			com_searchpaths = search->next;
			Z_Free(search);
			return; //once you hit the dir, youve already freed the paks
		}
		Sys_FileClose(search->pack->handle); //johnfitz
		search_killer = search->next;
		Z_Free(search->pack->files);
		Z_Free(search->pack);
		Z_Free(search);
		search = search_killer;
	}
}

// Return the number of games in memory
int NumGames(searchpath_t *search)
{
	int found = 0;
	while (search)
	{
		if (*search->filename)
			found++;
		search = search->next;
	}
	return found;
}

/*
 ==================
 Host_Game_f
 ==================
 */
void Host_Game_f(void)
{
	int i;
	searchpath_t *search = com_searchpaths;
	pack_t *pak;
	char pakfile[MAX_OSPATH]; //FIXME: it's confusing to use this string for two different things

	if (Cmd_Argc() > 1)
	{

		if (!registered.value) //disable command for shareware quake
		{
			Con_Printf("You must have the registered version to use modified games\n");
			return;
		}

		if (strstr(Cmd_Argv(1), ".."))
		{
			Con_Printf("Relative pathnames are not allowed.\n");
			return;
		}

		strcpy(pakfile, va("%s/%s", host_parms.basedir, Cmd_Argv(1)));
		if (!strcasecmp(pakfile, com_gamedir)) //no change
		{
			Con_Printf("\"game\" is already \"%s\"\n", COM_SkipPath(com_gamedir));
			return;
		}

		com_modified = true;

		//Kill the server
		CL_Disconnect();
		Host_ShutdownServer(true);

		//Write config file
		Host_WriteConfiguration();

		//Kill the extra game if it is loaded
		if (NumGames(com_searchpaths) > 1)
			KillGameDir(com_searchpaths);

		strcpy(com_gamedir, pakfile);

		if (strcasecmp(Cmd_Argv(1), GAMENAME)) //game is not id1
		{
			search = Z_Malloc(sizeof(searchpath_t));
			strcpy(search->filename, pakfile);
			search->next = com_searchpaths;
			com_searchpaths = search;

			//Load the paks if any are found:
			for (i = 0;; i++)
			{
				snprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", com_gamedir, i);
				pak = COM_LoadPackFile(pakfile);
				if (!pak)
					break;
				search = Z_Malloc(sizeof(searchpath_t));
				search->pack = pak;
				search->next = com_searchpaths;
				com_searchpaths = search;
			}
		}

		//clear out and reload appropriate data
		Cache_Flush_f();
		/*if (!isDedicated)
		 {
		 W_LoadWadFile ("gfx.wad");  // Baker 3.78 - I'm not so sure about this
		 Draw_Init(); // Baker 3.78 - I'm not so sure about this
		 Draw_ConsoleBackground (vid.height); // Baker 3.78 - I'm not so sure about this
		 }*/
		//ExtraMaps_NewGame ();
		//Cbuf_InsertText ("exec quake.rc\n");
		cls.recent_file[0] = 0;

		Con_Printf("\"gamedir\" changed to \"%s\"\n", COM_SkipPath(com_gamedir));
	}
	else
		//Diplay the current gamedir
		Con_Printf("\"gamedir\" is \"%s\"\n", COM_SkipPath(com_gamedir));
}

//==============================================================================
//johnfitz -- modlist management
//==============================================================================

typedef struct mod_s
{
	char name[MAX_OSPATH];
	struct mod_s *next;
} mod_t;

mod_t *modlist;

void Modlist_Add(char *name)
{
	mod_t *mod, *cursor, *prev;

	//ingore duplicate
	for (mod = modlist; mod; mod = mod->next)
		if (!strcmp(name, mod->name))
			return;

	mod = Z_Malloc(sizeof(mod_t));
	strcpy(mod->name, name);

	//insert each entry in alphabetical order
	if (modlist == NULL || strcasecmp(mod->name, modlist->name) < 0) //insert at front
	{
		mod->next = modlist;
		modlist = mod;
	}
	else //insert later
	{
		prev = modlist;
		cursor = modlist->next;
		while (cursor && (strcasecmp(mod->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		mod->next = prev->next;
		prev->next = mod;
	}
}

/*
 ==================
 Host_Mods_f -- johnfitz

 list all potential mod directories (contain either a pak file or a progs.dat)
 ==================
 *//*
 void Host_Mods_f (void)
 {
 int i;
 mod_t	*mod;

 for (mod = modlist, i=0; mod; mod = mod->next, i++)
 Con_SafePrintf ("   %s\n", mod->name);

 if (i)
 Con_SafePrintf ("%i mod(s)\n", i);
 else
 Con_SafePrintf ("no mods found\n");
 } */

//==============================================================================
/*
 =============
 Host_Mapname_f -- johnfitz
 =============
 */
void Host_Mapname_f(void)
{
	char name[MAX_QPATH];

	if (sv.active)
	{
		COM_StripExtension(sv.worldmodel->name + 5, name);
		Con_Printf("\"mapname\" is \"%s\"\n", name);
		return;
	}

	if (cls.state == ca_connected)
	{
		COM_StripExtension(cl.worldmodel->name + 5, name);
		Con_Printf("\"mapname\" is \"%s\"\n", name);
		return;
	}

	Con_Printf("no map loaded\n");
}

void Host_Status_f(void)
{
	client_t *client;
	int seconds;
	int minutes;
	int hours = 0;
	int j, a, b, c; // Baker 3.60 - a,b,c added for IP
	void (*print)(const char *fmt, ...);

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			cl.console_status = true;	// JPG 1.05 - added this;
			Cmd_ForwardToServer_f();
			return;
		}
		print = Con_Printf;
	}
	else
		print = SV_ClientPrintf;

	print("host:    %s (anti-wallhack %s)\n", Cvar_VariableString(hostname.name), sv_cullentities.value ? "on [mode: players]" : "off");
	print("version: %s %4.2f\n", ENGINE_NAME, PROQUAKE_SERIES_VERSION);
	if (tcpipAvailable)
		print("tcp/ip:  %s\n", my_tcpip_address);
	print("map:     %s\n", sv.name);
	print("players: %i active (%i max)\n\n", net_activeconnections, svs.maxclients);
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active)
			continue;
		seconds = (int) (net_time - client->netconnection->connecttime);
		minutes = seconds / 60;
		if (minutes)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;
			if (hours)
				minutes -= (hours * 60);
		}
		else
			hours = 0;
		print("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n", j + 1, client->name, (int) client->edict->v.frags, hours, minutes, seconds);

		if (cmd_source != src_command && sscanf(client->netconnection->address, "%d.%d.%d", &a, &b, &c) == 3 && sv_ipmasking.value) // Baker 3.60 - engine side ip masking from RocketGuy's ProQuake-r
			print("   %d.%d.%d.xxx\n", a, b, c);  // Baker 3.60 - engine side ip masking from RocketGuy's ProQuake-r
		else
			// Baker 3.60 - engine side ip masking from RocketGuy's ProQuake-r
			print("   %s\n", client->netconnection->address);
	}
}

/*
 ==================
 Host_QC_Exec

 Execute QC commands from the console
 ==================
 */
void Host_QC_Exec(void)
{
	dfunction_t *ED_FindFunction(char *name);
	dfunction_t *f;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (!sv.active)
	{
		Con_Printf("Not running local game\n");
		return;
	};

	if (!developer.value)
	{
		Con_Printf("Only available in developer mode\n");
		return;
	};

	f = 0;
	if ((f = ED_FindFunction(Cmd_Argv(1))) != NULL)
	{

		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram((func_t) (f - pr_functions));
	}
	else
		Con_Printf("bad function\n");

}

/*
 ==================
 Host_God_f

 Sets client to godmode
 ==================
 */
void Host_God_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set god mode to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int) sv_player->v.flags ^ FL_GODMODE;
		if (!((int) sv_player->v.flags & FL_GODMODE))
			SV_ClientPrintf("godmode OFF\n");
		else
			SV_ClientPrintf("godmode ON\n");
		break;
	case 2:
		if (atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int) sv_player->v.flags | FL_GODMODE;
			SV_ClientPrintf("godmode ON\n");
		}
		else
		{
			sv_player->v.flags = (int) sv_player->v.flags & ~FL_GODMODE;
			SV_ClientPrintf("godmode OFF\n");
		}
		break;
	default:
		Con_Printf("god [value] : toggle god mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

void Host_Notarget_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set notarget to on or off
	switch (Cmd_Argc())
	{
	case 1:
		sv_player->v.flags = (int) sv_player->v.flags ^ FL_NOTARGET;
		if (!((int) sv_player->v.flags & FL_NOTARGET))
			SV_ClientPrintf("notarget OFF\n");
		else
			SV_ClientPrintf("notarget ON\n");
		break;
	case 2:
		if (atof(Cmd_Argv(1)))
		{
			sv_player->v.flags = (int) sv_player->v.flags | FL_NOTARGET;
			SV_ClientPrintf("notarget ON\n");
		}
		else
		{
			sv_player->v.flags = (int) sv_player->v.flags & ~FL_NOTARGET;
			SV_ClientPrintf("notarget OFF\n");
		}
		break;
	default:
		Con_Printf("notarget [value] : toggle notarget mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

void Host_Noclip_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_NOCLIP)
		{
			cl.noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf("noclip ON\n");
		}
		else
		{
			cl.noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf("noclip OFF\n");
		}
		break;
	case 2:
		if (atof(Cmd_Argv(1)))
		{
			cl.noclip_anglehack = true;
			sv_player->v.movetype = MOVETYPE_NOCLIP;
			SV_ClientPrintf("noclip ON\n");
		}
		else
		{
			cl.noclip_anglehack = false;
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf("noclip OFF\n");
		}
		break;
	default:
		Con_Printf("noclip [value] : toggle noclip mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

/*
 ==================
 Host_Fly_f

 Sets client to flymode
 ==================
 */
void Host_Fly_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	//johnfitz -- allow user to explicitly set noclip to on or off
	switch (Cmd_Argc())
	{
	case 1:
		if (sv_player->v.movetype != MOVETYPE_FLY)
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf("flymode OFF\n");
		}
		break;
	case 2:
		if (atof(Cmd_Argv(1)))
		{
			sv_player->v.movetype = MOVETYPE_FLY;
			SV_ClientPrintf("flymode ON\n");
		}
		else
		{
			sv_player->v.movetype = MOVETYPE_WALK;
			SV_ClientPrintf("flymode OFF\n");
		}
		break;
	default:
		Con_Printf("fly [value] : toggle fly mode. values: 0 = off, 1 = on\n");
		break;
	}
	//johnfitz
}

void Host_Ping_f(void)
{
	int i, j;
	float total;
	client_t *client;
	char *n;	// JPG - for ping +N

	if (cmd_source == src_command)
	{
		// JPG - check for ping +N
		if (Cmd_Argc() == 2)
		{
			if (cls.state != ca_connected)
				return;

			n = Cmd_Argv(1);
			if (*n == '+')
			{
				Cvar_SetQuick(&pq_lag, n + 1);
				return;
			}
		}
		cl.console_ping = true;		// JPG 1.05 - added this

		Cmd_ForwardToServer_f();
		return;
	}

	SV_ClientPrintf("Client ping times:\n");
	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		if (!client->active)
			continue;
		total = 0;
		for (j = 0; j < NUM_PING_TIMES; j++)
			total += client->ping_times[j];
		total /= NUM_PING_TIMES;
		SV_ClientPrintf("%4i %s\n", (int) (total * 1000), client->name);
	}
}

/*
 ===============================================================================

 SERVER TRANSITIONS

 ===============================================================================
 */

/*
 ======================
 Host_Map_f

 handle a
 map <servername>
 command from the console.  Active clients are kicked off.
 ======================
 */
void Host_Map_f(void)
{
	int i;
	char name[MAX_QPATH];

	if (cmd_source != src_command)
		return;

	CL_Clear_Demos_Queue(); // "map" is a very intentional action so clear demos queue

	CL_Disconnect();
	Host_ShutdownServer(false);

	key_dest = key_game;			// remove console or menu
	SCR_BeginLoadingPlaque();

	cls.mapstring[0] = 0;
	for (i = 0; i < Cmd_Argc(); i++)
	{
		strlcat(cls.mapstring, Cmd_Argv(i), sizeof(cls.mapstring));
		strlcat(cls.mapstring, " ", sizeof(cls.mapstring));
	}
	strlcat(cls.mapstring, "\n", sizeof(cls.mapstring));

	svs.serverflags = 0;			// haven't completed an episode yet
	strcpy(name, Cmd_Argv(1));
	SV_SpawnServer(name);
	if (!sv.active)
		return;

	if (cls.state != ca_dedicated)
	{
		strcpy(cls.spawnparms, "");

		for (i = 2; i < Cmd_Argc(); i++)
		{
			strlcat(cls.spawnparms, Cmd_Argv(i), sizeof(cls.spawnparms));
			strlcat(cls.spawnparms, " ", sizeof(cls.spawnparms));
		}

		Cmd_ExecuteString("connect local", src_command);
	}
}

/*
 ==================
 Host_Changelevel_f

 Goes to a new map, taking all clients along
 ==================
 */
void Host_Changelevel_f(void)
{
	char level[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Con_Printf("changelevel <levelname> : continue game on a new level\n");
		return;
	}
	if (!sv.active || cls.demoplayback)
	{
		Con_Printf("Only the server may changelevel\n");
		return;
	}

	//johnfitz

	SV_SaveSpawnparms();
	strcpy(level, Cmd_Argv(1));
	SV_SpawnServer(level);
}

/*
 ==================
 Host_Restart_f

 Restarts the current server for a dead player
 ==================
 */
void Host_Restart_f(void)
{
	char mapname[MAX_QPATH];

	if (cls.demoplayback || !sv.active)
		return;

	if (cmd_source != src_command)
		return;

	// must copy out, because it gets cleared in sv_spawnserver
	strlcpy(mapname, sv.name, sizeof(mapname));

	SV_SpawnServer(mapname);
}

/*
 ==================
 Host_Reconnect_f

 This command causes the client to wait for the signon messages again.
 This is sent just before a server changes levels
 ==================
 */
void Host_Reconnect_f(void)
{
	if (cls.demoplayback)  // Multimap demo playback
	{
		Con_DPrintf("Demo playing; ignoring reconnect\n");
		return;
	}

	SCR_BeginLoadingPlaque();
	cls.signon = 0;		// need new connection messages
}

extern char server_name[MAX_QPATH];	// JPG 3.50

/*
 =====================
 Host_Connect_f

 User command to connect to server
 =====================
 */
void Host_Connect_f(void)
{
	char name[MAX_QPATH];

	cls.demonum = -1;		// stop demo loop in case this fails
	if (cls.demoplayback)
	{
		CL_StopPlayback();
		CL_Disconnect();
	}
	strcpy(name, Cmd_Argv(1));
	CL_EstablishConnection(name);
	Host_Reconnect_f();

	strlcpy(server_name, name, sizeof(server_name));	// JPG 3.50
}

/*
 ===============================================================================

 LOAD / SAVE GAME

 ===============================================================================
 */

#define	SAVEGAME_VERSION	5

/*
 ===============
 Host_SavegameComment

 Writes a SAVEGAME_COMMENT_LENGTH character comment describing the current
 ===============
 */
void Host_SavegameComment(char *text)
{
	int i;
	char kills[20];

	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		text[i] = ' ';
	memcpy(text, cl.levelname, min(strlen(cl.levelname), 22)); //johnfitz -- only copy 22 chars.
	snprintf(kills, sizeof(kills), "kills:%3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	memcpy(text + 22, kills, strlen(kills));
// convert space to _ to make stdio happy
	for (i = 0; i < SAVEGAME_COMMENT_LENGTH; i++)
		if (text[i] == ' ')
			text[i] = '_';
	text[SAVEGAME_COMMENT_LENGTH] = '\0';
}

void Host_Savegame_f(void)
{
	char name[256];
	FILE *f;
	int i;
	char comment[SAVEGAME_COMMENT_LENGTH + 1];

	if (cmd_source != src_command)
		return;

	if (!sv.active)
	{
		Con_Printf("Not playing a local game.\n");
		return;
	}

	if (cl.intermission)
	{
		Con_Printf("Can't save in intermission.\n");
		return;
	}

	if (svs.maxclients != 1)
	{
		Con_Printf("Can't save multiplayer games.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Con_Printf("save <savename> : save a game\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf("Relative pathnames are not allowed.\n");
		return;
	}

	for (i = 0; i < svs.maxclients; i++)
	{
		if (svs.clients[i].active && (svs.clients[i].edict->v.health <= 0))
		{
			Con_Printf("Can't savegame with a dead player\n");
			return;
		}
	}

	snprintf(name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_ForceExtension(name, ".sav"); // joe: force to ".sav"

	Con_Printf("Saving game to %s...\n", name);
	f = fopen(name, "w");

	if (!f)
	{
		Con_Printf("ERROR: couldn't open save file for writing.\n");
		return;
	}

	fprintf(f, "%i\n", SAVEGAME_VERSION);
	Host_SavegameComment(comment);
	fprintf(f, "%s\n", comment);
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		fprintf(f, "%f\n", svs.clients->spawn_parms[i]);
	fprintf(f, "%d\n", current_skill);
	fprintf(f, "%s\n", sv.name);
	fprintf(f, "%f\n", sv.time);

	// write the light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (sv.lightstyles[i])
			fprintf(f, "%s\n", sv.lightstyles[i]);
		else
			fprintf(f, "m\n");
	}

	ED_WriteGlobals(f);
	for (i = 0; i < sv.num_edicts; i++)
		ED_Write(f, EDICT_NUM(i));

	fclose(f);
	Con_Printf("done.\n");
}

void Host_Loadgame_f(void)
{
	char name[MAX_OSPATH];
	FILE *f;
	char mapname[MAX_QPATH];
	float time, tfloat;
	char str[32768], *start;
	int i, r;
	edict_t *ent;
	int entnum;
	int version;
	float spawn_parms[NUM_SPAWN_PARMS];

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("load <savename> : load a game\n");
		return;
	}

	cls.demonum = -1;		// stop demo loop in case this fails

	snprintf(name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_DefaultExtension(name, ".sav");

// we can't call SCR_BeginLoadingPlaque, because too much stack space has
// been used.  The menu calls it before stuffing loadgame command
//	SCR_BeginLoadingPlaque ();

	Con_Printf("Loading game from %s...\n", name);
	f = fopen(name, "r");
	if (!f)
	{
		Con_Printf("ERROR: couldn't open save file for reading.\n");
		return;
	}

	if (fscanf(f, "%i\n", &version) < 1)
		Con_Printf("ERROR: couldn't read from file.\n");
	if (version != SAVEGAME_VERSION)
	{
		fclose(f);
		Con_Printf("Savegame is version %i, not %i\n", version, SAVEGAME_VERSION);
		return;
	}
	if (fscanf(f, "%s\n", str) < 1)
		Con_Printf("ERROR: couldn't read from file.\n");
	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		if (fscanf(f, "%f\n", &spawn_parms[i]) < 1)
			Con_Printf("ERROR: couldn't read from file.\n");
// this silliness is so we can load 1.06 save files, which have float skill values
	if (fscanf(f, "%f\n", &tfloat) < 1)
		Con_Printf("ERROR: couldn't read from file.\n");
	current_skill = (int) (tfloat + 0.1);
	Cvar_SetValueQuick(&skill, (float) current_skill);

	if (fscanf(f, "%s\n", mapname) < 1)
		Con_Printf("ERROR: couldn't read from file.\n");
	if (fscanf(f, "%f\n", &time) < 1)
		Con_Printf("ERROR: couldn't read from file.\n");

	CL_Disconnect_f();

	SV_SpawnServer(mapname);

	if (!sv.active)
	{
		Con_Printf("Couldn't load map\n");
		return;
	}
	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;

// load the light styles

	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		if (fscanf(f, "%s\n", str) < 1)
			Con_Printf("ERROR: couldn't read from file.\n");
		sv.lightstyles[i] = Hunk_Alloc(strlen(str) + 1);
		strcpy(sv.lightstyles[i], str);
	}

// load the edicts out of the savegame file
	entnum = -1;		// -1 is the globals
	while (!feof(f))
	{
		for (i = 0; i < sizeof(str) - 1; i++)
		{
			r = fgetc(f);
			if (r == EOF || !r)
				break;
			str[i] = r;
			if (r == '}')
			{
				i++;
				break;
			}
		}
		if (i == sizeof(str) - 1)
			Sys_Error("Loadgame buffer overflow");
		str[i] = 0;
		start = str;
		start = COM_Parse(str);
		if (!com_token[0])
			break;		// end of file
		if (strcmp(com_token, "{"))
			Sys_Error("First token isn't a brace");

		if (entnum == -1)
		{	// parse the global vars
			ED_ParseGlobals(start);
		}
		else
		{	// parse an edict

			ent = EDICT_NUM(entnum);
			memset(&ent->v, 0, progs->entityfields * 4);
			ent->free = false;
			ED_ParseEdict(start, ent);

			// link it into the bsp tree
			if (!ent->free)
				SV_LinkEdict(ent, false);
		}

		entnum++;
	}

	sv.num_edicts = entnum;
	sv.time = time;

	fclose(f);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		svs.clients->spawn_parms[i] = spawn_parms[i];

	if (cls.state != ca_dedicated)
	{
		CL_EstablishConnection("local");
		Host_Reconnect_f();
	}
}

void Host_Name_f(void)
{
	char newName[32];

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"name\" is \"%s\"\n", cl_name.string);
		return;
	}
	if (Cmd_Argc() == 2)
		strlcpy(newName, Cmd_Argv(1), sizeof(newName));
	else
		strlcpy(newName, Cmd_Args(), sizeof(newName));
	newName[15] = 0; // client_t structure actually says name[32].

	if (cmd_source == src_command)
	{
		if (strcmp(cl_name.string, newName) == 0)
			return;
		Cvar_SetQuick(&cl_name, newName);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer_f();
		return;
	}

	if (host_client->name[0] && strcmp(host_client->name, "unconnected"))
	{
		if (strcmp(host_client->name, newName) != 0)
			Con_Printf("%s renamed to %s\n", host_client->name, newName);
	}
	strcpy(host_client->name, newName);
	host_client->edict->v.netname = PR_SetEngineString(host_client->name);

	// send notification to all clients

	MSG_WriteByte(&sv.reliable_datagram, svc_updatename);
	MSG_WriteByte(&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteString(&sv.reliable_datagram, host_client->name);
}

char *VersionString(void)
{
	static char str[32];
	//                        "12345678901234567890123456"  //26
	// Return something like: "Windows GL ProQuake 3.99x"
	//                        "Mac OSX GL ProQuake 3.99x"
	//                        "Mac OSX Win ProQuake 3.99x"
	//                        "Linux GL ProQuake 3.99x"
	//                        "Windows D3D ProQuake 3.99x"

	snprintf(str, sizeof(str), "%s %s %s", OS_NAME, RENDERER_NAME, ENGINE_VERSION);

	return str;
}

void Host_Version_f(void)
{
	Con_Printf("%s version %s\n", ENGINE_NAME, VersionString());
	Con_Printf("Exe: "__TIME__" "__DATE__"\n");
//#ifdef __GNUC__
//    Con_DevPrintf (DEV_ANY, "Compiled with GNUC\n");
//#else
//	Con_DevPrintf (DEV_ANY, "Compiled with _MSC_VER %i\n", _MSC_VER);
//#endif
}

void Host_Say(bool teamonly)
{
	client_t *client;
	client_t *save;
	int j;
	char *p;
	char text[64];
	bool fromServer = false;

	if (cmd_source == src_command)
	{
		if (cls.state == ca_dedicated)
		{
			fromServer = true;
			teamonly = false;
		}
		else
		{
			Cmd_ForwardToServer_f();
			return;
		}
	}

	if (Cmd_Argc() < 2)
		return;

	save = host_client;

	p = Cmd_Args();
// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

// turn on color set 1
	if (!fromServer)
	{
		// R00k - dont allow new connecting players to spam obscenities...
		if (pq_connectmute.value && (net_time - host_client->netconnection->connecttime) < pq_connectmute.value)
			return;

		// JPG - spam protection
		if (sv.time - host_client->spam_time > pq_spam_rate.value * pq_spam_grace.value)
			host_client->spam_time = sv.time - pq_spam_rate.value * pq_spam_grace.value;
		host_client->spam_time += pq_spam_rate.value;
		if (host_client->spam_time > sv.time)
			return;

		// JPG 3.00 - don't allow messages right after a colour/name change
		if (pq_tempmute.value && sv.time - host_client->change_time < 1 && host_client->netconnection->mod != MOD_QSMACK)
			return;

		// JPG 3.11 - feature request from Slot Zero
		if (pq_showedict.value)
			Sys_Printf("#%d ", NUM_FOR_EDICT(host_client->edict));
		if (teamplay.value && teamonly) // JPG - added () for mm2
			snprintf(text, sizeof(text), "%c(%s): ", 1, save->name);
		else
			snprintf(text, sizeof(text), "%c%s: ", 1, save->name);

		// JPG 3.20 - optionally remove '\r'
		if (pq_removecr.value)
		{
			char *ch;
			for (ch = p; *ch; ch++)
			{
				if (*ch == '\r')
					*ch += 128;
			}
		}
	}
	else
	{
		snprintf(text, sizeof(text), "%c<%s> ", 1, hostname.string);
	}

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if (strlen(p) > j)
		p[j] = 0;

	strlcat(text, p, sizeof(text));
	strlcat(text, "\n", sizeof(text));

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client || !client->active || !client->spawned)
			continue;
		if (teamplay.value && teamonly && client->edict->v.team != save->edict->v.team)
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
	}
	host_client = save;

	// JPG 3.20 - optionally write player binds to server log
	if (pq_logbinds.value)
		Con_Printf("%s", &text[1]);
	else
		Sys_Printf("%s", &text[1]);
}

void Host_Say_f(void)
{
	Host_Say(false);
}

void Host_Say_Team_f(void)
{
	Host_Say(true);
}

void Host_Tell_f(void)
{
	client_t *client;
	client_t *save;
	int j;
	char *p;
	char text[64] = { 0 };

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	// JPG - disabled Tell (to prevent cheating)
	if (host_client->netconnection->mod != MOD_QSMACK)
	{
		SV_ClientPrintf("%cTell is diabled on this server\n", 1);
		return;
	}

	if (Cmd_Argc() < 3)
		return;

	strlcpy(text, host_client->name, sizeof(text));
	strlcat(text, ": ", sizeof(text));

	p = Cmd_Args();

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

// check length & truncate if necessary
	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if (strlen(p) > j)
		p[j] = 0;

	strlcat(text, p, sizeof(text));
	strlcat(text, "\n", sizeof(text));

	save = host_client;
	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (!client->active || !client->spawned)
			continue;
		if (strcasecmp(client->name, Cmd_Argv(1)))
			continue;
		host_client = client;
		SV_ClientPrintf("%s", text);
		break;
	}
	host_client = save;
}

void Host_Color_f(void)
{
	int top, bottom;
	int playercolor;
	extern cvar_t sv_allcolors;

	if (Cmd_Argc() == 1)
	{
		Con_Printf("\"color\" is \"%i %i\"\n", ((int) cl_color.value) >> 4, ((int) cl_color.value) & 0x0f);
		Con_Printf("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2)
		top = bottom = atoi(Cmd_Argv(1));
	else
	{
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	bottom &= 15;
	if (!sv_allcolors.value)
	{
		if (top > 13)
			top = 13;

		if (bottom > 13)
			bottom = 13;
	}

	playercolor = top * 16 + bottom;

	if (cmd_source == src_command)
	{
		Cvar_SetValueQuick(&cl_color, playercolor);
		if (cls.state == ca_connected)
			Cmd_ForwardToServer_f();
		return;
	}

	// JPG 3.11 - bail if the color isn't actually changing
	if (host_client->colors == playercolor)
		return;

	host_client->colors = playercolor;
	host_client->edict->v.team = bottom + 1;

// JPG 3.00 - prevent messages right after a colour/name change
	if (bottom)
		host_client->change_time = sv.time;

// send notification to all clients
	MSG_WriteByte(&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte(&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte(&sv.reliable_datagram, host_client->colors);
}

void Host_Kill_f(void)
{
	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (sv_player->v.health <= 0)
	{
		SV_ClientPrintf("Can't suicide -- already dead!\n");	// JPG 3.02 allready->already
		return;
	}

	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(sv_player);
	PR_ExecuteProgram(pr_global_struct->ClientKill);
}

void Host_Pause_f(void)
{
//	if (cls.demonum == -1) // Don't allow startdemos to be paused
	cl.paused ^= true; // to handle demo-pause

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}
	if (!pausable.value)
		SV_ClientPrintf("Pause not allowed.\n");
	else
	{
		sv.paused ^= true;

		if (sv.paused)
		{
			SV_BroadcastPrintf("%s paused the game\n", PR_GetString(sv_player->v.netname));
		}
		else
		{
			SV_BroadcastPrintf("%s unpaused the game\n", PR_GetString(sv_player->v.netname));
		}

		// send notification to all clients
		MSG_WriteByte(&sv.reliable_datagram, svc_setpause);
		MSG_WriteByte(&sv.reliable_datagram, sv.paused);
	}
}

//===========================================================================

void Host_PreSpawn_f(void)
{
	if (cmd_source == src_command)
	{
		Con_Printf("prespawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf("prespawn not valid -- already spawned\n");	// JPG 3.02 allready->already
		return;
	}

	SZ_Write(&host_client->message, sv.signon.data, sv.signon.cursize);
	MSG_WriteByte(&host_client->message, svc_signonnum);
	MSG_WriteByte(&host_client->message, 2);
	host_client->sendsignon = true;

	host_client->netconnection->encrypt = 2; // JPG 3.50
}

void Host_Spawn_f(void)
{
	int i;
	client_t *client;
	edict_t *ent;

	if (cmd_source == src_command)
	{
		Con_Printf("spawn is not valid from the console\n");
		return;
	}

	if (host_client->spawned)
	{
		Con_Printf("Spawn not valid -- already spawned\n");
		return;
	}

	// run the entrance script
	if (sv.loadgame)
	{	// loaded games are fully inited already
		// if this is the last client to be connected, unpause
		sv.paused = false;
	}
	else
	{
		// set up the edict
		ent = host_client->edict;

		memset(&ent->v, 0, progs->entityfields * 4);
		ent->v.colormap = NUM_FOR_EDICT(ent);
		ent->v.team = (host_client->colors & 15) + 1;
		ent->v.netname = PR_SetEngineString(host_client->name);

		// copy spawn parms out of the client_t
		for (i = 0; i < NUM_SPAWN_PARMS; i++)
			(&pr_global_struct->parm1)[i] = host_client->spawn_parms[i];

		// call the spawn function
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(sv_player);
		PR_ExecuteProgram(pr_global_struct->ClientConnect);

		if ((Sys_DoubleTime() - host_client->netconnection->connecttime) <= sv.time)
			Sys_Printf("%s entered the game\n", host_client->name);

		PR_ExecuteProgram(pr_global_struct->PutClientInServer);
	}

// send all current names, colors, and frag counts
	SZ_Clear(&host_client->message);

// send time of update
	MSG_WriteByte(&host_client->message, svc_time);
	MSG_WriteFloat(&host_client->message, sv.time);

	for (i = 0, client = svs.clients; i < svs.maxclients; i++, client++)
	{
		MSG_WriteByte(&host_client->message, svc_updatename);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteString(&host_client->message, client->name);
		MSG_WriteByte(&host_client->message, svc_updatefrags);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteShort(&host_client->message, client->old_frags);
		MSG_WriteByte(&host_client->message, svc_updatecolors);
		MSG_WriteByte(&host_client->message, i);
		MSG_WriteByte(&host_client->message, client->colors);
	}

// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		MSG_WriteByte(&host_client->message, svc_lightstyle);
		MSG_WriteByte(&host_client->message, (char) i);
		MSG_WriteString(&host_client->message, sv.lightstyles[i]);
	}

//
// send some stats
//
	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_TOTALSECRETS);
	MSG_WriteLong(&host_client->message, pr_global_struct->total_secrets);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_TOTALMONSTERS);
	MSG_WriteLong(&host_client->message, pr_global_struct->total_monsters);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_SECRETS);
	MSG_WriteLong(&host_client->message, pr_global_struct->found_secrets);

	MSG_WriteByte(&host_client->message, svc_updatestat);
	MSG_WriteByte(&host_client->message, STAT_MONSTERS);
	MSG_WriteLong(&host_client->message, pr_global_struct->killed_monsters);

//
// send a fixangle
// Never send a roll angle, because savegames can catch the server
// in a state where it is expecting the client to correct the angle
// and it won't happen if the game was just loaded, so you wind up
// with a permanent head tilt
	ent = EDICT_NUM(1 + (host_client - svs.clients));
	MSG_WriteByte(&host_client->message, svc_setangle);
	if (sv.loadgame)
	{
		MSG_WriteAngle(&host_client->message, ent->v.v_angle[0]);
		MSG_WriteAngle(&host_client->message, ent->v.v_angle[1]);
		MSG_WriteAngle(&host_client->message, 0);
	}
	else
	{
		MSG_WriteAngle(&host_client->message, ent->v.angles[0]);
		MSG_WriteAngle(&host_client->message, ent->v.angles[1]);
		MSG_WriteAngle(&host_client->message, 0);
	}

	SV_WriteClientdataToMessage(sv_player, &host_client->message);

	MSG_WriteByte(&host_client->message, svc_signonnum);
	MSG_WriteByte(&host_client->message, 3);
	host_client->sendsignon = true;

	// JPG - added this for spam protection
	host_client->spam_time = 0;
}

void Host_Begin_f(void)
{
	if (cmd_source == src_command)
	{
		Con_Printf("begin is not valid from the console\n");
		return;
	}

	host_client->spawned = true;

	host_client->netconnection->encrypt = 0;	// JPG 3.50
}

//===========================================================================

/*
 ==================
 Host_Kick_f

 Kicks a user off of the server
 ==================
 */
void Host_Kick_f(void)
{
	char *who;
	char *message = NULL;
	client_t *save;
	int i;
	bool byNumber = false;

	if (cmd_source == src_command)
	{
		if (!sv.active)
		{
			Cmd_ForwardToServer_f();
			return;
		}
	}
	else if (pr_global_struct->deathmatch)
		return;

	save = host_client;

	if (Cmd_Argc() > 2 && strcmp(Cmd_Argv(1), "#") == 0)
	{
		i = atof(Cmd_Argv(2)) - 1;
		if (i < 0 || i >= svs.maxclients)
			return;
		if (!svs.clients[i].active)
			return;
		host_client = &svs.clients[i];
		byNumber = true;
	}
	else
	{
		for (i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++)
		{
			if (!host_client->active)
				continue;
			if (strcasecmp(host_client->name, Cmd_Argv(1)) == 0)
				break;
		}
	}

	if (i < svs.maxclients)
	{
		if (cmd_source == src_command)
			if (cls.state == ca_dedicated)
				who = "Console";
			else
				who = cl_name.string;
		else
			who = save->name;

		// can't kick yourself!
		if (host_client == save)
			return;

		if (Cmd_Argc() > 2)
		{
			message = COM_Parse(Cmd_Args());
			if (byNumber)
			{
				message++;							// skip the #
				while (*message == ' ')				// skip white space
					message++;
				message += strlen(Cmd_Argv(2));	// skip the number
			}
			while (*message && *message == ' ')
				message++;
		}
		if (message)
			SV_ClientPrintf("Kicked by %s: %s\n", who, message);
		else
			SV_ClientPrintf("Kicked by %s\n", who);
		SV_DropClient(false);
	}

	host_client = save;
}

/*
 ===============================================================================

 DEBUGGING TOOLS

 ===============================================================================
 */

void Host_Give_f(void)
{
	char *t;
	int v;
	eval_t *val;

	if (cmd_source == src_command)
	{
		Cmd_ForwardToServer_f();
		return;
	}

	if (pr_global_struct->deathmatch)
		return;

	t = Cmd_Argv(1);
	v = atoi(Cmd_Argv(2));

	switch (t[0])
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		// MED 01/04/97 added hipnotic give stuff
		if (hipnotic)
		{
			if (t[0] == '6')
			{
				if (t[1] == 'a')
					sv_player->v.items = (int) sv_player->v.items | HIT_PROXIMITY_GUN;
				else
					sv_player->v.items = (int) sv_player->v.items | IT_GRENADE_LAUNCHER;
			}
			else if (t[0] == '9')
				sv_player->v.items = (int) sv_player->v.items | HIT_LASER_CANNON;
			else if (t[0] == '0')
				sv_player->v.items = (int) sv_player->v.items | HIT_MJOLNIR;
			else if (t[0] >= '2')
				sv_player->v.items = (int) sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		else
		{
			if (t[0] >= '2')
				sv_player->v.items = (int) sv_player->v.items | (IT_SHOTGUN << (t[0] - '2'));
		}
		break;

	case 's':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_shells1)))
				val->_float = v;
		}

		sv_player->v.ammo_shells = v;
		break;

	case 'n':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_nails1)))
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		else
		{
			sv_player->v.ammo_nails = v;
		}
		break;

	case 'l':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_lava_nails)))
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_nails = v;
			}
		}
		break;

	case 'r':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_rockets1)))
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		else
		{
			sv_player->v.ammo_rockets = v;
		}
		break;

	case 'm':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_multi_rockets)))
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_rockets = v;
			}
		}
		break;

	case 'h':
		sv_player->v.health = v;
		break;

	case 'c':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_cells1)))
			{
				val->_float = v;
				if (sv_player->v.weapon <= IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		else
		{
			sv_player->v.ammo_cells = v;
		}
		break;

	case 'p':
		if (rogue)
		{
			if ((val = GETEDICTFIELDVALUE(sv_player, eval_ammo_plasma)))
			{
				val->_float = v;
				if (sv_player->v.weapon > IT_LIGHTNING)
					sv_player->v.ammo_cells = v;
			}
		}
		break;
		// Baker 3.60 - give "a" for armor from FitzQuake
		//johnfitz -- give armour
	case 'a':
		if (v >= 0 && v <= 100)
		{
			sv_player->v.armortype = 0.3;
			sv_player->v.armorvalue = v;
			sv_player->v.items = sv_player->v.items - ((int) (sv_player->v.items) & (int) (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR1;
		}
		if (v > 100 && v <= 150)
		{
			sv_player->v.armortype = 0.6;
			sv_player->v.armorvalue = v;
			sv_player->v.items = sv_player->v.items - ((int) (sv_player->v.items) & (int) (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR2;
		}
		if (v > 150 && v <= 200)
		{
			sv_player->v.armortype = 0.8;
			sv_player->v.armorvalue = v;
			sv_player->v.items = sv_player->v.items - ((int) (sv_player->v.items) & (int) (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR3;
		}
		break;
		//johnfitz
	}
}

edict_t *FindViewthing(void)
{
	int i;
	edict_t *e;

	for (i = 0; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM(i);
		if (!strcmp(PR_GetString(e->v.classname), "viewthing"))
			return e;
	}
	Con_Printf("No viewthing on map\n");
	return NULL;
}

void Host_Viewmodel_f(void)
{
	edict_t *e;
	model_t *m;

	e = FindViewthing();
	if (!e)
		return;

	m = Mod_ForName(Cmd_Argv(1), false);
	if (!m)
	{
		Con_Printf("Can't load %s\n", Cmd_Argv(1));
		return;
	}

	e->v.frame = 0;
	cl.model_precache[(int) e->v.modelindex] = m;
}

void Host_Viewframe_f(void)
{
	edict_t *e;
	int f;
	model_t *m;

	e = FindViewthing();
	if (!e)
		return;
	m = cl.model_precache[(int) e->v.modelindex];

	f = atoi(Cmd_Argv(1));
	if (f >= m->numframes)
		f = m->numframes - 1;

	e->v.frame = f;
}

void PrintFrameName(model_t *m, int frame)
{
	alias_model_t *aliasmodel;
	maliasframedesc_t *pframedesc;

	aliasmodel = m->aliasmodel;
	if (!aliasmodel)
		return;
	pframedesc = &aliasmodel->frames[frame];

	Con_Printf("frame %i: %s\n", frame, pframedesc->name);
}

void Host_Viewnext_f(void)
{
	edict_t *e;
	model_t *m;

	e = FindViewthing();
	if (!e)
		return;
	m = cl.model_precache[(int) e->v.modelindex];

	e->v.frame = e->v.frame + 1;
	if (e->v.frame >= m->numframes)
		e->v.frame = m->numframes - 1;

	PrintFrameName(m, e->v.frame);
}

void Host_Viewprev_f(void)
{
	edict_t *e;
	model_t *m;

	e = FindViewthing();
	if (!e)
		return;

	m = cl.model_precache[(int) e->v.modelindex];

	e->v.frame = e->v.frame - 1;
	if (e->v.frame < 0)
		e->v.frame = 0;

	PrintFrameName(m, e->v.frame);
}

/*
 ===============================================================================

 DEMO LOOP CONTROL

 ===============================================================================
 */

void Host_Startdemos_f(void)
{
	int i, c;

	if (cls.state == ca_dedicated)
	{
		if (!sv.active)
			Cbuf_AddText("map start\n");
		return;
	}

	c = Cmd_Argc() - 1;
	if (c > MAX_DEMOS)
	{
		Con_Printf("Max %i demos in demoloop\n", MAX_DEMOS);
		c = MAX_DEMOS;
	}

	if (Cmd_Argc() != 1)
	{
		cls.demonum = 0;

	}
	Con_DPrintf("%i demo(s) in loop\n", c);

	for (i = 1; i < c + 1; i++)
		strncpy(cls.demos[i - 1], Cmd_Argv(i), sizeof(cls.demos[0]) - 1);

	if (!sv.active && cls.demonum != -1 && !cls.demoplayback)
	{
		cls.demonum = 0;
		CL_NextDemo();
	}
	else
		cls.demonum = -1;
}

/*
 ==================
 Host_Demos_f

 Return to looping demos
 ==================
 */
void Host_Demos_f(void)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demonum == -1)
		cls.demonum = 1;

	CL_Disconnect_f();
	CL_NextDemo();
}

/*
 ==================
 Host_Stopdemo_f

 Return to looping demos
 ==================
 */
void Host_Stopdemo_f(void)
{
	if (cls.state == ca_dedicated)
		return;
	if (!cls.demoplayback)
		return;
	CL_StopPlayback();

// Baker :Since this is an intentional user action,
// clear the demos queue.
	CL_Clear_Demos_Queue();

	CL_Disconnect();
}

#define DIGIT(x) ((x) >= '0' && (x) <= '9')
void Load_Stats_Id_f(void)
{
	if (Cmd_Argc() != 2)
		Con_Printf("%s command requires a parameter\n", Cmd_Argv(0));
	else if (atoi(Cmd_Argv(1)) == 0)
		Con_Printf("%s command uses a number", Cmd_Argv(0));
	else
	{
		//FIXME: This was not a portable solution
		//int hdsernum = Sys_GetHardDriveSerial (argv[0]) / 1000000;
		int hdsernum = 0;
		int statsnum = atoi(Cmd_Argv(1)) - hdsernum;
		Cbuf_AddText(va("pq_password %i\n", statsnum));
		Con_Printf("Stats tracking id loaded\n");
	}
}

void Stats_Id_f(void)
{
	if (Cmd_Argc() != 2)
		Con_Printf("%s <quakeone.com stats id> to log it\n", Cmd_Argv(0));
	else if (atoi(Cmd_Argv(1)) == 0)
		Con_Printf("Your QuakeOne.com stats id is a number\n");
	else
	{
		char buffer[32] = { 0 };
		const char* stats_id_text = Cmd_Argv(1);
		const char* cursor = stats_id_text;
		int stringlen = strlen(stats_id_text);
		int i;
		FILE *f;
		for (i = 0; i < stringlen; i++)
		{
			// Advance past dashes and stuff  IS_NUM
			while (*cursor && (!DIGIT(*cursor)))
				cursor++;
			if (!*cursor)
				break;
			buffer[i] = *cursor++;
		}

		f = fopen(va("%s/id1/stats_id.cfg", com_basedir), "wt");
		if (!f)
			Con_Printf("Could open stats id file for writing\n");
		else
		{
			int stats_num = atoi(buffer);
			//FIXME: This was not a portable solution
			//int hdsernum = Sys_GetHardDriveSerial (argv[0]) / 1000000;
			int hdsernum = 0;

			fprintf(f, "load_stats_id \"%i\"\n", stats_num + hdsernum);
			Con_Printf("Committed your stats id \"%i\" to file\n", stats_num);
			fclose(f);
		}

	}

}

void Host_InitCommands(void)
{
	Cmd_AddCommand("status", Host_Status_f);
	Cmd_AddCommand("gamedir", Host_Game_f); //johnfitz
	Cmd_AddCommand("quit", Host_Quit_f);
	Cmd_AddCommand("god", Host_God_f);
	Cmd_AddCommand("notarget", Host_Notarget_f);
	Cmd_AddCommand("fly", Host_Fly_f);
	Cmd_AddCommand("map", Host_Map_f);
	Cmd_AddCommand("restart", Host_Restart_f);
	Cmd_AddCommand("changelevel", Host_Changelevel_f);
	Cmd_AddCommand("connect", Host_Connect_f);
	Cmd_AddCommand("reconnect", Host_Reconnect_f);
	Cmd_AddCommand("name", Host_Name_f);
	Cmd_AddCommand("noclip", Host_Noclip_f);
	Cmd_AddCommand("version", Host_Version_f);
	Cmd_AddCommand("say", Host_Say_f);
	Cmd_AddCommand("say_team", Host_Say_Team_f);
	Cmd_AddCommand("tell", Host_Tell_f);
	Cmd_AddCommand("color", Host_Color_f);
	Cmd_AddCommand("kill", Host_Kill_f);
	Cmd_AddCommand("pause", Host_Pause_f);
	Cmd_AddCommand("spawn", Host_Spawn_f);
	Cmd_AddCommand("begin", Host_Begin_f);
	Cmd_AddCommand("prespawn", Host_PreSpawn_f);
	Cmd_AddCommand("kick", Host_Kick_f);
	Cmd_AddCommand("ping", Host_Ping_f);
	Cmd_AddCommand("load", Host_Loadgame_f);
	Cmd_AddCommand("save", Host_Savegame_f);
	Cmd_AddCommand("give", Host_Give_f);

	Cmd_AddCommand("startdemos", Host_Startdemos_f);
	Cmd_AddCommand("demos", Host_Demos_f);
	Cmd_AddCommand("stopdemo", Host_Stopdemo_f);

	Cmd_AddCommand("viewmodel", Host_Viewmodel_f);
	Cmd_AddCommand("viewframe", Host_Viewframe_f);
	Cmd_AddCommand("viewnext", Host_Viewnext_f);
	Cmd_AddCommand("viewprev", Host_Viewprev_f);

	Cmd_AddCommand("qcexec", Host_QC_Exec);
	Cmd_AddCommand("mcache", Mod_Print);

	Cmd_AddCommand("stats_id", Stats_Id_f);
	Cmd_AddCommand("load_stats_id", Load_Stats_Id_f);

	Cvar_RegisterVariable(&cl_confirmquit); // Baker 3.60
}
