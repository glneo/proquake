/*
 * Main control for any streaming sound output device
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2007-2008 Kristian Duske
 * Copyright (C) 2010-2011 O. Sezer <sezero@users.sourceforge.net>
 * Copyright (C) 2010-2014 QuakeSpasm developers
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

#define	sound_nominal_clip_dist	1000.0

channel_t snd_channels[MAX_CHANNELS];
int total_channels;

static dma_t sn;
volatile dma_t *shm = NULL;

static vec3_t listener_origin;
static vec3_t listener_forward;
static vec3_t listener_right;
static vec3_t listener_up;

int paintedtime;

int s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

#define	MAX_SFX		1024
static sfx_t *known_sfx = NULL;	// hunk allocated [MAX_SFX]
static unsigned int num_sfx;

static sfx_t *ambient_sfx[NUM_AMBIENTS];

static bool sound_started = false;
static bool snd_blocked = false;

cvar_t bgmvolume = { "bgmvolume", "1", CVAR_ARCHIVE };
cvar_t sfxvolume = { "sfxvolume", "0.7", CVAR_ARCHIVE };
cvar_t ambientvolume = { "ambientvolume", "0.3", CVAR_ARCHIVE };

cvar_t sndspeed = { "sndspeed", "11025", CVAR_NONE };
cvar_t snd_filterquality = { "snd_filterquality", "1", CVAR_NONE };

static cvar_t nosound = { "nosound", "0", CVAR_NONE };
static cvar_t ambient_fade = { "ambient_fade", "100", CVAR_NONE };
static cvar_t snd_noextraupdate = { "snd_noextraupdate", "0", CVAR_NONE };
static cvar_t snd_show = { "snd_show", "0", CVAR_NONE };
static cvar_t _snd_mixahead = { "_snd_mixahead", "0.1", CVAR_NONE };

static void SND_Callback_sfxvolume(cvar_t *var)
{
	SND_InitScaletable();
}

static void SND_Callback_snd_filterquality(cvar_t *var)
{
	if (snd_filterquality.value < 1 || snd_filterquality.value > 5)
	{
		Con_Printf("snd_filterquality must be between 1 and 5\n");
		Cvar_SetValueQuick(&snd_filterquality, CLAMP(1, snd_filterquality.value, 5));
	}
}

static sfx_t *S_FindName(const char *name)
{
	if (!name)
		Sys_Error("NULL");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error("Sound name too long: %s", name);

	// see if already loaded
	size_t i;
	for (i = 0; i < num_sfx; i++)
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];

	if (num_sfx++ == MAX_SFX)
		Sys_Error("out of sfx_t");

	sfx_t *sfx = &known_sfx[i];
	strlcpy(sfx->name, name, sizeof(sfx->name));

	return sfx;
}

void S_TouchSound(const char *name)
{
	if (!sound_started)
		return;

	sfx_t *sfx = S_FindName(name);
	Cache_Check(&sfx->cache);
}

sfx_t *S_PrecacheSound(const char *name)
{
	if (!sound_started || nosound.value)
		return NULL;

	sfx_t *sfx = S_FindName(name);

	// cache it in
	S_LoadSound(sfx);

	return sfx;
}

//=============================================================================

/* spatializes a channel */
static void SND_Spatialize(channel_t *ch)
{
	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.viewentity)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo separation and distance attenuation
	vec3_t source_vec;
	VectorSubtract(ch->origin, listener_origin, source_vec);
	vec_t dist = VectorNormalize(source_vec) * ch->dist_mult;
	vec_t dot = DotProduct(listener_right, source_vec);

	vec_t lscale, rscale;
	if (shm->channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

	// add in distance effect
	vec_t scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}

/* picks a channel based on priorities, empty slots, number of channels */
static channel_t *SND_PickChannel(int entnum, int entchannel)
{
	// Check for replacement sound, or find the best one to replace
	int first_to_die = -1;
	int life_left = 0x7fffffff;
	for (int ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		if (entchannel != 0 && // channel 0 never overrides
		    snd_channels[ch_idx].entnum == entnum &&
		    (snd_channels[ch_idx].entchannel == entchannel || entchannel == -1))
		{
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (snd_channels[ch_idx].entnum == cl.viewentity &&
		    entnum != cl.viewentity &&
		    snd_channels[ch_idx].sfx)
			continue;

		if (snd_channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = snd_channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (snd_channels[first_to_die].sfx)
		snd_channels[first_to_die].sfx = NULL;

	return &snd_channels[first_to_die];
}

/* Start a sound effect */
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
	if (!sound_started || !sfx || nosound.value)
		return;

	// pick a channel to play on
	channel_t *target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset(target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = (int) (fvol * 255);
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return; // not audible at all

	// new channel
	sfxcache_t *sc = S_LoadSound(sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return; // couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sc->length;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	for (size_t ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		channel_t *check = &snd_channels[ch_idx];
		if (check == target_chan || check->sfx != sfx || check->pos)
			continue;

		int skip = 0.1 * shm->speed;
		if (skip > sc->length)
			skip = sc->length;
		if (skip > 0)
			skip = rand() % skip;
		target_chan->pos += skip;
		target_chan->end -= skip;
		break;
	}
}

void S_StopSound(int entnum, int entchannel)
{
	for (size_t i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
	{
		if (snd_channels[i].entnum == entnum &&
		    snd_channels[i].entchannel == entchannel)
		{
			snd_channels[i].end = 0;
			snd_channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds(bool clear)
{
	if (!sound_started)
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; // no statics

	for (size_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (snd_channels[i].sfx)
			snd_channels[i].sfx = NULL;
	}

	memset(snd_channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (clear)
		S_ClearBuffer();
}

void S_ClearBuffer(void)
{
	if (!sound_started || !shm)
		return;

	SNDDMA_LockBuffer();
	if (!shm->buffer)
		return;

	s_rawend = 0;

	int clear;
	if (shm->samplebits == 8 && !shm->signed8)
		clear = 0x80;
	else
		clear = 0;

	memset(shm->buffer, clear, shm->samples * shm->samplebits / 8);

	SNDDMA_Submit();
}

void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Con_Printf("total_channels == MAX_CHANNELS\n");
		return;
	}

	channel_t *ss = &snd_channels[total_channels];
	total_channels++;

	sfxcache_t *sc = S_LoadSound(sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Con_Printf("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	VectorCopy(origin, ss->origin);
	ss->master_vol = (int) vol;
	ss->dist_mult = (attenuation / 64) / sound_nominal_clip_dist;
	ss->end = paintedtime + sc->length;

	SND_Spatialize(ss);
}

/* FIXME: do we really need the blocking at the driver level? */
void S_BlockSound(void)
{
	if (!sound_started || snd_blocked)
		return;

	snd_blocked = 1;
	S_ClearBuffer();
	if (shm)
		SNDDMA_BlockSound();
}

void S_UnblockSound(void)
{
	if (!sound_started || !snd_blocked)
		return;

	snd_blocked = 0;
	SNDDMA_UnblockSound();
	S_ClearBuffer();
}

void S_LocalSound(const char *name)
{
	if (!sound_started ||
	    nosound.value)
		return;

	sfx_t *sfx = S_PrecacheSound(name);
	if (!sfx)
	{
		Con_Printf("S_LocalSound: can't cache %s\n", name);
		return;
	}
	S_StartSound(cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

//=============================================================================

static int GetSoundtime(void)
{
	static int buffers;
	static int oldsamplepos;

	int fullsamples = shm->samples / shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	int samplepos = SNDDMA_GetDMAPos();
	if (samplepos < oldsamplepos)
	{
		buffers++; // buffer wrapped

		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds(true);
		}
	}
	oldsamplepos = samplepos;

	return (buffers * fullsamples) + (samplepos / shm->channels);
}

static void S_Update_(void)
{
	if (!sound_started || snd_blocked)
		return;

	SNDDMA_LockBuffer();
	if (!shm->buffer)
		return;

	// Updates DMA time
	int soundtime = GetSoundtime();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		// Con_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	// mix ahead of current position
	unsigned int endtime = soundtime + (unsigned int) (_snd_mixahead.value * shm->speed);
	int samps = shm->samples >> (shm->channels - 1);
	endtime = min(endtime, (unsigned int )(soundtime + samps));

	S_PaintChannels(endtime);

	SNDDMA_Submit();
}

static void S_UpdateAmbientSounds(void)
{
	// no ambients when disconnected
	if (cls.state != ca_connected ||
	    !cl.worldmodel)
		return;

	mleaf_t *l = Mod_PointInLeaf(listener_origin, cl.worldmodel->brushmodel);
	if (!l || !ambientvolume.value)
	{
		for (size_t ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
			snd_channels[ambient_channel].sfx = NULL;
		return;
	}

	for (size_t ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++)
	{
		channel_t *chan = &snd_channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		int vol = (int)(ambientvolume.value * l->ambient_sound_level[ambient_channel]);
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += (int) (host_frametime * ambient_fade.value);
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= (int) (host_frametime * ambient_fade.value);
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

/* Called once each time through the main loop */
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	if (!sound_started || snd_blocked)
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds();

	channel_t *combine = NULL;

	// update spatialization for static and dynamic sounds
	for (int i = NUM_AMBIENTS; i < total_channels; i++)
	{
		channel_t *ch = &snd_channels[i];

		if (!ch->sfx)
			continue;

		SND_Spatialize(ch); // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame
		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
			// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			int j;
			combine = snd_channels + MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++)
			{
				if (combine->sfx == ch->sfx)
					break;
			}
			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
			}
		}
	}

	// debugging output
	if (snd_show.value)
	{
		int total = 0;
		for (int i = 0; i < total_channels; i++)
		{
			channel_t *ch = &snd_channels[i];
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				Con_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		}

		Con_Printf("----(%i)----\n", total);
	}

	// add raw data from streamed samples
	// BGM_Update(); // moved to the main loop just before S_Update ()

	// mix some sound
	S_Update_();
}

void S_ExtraUpdate(void)
{
	if (snd_noextraupdate.value)
		return; // don't pollute timings
	S_Update_();
}

//=============================================================================

static void S_Play_f(void)
{
	static int hash = 345;
	char name[256];

	for (int i = 1; i < Cmd_Argc(); i++)
	{
		strlcpy(name, Cmd_Argv(i), sizeof(name));
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strlcat(name, ".wav", sizeof(name));
		}
		sfx_t *sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

static void S_PlayVol_f(void)
{
	static int hash = 543;
	char name[256];

	for (int i = 1; i < Cmd_Argc(); i += 2)
	{
		strlcpy(name, Cmd_Argv(i), sizeof(name));
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strlcat(name, ".wav", sizeof(name));
		}
		sfx_t *sfx = S_PrecacheSound(name);
		float vol = atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 1.0);
	}
}

static void S_StopAllSounds_f(void)
{
	S_StopAllSounds(true);
}

static void S_SoundList_f(void)
{
	int total = 0;
	for (size_t i = 0; i < num_sfx; i++)
	{
		sfxcache_t *sc = (sfxcache_t *)Cache_Check(&known_sfx[i].cache);
		if (!sc)
			continue;

		int size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Con_SafePrintf("L");
		else
			Con_SafePrintf(" ");
		Con_SafePrintf("(%2db) %6i : %s\n", sc->width * 8, size, known_sfx[i].name);
	}
	Con_Printf("%i sounds, %i bytes\n", num_sfx, total);
}

static void S_SoundInfo_f(void)
{
	if (!sound_started || !shm)
	{
		Con_Printf("sound system not started\n");
		return;
	}

	Con_Printf("%d bit, %s, %d Hz\n", shm->samplebits, (shm->channels == 2) ? "stereo" : "mono", shm->speed);
	Con_Printf("%5d samples\n", shm->samples);
	Con_Printf("%5d samplepos\n", shm->samplepos);
	Con_Printf("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf("%5d total_channels\n", total_channels);
	Con_Printf("%p dma buffer\n", shm->buffer);
}

void S_Init(void)
{
	static bool snd_initialized = false;

	if (snd_initialized)
	{
		Con_Printf("Sound is already initialized\n");
		return;
	}

	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&sfxvolume);
	Cvar_SetCallback(&sfxvolume, SND_Callback_sfxvolume);
	Cvar_RegisterVariable(&ambientvolume);

	Cvar_RegisterVariable(&sndspeed);
	Cvar_RegisterVariable(&snd_filterquality);
	Cvar_SetCallback(&snd_filterquality, SND_Callback_snd_filterquality);

	Cvar_RegisterVariable(&nosound);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&snd_noextraupdate);
	Cvar_RegisterVariable(&snd_show);
	Cvar_RegisterVariable(&_snd_mixahead);

	if (COM_CheckParm("-nosound"))
		return;

	Con_Printf("\nSound Initialization\n");

	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("playvol", S_PlayVol_f);
	Cmd_AddCommand("stopsound", S_StopAllSounds_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	int i = COM_CheckParm("-sndspeed");
	if (i && i < com_argc - 1)
	{
		Cvar_SetQuick(&sndspeed, com_argv[i + 1]);
	}

	SND_InitScaletable();

	known_sfx = (sfx_t *) Hunk_AllocName(MAX_SFX * sizeof(sfx_t), "sfx_t");
	num_sfx = 0;

	sound_started = SNDDMA_Init(&sn);
	if (!sound_started)
	{
		Con_Printf("Failed initializing sound\n");
		return;
	}

	Con_Printf("Audio: %d bit, %s, %d Hz\n",
		   shm->samplebits,
		   (shm->channels == 2) ? "stereo" : "mono",
		   shm->speed);

	// provides a tick sound until washed clean
//	if (shm->buffer)
//		shm->buffer[4] = shm->buffer[5] = 0x7f;	// force a pop for debugging

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");

	S_StopAllSounds(true);

	snd_initialized = true;
}

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	sound_started = 0;
	snd_blocked = 0;

	SNDDMA_Shutdown();
	shm = NULL;
}
