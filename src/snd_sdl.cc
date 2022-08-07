/*
 * snd_sdl.c - SDL audio driver for Hexen II: Hammer of Thyrion (uHexen2)
 * based on implementations found in the quakeforge and ioquake3 projects.
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2005-2012 O.Sezer <sezero@users.sourceforge.net>
 * Copyright (C) 2010-2014 QuakeSpasm developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "quakedef.h"

#include <SDL2/SDL.h>

static int buffersize;

int SNDDMA_GetDMAPos(void)
{
	return shm->samplepos;
}

void SNDDMA_LockBuffer(void)
{
	SDL_LockAudio();
}

void SNDDMA_Submit(void)
{
	SDL_UnlockAudio();
}

void SNDDMA_BlockSound(void)
{
	SDL_PauseAudio(1);
}

void SNDDMA_UnblockSound(void)
{
	SDL_PauseAudio(0);
}

static void paint_audio(void *unused, Uint8 *stream, int len)
{
	int pos, tobufend;
	int len1, len2;

	if (!shm)
	{ /* shouldn't happen, but just in case */
		memset(stream, 0, len);
		return;
	}

	pos = (shm->samplepos * (shm->samplebits / 8));
	if (pos >= buffersize)
		shm->samplepos = pos = 0;

	tobufend = buffersize - pos; /* bytes to buffer's end. */
	len1 = len;
	len2 = 0;

	if (len1 > tobufend)
	{
		len1 = tobufend;
		len2 = len - len1;
	}

	memcpy(stream, shm->buffer + pos, len1);

	if (len2 <= 0)
	{
		shm->samplepos += (len1 / (shm->samplebits / 8));
	}
	else
	{ /* wraparound? */
		memcpy(stream + len1, shm->buffer, len2);
		shm->samplepos = (len2 / (shm->samplebits / 8));
	}

	if (shm->samplepos >= buffersize)
		shm->samplepos = 0;
}

bool SNDDMA_Init(dma_t *dma)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		Con_Printf("Couldn't init SDL audio: %s\n", SDL_GetError());
		return false;
	}

	/* Set up the desired format */
	SDL_AudioSpec desired;
	desired.freq = sndspeed.value;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2; /* = desired_channels; */
	if (desired.freq <= 11025)
		desired.samples = 256;
	else if (desired.freq <= 22050)
		desired.samples = 512;
	else if (desired.freq <= 44100)
		desired.samples = 1024;
	else if (desired.freq <= 56000)
		desired.samples = 2048; /* for 48 kHz */
	else
		desired.samples = 4096; /* for 96 kHz */
	desired.callback = paint_audio;
	desired.userdata = NULL;

	/* Open the audio device */
	SDL_AudioSpec obtained;
	if (SDL_OpenAudio(&desired, &obtained) == -1)
	{
		Con_Printf("Couldn't open SDL audio: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return false;
	}

	/* Make sure we can support the audio format */
	switch (obtained.format)
	{
	case AUDIO_S8: /* maybe needed by AHI */
	case AUDIO_U8:
	case AUDIO_S16SYS:
		/* Supported */
		break;
	default:
		Con_Printf("Unsupported audio format received (%u)\n", obtained.format);
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return false;
	}

	memset((void *) dma, 0, sizeof(dma_t));
	shm = dma;

	/* Fill the audio DMA information block */
	shm->samplebits = (obtained.format & 0xFF); /* first byte of format is bits */
	shm->signed8 = (obtained.format == AUDIO_S8);
	if (obtained.freq != sndspeed.value)
		Con_Printf("Warning: Rate set (%d) didn't match requested rate (%d)!\n", obtained.freq, (int)sndspeed.value);
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	int samples = (obtained.samples * obtained.channels) * 10;
	if (samples & (samples - 1))
	{
		/* make it a power of two */
		int val = 1;
		while (val < samples)
			val <<= 1;
		samples = val;
	}
	shm->samples = samples;
	shm->samplepos = 0;
	shm->submission_chunk = 1;

	Con_Printf("SDL audio spec  : %d Hz, %d samples, %d channels\n", obtained.freq, obtained.samples, obtained.channels);

	const char *driver = SDL_GetCurrentAudioDriver();
	const char *device = SDL_GetAudioDeviceName(0, SDL_FALSE);
	char drivername[128];
	snprintf(drivername, sizeof(drivername), "%s - %s",
			driver != NULL ? driver : "(UNKNOWN)",
			device != NULL ? device : "(UNKNOWN)");

	buffersize = shm->samples * (shm->samplebits / 8);
	Con_Printf("SDL audio driver: %s, %d bytes buffer\n", drivername, buffersize);

	shm->buffer = (unsigned char *)Q_calloc(1, buffersize);
	if (!shm->buffer)
	{
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		shm = NULL;
		Con_Printf("Failed allocating memory for SDL audio\n");
		return false;
	}

	SDL_PauseAudio(0);

	return true;
}

void SNDDMA_Shutdown(void)
{
	if (shm)
	{
		Con_Printf("Shutting down SDL sound\n");
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		if (shm->buffer)
			free(shm->buffer);
		shm->buffer = NULL;
		shm = NULL;
	}
}
