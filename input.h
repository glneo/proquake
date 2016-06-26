/*
 * External (non-keyboard) input devices
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

void IN_Init (void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

void IN_Accumulate (void);

void IN_Move (usercmd_t *cmd);
// add additional movement on top of the keyboard move cmd

void IN_ClearStates (void);
// restores all button and position states to defaults

#ifdef _WIN32
void IN_MouseWheel (void);
#endif

extern	cvar_t	freelook;   // Baker 3.60 - Freelook cvar support
extern cvar_t	m_accel;

void IN_Keyboard_Acquire (void);
void IN_Keyboard_Unacquire (void);

#define mlook_active	(freelook.value || (in_mlook.state & 1))   // Baker 3.60 - Freelook cvar support from JoeQuake
