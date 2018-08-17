/*
 * Stub sound definitions
 *
 * Copyright (C) 1996-1997 Id Software, Inc.
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

cvar_t bgmvolume = { "bgmvolume", "1", CVAR_ARCHIVE };
cvar_t sfxvolume = { "sfxvolume", "0.7", CVAR_ARCHIVE };
cvar_t ambientvolume = { "ambientvolume", "0.3", CVAR_ARCHIVE };

void S_Init(void)
{
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&sfxvolume);
	Cvar_RegisterVariable(&ambientvolume);

	Con_Printf("Built without sound support, audio will be unavailable\n");
}

void S_Shutdown(void)
{
}

void S_TouchSound(const char *sample)
{
}

void S_ClearBuffer(void)
{
}

void S_StaticSound(sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
}

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol, float attenuation)
{
}

void S_StopSound(int entnum, int entchannel)
{
}

sfx_t *S_ForName(const char *name)
{
	return NULL;
}

void S_Update(vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
}

void S_StopAllSounds(bool clear)
{
}

void S_BeginPrecaching(void)
{
}

void S_EndPrecaching(void)
{
}

void S_ExtraUpdate(void)
{
}

void S_LocalSound(const char *s)
{
}
