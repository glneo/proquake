/*
 * Client sound I/O functions
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2007-2008 Kristian Duske
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

#ifndef _SOUND_H
#define _SOUND_H

#define	MAX_CHANNELS 1024
#define	MAX_DYNAMIC_CHANNELS 128

#define DEFAULT_SOUND_PACKET_VOLUME 255
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

#define	MAX_RAW_SAMPLES	8192

typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char name[MAX_QPATH];
	cache_user_t cache;
} sfx_t;

typedef struct
{
	int length;
	int loopstart;
	int speed;
	int width;
	int stereo;
	byte data[1]; /* variable sized	*/
} sfxcache_t;

typedef struct
{
	int channels;
	int samples; /* mono samples in buffer */
	int submission_chunk; /* don't mix less than this # */
	int samplepos; /* in mono samples */
	int samplebits;
	int signed8; /* device opened for S8 format? (e.g. Amiga AHI) */
	int speed;
	unsigned char *buffer;
} dma_t;

typedef struct
{
	sfx_t *sfx; /* sfx number */
	int leftvol; /* 0-255 volume */
	int rightvol; /* 0-255 volume */
	int end; /* end time in global paintsamples */
	int pos; /* sample position in sfx */
	int looping; /* where to loop, -1 = no looping */
	int entnum; /* to allow overriding a specific sound */
	int entchannel;
	vec3_t origin; /* origin of sound effect */
	vec_t dist_mult; /* distance multiplier (attenuation/clipK) */
	int master_vol; /* 0-255 master volume */
} channel_t;

extern portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

/*
 * 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
 * MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
 * MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds
 */
extern channel_t snd_channels[MAX_CHANNELS];

extern volatile dma_t *shm;

extern int total_channels;
extern int paintedtime;
extern int s_rawend;

extern cvar_t sndspeed;
extern cvar_t snd_mixspeed;
extern cvar_t snd_filterquality;
extern cvar_t sfxvolume;
extern cvar_t loadas8bit;
extern cvar_t bgmvolume;

// snd_dma.cc
void S_TouchSound(const char *name);
sfx_t *S_PrecacheSound(const char *name);
void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation);
void S_StopSound(int entnum, int entchannel);
void S_StopAllSounds(bool clear);
void S_ClearBuffer(void);
void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void S_LocalSound(const char *name);
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up);
void S_ExtraUpdate(void);
void S_Init(void);
void S_Shutdown(void);

// snd_mem.cc
sfxcache_t *S_LoadSound(sfx_t *s);

// snd_mix.cc
void S_PaintChannels(int endtime);
void SND_InitScaletable(void);

// snd_sdl.cc
int SNDDMA_GetDMAPos(void); /* gets the current DMA position */
void SNDDMA_LockBuffer(void); /* validates & locks the dma buffer */
void SNDDMA_Submit(void); /* unlocks the dma buffer / sends sound to the device */
void SNDDMA_BlockSound(void); /* blocks sound output upon window focus loss */
void SNDDMA_UnblockSound(void); /* unblocks the output upon window focus gain */
bool SNDDMA_Init(dma_t *dma); /* initializes cycling through a DMA buffer and returns information on it */
void SNDDMA_Shutdown(void); /* shutdown the DMA xfer */

#endif	/* _SOUND_H */
