/*
 * Sound caching
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2010-2011 O. Sezer <sezero@users.sourceforge.net>
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

#define	MAX_SFX 1024
sfx_t known_sfx[MAX_SFX]; // FIXME: Make dynamic
unsigned int num_sfx;

/* WAV loading */

#define WAV_FORMAT_PCM	1

typedef struct
{
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs; /* chunk starts this many bytes from file start */
} wavinfo_t;

static byte *data_p;
static byte *iff_end;
static byte *last_chunk;
static byte *iff_data;
static int iff_chunk_len;

static void FindNextChunk(const char *name)
{
	while (1)
	{
		// Need at least 8 bytes for a chunk
		if (last_chunk + 8 >= iff_end)
		{
			data_p = NULL;
			return;
		}

		data_p = last_chunk + 4;
		iff_chunk_len = LittleLong(*(int *)data_p);
		data_p += 4;
		if (iff_chunk_len < 0 || iff_chunk_len > iff_end - data_p)
		{
			data_p = NULL;
			Con_DPrintf("bad \"%s\" chunk length (%d)\n", name, iff_chunk_len);
			return;
		}
		last_chunk = data_p + ((iff_chunk_len + 1) & ~1);
		data_p -= 8;
		if (!strncmp((char *) data_p, name, 4))
			return;
	}
}

static void FindChunk(const char *name)
{
	last_chunk = iff_data;
	FindNextChunk(name);
}

static wavinfo_t GetWavinfo(const char *name, byte *wav, int wavlength)
{
	wavinfo_t info;
	int i;
	int format;
	int samples;

	memset(&info, 0, sizeof(info));

	if (!wav)
		return info;

	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((char *) data_p + 8, "WAVE", 4)))
	{
		Con_Printf("%s missing RIFF/WAVE chunks\n", name);
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Printf("%s is missing fmt chunk\n", name);
		return info;
	}
	data_p += 8;
	format = LittleShort(*(short *) data_p);
	data_p += 2;
	if (format != WAV_FORMAT_PCM)
	{
		Con_Printf("%s is not Microsoft PCM format\n", name);
		return info;
	}

	info.channels = LittleShort(*(short *) data_p);
	data_p += 2;
	info.rate = LittleLong(*(int *) data_p);
	data_p += 4;
	data_p += 4 + 2;
	i = LittleShort(*(short *) data_p);
	data_p += 2;
	if (i != 8 && i != 16)
		return info;
	info.width = i / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = LittleLong(*(int *) data_p);
		data_p += 4;
		//	Con_Printf("loopstart=%d\n", sfx->loopstart);

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk("LIST");
		if (data_p)
		{
			if (!strncmp((char *) data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = LittleLong(*(int *) data_p);	// samples in loop
				data_p += 4;
				info.samples = info.loopstart + i;
				//		Con_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Printf("%s is missing data chunk\n", name);
		return info;
	}

	data_p += 4;
	samples = LittleLong(*(int *) data_p) / info.width;
	data_p += 4;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error("%s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;

	return info;
}

static void ResampleSfx(sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	// see if still in memory
	if (!sfx->needload)
		return;

	sfxcache_t *sc = sfx->sc;
	if (!sc)
		return;

	float stepscale = (float) inrate / shm->speed; // this is usually 0.5, 1, or 2

	sc->length /= stepscale;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = shm->speed;
	sc->width = inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate
	if (stepscale == 1)
	{
		// fast special case
		for (int i = 0; i < sc->length; i++)
			((signed char *) sc->data)[i] = (int) ((unsigned char) (data[i]) - 128);
	}
	else
	{
		// general case
		int samplefrac = 0;
		int fracstep = stepscale * 256;
		for (int i = 0; i < sc->length; i++)
		{
			int srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				((short *) sc->data)[i] = LittleShort(((short *) data)[srcsample]);
			else
				((signed char *) sc->data)[i] = (int) ((unsigned char) (data[srcsample]) - 128);
		}
	}
}

sfxcache_t *S_LoadSound(sfx_t *s)
{
	// see if still in memory
	if (!s->needload)
		return s->sc;

	// load it in
	char namebuffer[256];
	strlcpy(namebuffer, "sound/", sizeof(namebuffer));
	strlcat(namebuffer, s->name, sizeof(namebuffer));
	byte *data = COM_LoadMallocFile(namebuffer);
	if (!data)
	{
		Con_Printf("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	wavinfo_t info = GetWavinfo(s->name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf("%s is not mono channeled\n", s->name);
		return NULL;
	}
	if (info.width != 1 && info.width != 2)
	{
		Con_Printf("%s is not 8 or 16 bit\n", s->name);
		return NULL;
	}

	if (!shm)
		return NULL;

	float stepscale = (float) info.rate / shm->speed;
	int len = info.samples / stepscale;
	len *= info.width;
	len *= info.channels;

	if (info.samples == 0 || len == 0)
	{
		Con_Printf("%s has zero samples\n", s->name);
		return NULL;
	}

	s->sc = (sfxcache_t *) Q_malloc(len + sizeof(sfxcache_t));
	if (!s->sc)
		return NULL;

	s->sc->length = info.samples;
	s->sc->loopstart = info.loopstart;
	s->sc->speed = info.rate;
	s->sc->width = info.width;
	s->sc->stereo = info.channels;

	ResampleSfx(s, s->sc->speed, s->sc->width, data + info.dataofs);

	free(data);

	s->needload = false;

	return s->sc;
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
	sfx->needload = true;

	return sfx;
}

void S_TouchSound(const char *name)
{
	S_FindName(name);
}

/* Loads in a sound for the given name */
sfx_t *S_ForName(const char *name)
{
	sfx_t *sfx = S_FindName(name);

	// cache it in
	S_LoadSound(sfx);

	return sfx;
}
