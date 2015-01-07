/*
 * Input devices
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

#ifndef __INPUT_H
#define __INPUT_H

void IN_Commands(void); // opportunity for devices to stick commands on the script buffer

void IN_Move(usercmd_t *cmd); // add additional movement on top of the keyboard move cmd

void IN_SendKeyEvents(void);

void IN_UpdateInputMode(void);

void IN_Deactivate(void);
void IN_Activate(void);

void IN_Init(void);
void IN_Shutdown(void);

#endif /* __INPUT_H */
