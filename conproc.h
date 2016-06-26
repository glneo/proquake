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

#define CCOM_WRITE_TEXT		0x2
// Param1 : Text

#define CCOM_GET_TEXT		0x3
// Param1 : Begin line
// Param2 : End line

#define CCOM_GET_SCR_LINES	0x4
// No params

#define CCOM_SET_SCR_LINES	0x5
// Param1 : Number of lines

void InitConProc (HANDLE hFile, HANDLE heventParent, HANDLE heventChild);
void DeinitConProc (void);

