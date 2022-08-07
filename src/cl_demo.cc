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
#include <time.h> // easyrecord stats

typedef struct framepos_s
{
	long baz;
	struct framepos_s *next;
} framepos_t;

framepos_t *demo_framepos = NULL;
bool start_of_demo = false;
bool bumper_on = false;

/*
 * When a demo is playing back, all NET_SendMessages are skipped, and
 * NET_GetMessages are read from the demo file.
 *
 * Whenever cl.time gets past the last received message, another message is
 * read from the demo file.
 */

// JPG 1.05 - support for recording demos after connecting to the server
byte demo_head[3][MAX_MSGLEN];
int demo_head_size[2];

static void CL_FinishTimeDemo(void)
{
	cls.timedemo = false;

	// the first frame didn't count
	int frames = (host_framecount - cls.td_startframe) - 1;
	float time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames / time);
}

/* Called when a demo file runs out, or the user starts a game */
void CL_StopPlayback(void)
{
	if (!cls.demoplayback)
		return;

	COM_CloseFile(cls.demofile);
	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;

	Con_DPrintf("Demo playback has ended\n");

	if (cls.timedemo)
		CL_FinishTimeDemo();
}

/* Dumps the current net message, prefixed by the length and view angles */
static void CL_WriteDemoMessage(void)
{
	int len = LittleLong(net_message.cursize);
	Sys_FileWrite(cls.demofile, &len, 4);
	for (int i = 0; i < 3; i++)
	{
		float f = LittleFloat(cl.viewangles[i]);
		Sys_FileWrite(cls.demofile, &f, 4);
	}
	Sys_FileWrite(cls.demofile, net_message.data, net_message.cursize);
	fflush(cls.demofile);
}

static void PushFrameposEntry(long fbaz)
{
	framepos_t *newf = (framepos_t *)Q_malloc(sizeof(framepos_t));
	newf->baz = fbaz;

	if (!demo_framepos)
		start_of_demo = false;

	newf->next = demo_framepos;
	demo_framepos = newf;
}

static void EraseTopEntry(void)
{
	framepos_t *top = demo_framepos;
	demo_framepos = demo_framepos->next;
	free(top);
}

static int CL_GetDemoMessage()
{
	size_t ret;
	float f;

	if (cls.signon < SIGNONS) // clear stuffs if new demo
		while (demo_framepos)
			EraseTopEntry();

	// decide if it is time to grab the next message
	if (cls.signon == SIGNONS) // always grab until fully connected
	{
		if (cls.timedemo)
		{
			if (host_framecount == cls.td_lastframe)
				return 0; // already read this frame's message
			cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
			if (host_framecount == cls.td_startframe + 1)
				cls.td_starttime = realtime;
		}
		else if (cl.ctime <= cl.mtime[0])
			return 0; // don't need another message yet

		// fill in the stack of frames' positions
		// enable on intermission or not...?
		// NOTE: it can't handle fixed intermission views!
		if (!cl.intermission)
			PushFrameposEntry(ftell(cls.demofile));
	}

	// get the next message
	cls.demo_offset_current = ftell(cls.demofile);
	ret = Sys_FileRead(cls.demofile, &net_message.cursize, 4);
	if (ret != 4)
	{
		CL_StopPlayback();
		return 0;
	}
	net_message.cursize = LittleLong(net_message.cursize);
	if (net_message.cursize > MAX_MSGLEN)
		Sys_Error("Demo message > MAX_MSGLEN");

	VectorCopy(cl.mviewangles[0], cl.mviewangles[1]);
	for (int i = 0; i < 3; i++)
	{
		ret = Sys_FileRead(cls.demofile, &f, 4);
		if (ret != 4)
		{
			CL_StopPlayback();
			return 0;
		}
		cl.mviewangles[0][i] = LittleFloat(f);
	}

	ret = Sys_FileRead(cls.demofile, net_message.data, net_message.cursize);
	if (ret != net_message.cursize)
	{
		CL_StopPlayback();
		return 0;
	}

	return 1;
}

/* Handles recording and playback of demos, on top of NET_ code */
int CL_GetMessage(void)
{
	int ret;

	if (cl.paused)
		return 0;

	if (cls.demoplayback)
		return CL_GetDemoMessage();

	while (true)
	{
		ret = NET_GetMessage(cls.netcon);

		if (ret != 1 && ret != 2)
			return ret;

		// discard nop keepalive message
		if (net_message.cursize == 1 && net_message.data[0] == svc_nop)
			Con_Printf("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage();

	if (cls.signon < 2)
	{
		// record messages before full connection, so that a
		// demo record can happen after connection is done
		memcpy(demo_head[cls.signon], net_message.data, net_message.cursize);
		demo_head_size[cls.signon] = net_message.cursize;
	}

	return ret;
}

/* stop recording a demo */
void CL_Stop_f(void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf("Not recording a demo\n");
		return;
	}

	// write a disconnect message to the demo file
	SZ_Clear(&net_message);
	MSG_WriteByte(&net_message, svc_disconnect);
	CL_WriteDemoMessage();

	// finish up
	COM_CloseFile(cls.demofile);
	// Close demo instance #2 (record end)
	strlcpy(cls.recent_file, cls.demoname, sizeof(cls.recent_file));

	cls.demofile = NULL;
	cls.demorecording = false;
	Con_Printf("Completed demo\n");
}

void CL_Clear_Demos_Queue(void)
{
	// Clear demo loop queue
	for (int i = 0; i < MAX_DEMOS; i++)
		cls.demos[i][0] = 0;
	// Set next demo to none
	cls.demonum = -1;
}
/* record <demoname> <map> [cd track] */
void CL_Record_f(void)
{
	char name[MAX_OSPATH];
	int track;

	if (cmd_source != src_command)
		return;

	if (cls.demoplayback)
	{
		Con_Printf("Can't record during demo playback\n");
		return;
	}

	int argc = Cmd_Argc();
	// Baker: demo parameters = 2 thru 4
	// Not supporting autoname yet (==1)
	if (argc != 2 && argc != 3 && argc != 4)
	{
		Con_Printf("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf("Relative pathnames are not allowed.\n");
		return;
	}

	// JPG 3.00 - consecutive demo bug
	if (cls.demorecording)
		CL_Stop_f();

	// JPG 1.05 - replaced it with this
	if (argc == 2 && cls.state == ca_connected && cls.signon < 2)
	{
		Con_Printf("Can't record - try again when connected\n");
		return;
	}

	// write the forced cd track number, or -1
	if (argc == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
		track = -1;


	CL_Clear_Demos_Queue(); // timedemo is a very intentional action

	// start the map up
	if (argc > 2)
	{
		Cmd_ExecuteString(va("map %s", Cmd_Argv(2)), src_command);
		// If couldn't find the map, don't start recording
		if (cls.state != ca_connected)
			return;
	}

	// open the demo file
	snprintf(name, sizeof(name), "%s/%s", com_gamedir, Cmd_Argv(1));
	COM_ForceExtension(name, ".dem");

	Con_Printf("recording to %s.\n", name);
	cls.demofile = Sys_FileOpenWrite(name);
	if (!cls.demofile)
	{
		Con_Printf("ERROR: couldn't open demo for writing.\n");
		return;
	}

	// Officially recording ... copy the name for reference
	snprintf(cls.demoname, sizeof(cls.demoname), "%s", name);

	cls.forcetrack = track;
	fprintf(cls.demofile, "%i\n", cls.forcetrack);

	cls.demorecording = true;

	// Initialize the demo file if we're already connected
	if (argc == 2 && cls.state == ca_connected)
	{
		byte *data = net_message.data;
		int cursize = net_message.cursize;

		for (int i = 0; i < 2; i++)
		{
			net_message.data = demo_head[i];
			net_message.cursize = demo_head_size[i];
			CL_WriteDemoMessage();
		}

		net_message.data = demo_head[2];
		SZ_Clear(&net_message);

		// current names, colors, and frag counts
		for (int i = 0; i < cl.maxclients; i++)
		{
			MSG_WriteByte(&net_message, svc_updatename);
			MSG_WriteByte(&net_message, i);
			MSG_WriteString(&net_message, cl.scores[i].name);
			MSG_WriteByte(&net_message, svc_updatefrags);
			MSG_WriteByte(&net_message, i);
			MSG_WriteShort(&net_message, cl.scores[i].frags);
			MSG_WriteByte(&net_message, svc_updatecolors);
			MSG_WriteByte(&net_message, i);
			MSG_WriteByte(&net_message, cl.scores[i].colors);
		}

		// send all current light styles
		for (int i = 0; i < MAX_LIGHTSTYLES; i++)
		{
			MSG_WriteByte(&net_message, svc_lightstyle);
			MSG_WriteByte(&net_message, i);
			MSG_WriteString(&net_message, cl_lightstyle[i].map);
		}

		// Write out single player stats too
		MSG_WriteByte(&net_message, svc_updatestat);
		MSG_WriteByte(&net_message, STAT_TOTALSECRETS);
		MSG_WriteLong(&net_message, pr_global_struct->total_secrets);

		MSG_WriteByte(&net_message, svc_updatestat);
		MSG_WriteByte(&net_message, STAT_TOTALMONSTERS);
		MSG_WriteLong(&net_message, pr_global_struct->total_monsters);

		MSG_WriteByte(&net_message, svc_updatestat);
		MSG_WriteByte(&net_message, STAT_SECRETS);
		MSG_WriteLong(&net_message, pr_global_struct->found_secrets);

		MSG_WriteByte(&net_message, svc_updatestat);
		MSG_WriteByte(&net_message, STAT_MONSTERS);
		MSG_WriteLong(&net_message, pr_global_struct->killed_monsters);

		// view entity
		MSG_WriteByte(&net_message, svc_setview);
		MSG_WriteShort(&net_message, cl.viewentity);

		// signon
		MSG_WriteByte(&net_message, svc_signonnum);
		MSG_WriteByte(&net_message, 3);

		CL_WriteDemoMessage();

		// restore net_message
		net_message.data = data;
		net_message.cursize = cursize;
	}
}

void StartPlayingOpenedDemo(void)
{
	int c;
	bool neg = false;

	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = getc(cls.demofile)) != '\n')
	{
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');
	}

	if (neg)
		cls.forcetrack = -cls.forcetrack;
}

// So we know this is a real start demo
bool play_as_start_demo = false;

/* nextstartdemo [demoname] */
void CL_PlayDemo_NextStartDemo_f(void)
{
	play_as_start_demo = true;
	CL_PlayDemo_f(); // Inherits the cmd_argc and cmd_argv
	play_as_start_demo = false;
}

/* playdemo [demoname] */
void CL_PlayDemo_f(void)
{
	char name[256];

	if (cmd_source != src_command)
		return;

	if (!play_as_start_demo)
		CL_Clear_Demos_Queue();

	if (Cmd_Argc() != 2)
	{
		Con_Printf("playdemo <demoname> : plays a demo\n");
		return;
	}

	// disconnect from server
	CL_Disconnect();

	// Revert
	cls.demospeed = 0; // 0 = Don't use
	bumper_on = false;

	// open the demo file
	strlcpy(name, Cmd_Argv(1), sizeof(name));
	COM_DefaultExtension(name, ".dem");

	COM_OpenFile(name, &cls.demofile);
	if (!cls.demofile)
	{
		Con_Printf("ERROR: couldn't open %s\n", name);
		cls.demonum = -1;		// stop demo loop
		return;
	}

	snprintf(cls.demoname, sizeof(cls.demoname), "%s", name);
	cls.demo_offset_start = ftell(cls.demofile); // qfs_lastload.offset instead?
	cls.demo_file_length = com_filesize;
	cls.demo_hosttime_start = cls.demo_hosttime_elapsed = 0; // Fill this in ... host_time;
	cls.demo_cltime_start = cls.demo_cltime_elapsed = 0;  // Fill this in

	Con_Printf("Playing demo from %s\n", COM_SkipPath(name));

	StartPlayingOpenedDemo();
}

/* timedemo [demoname] */
void CL_TimeDemo_f(void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	// Close the console for timedemo
	// This is a performance benchmark, no reason to have console up
	key_dest = key_game;

	// timedemo is a very intentional action
	CL_Clear_Demos_Queue();

	CL_PlayDemo_f();

	// don't trigger timedemo mode if playdemo fails
	if (!cls.demofile)
		return;

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted
	cls.timedemo = true;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1; // get a new message this frame
}
