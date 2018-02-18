/*
 * Memory Allocation
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

#ifndef __ZONE_H
#define __ZONE_H

/*
 * H_??? The hunk manages the entire memory block given to quake.  It must be
 * contiguous.  Memory can be allocated from either the low or high end in a
 * stack fashion.  The only way memory is released is by resetting one of the
 * pointers.
 *
 * Hunk allocations should be given a name, so the Hunk_Print () function
 * can display usage.
 *
 * Hunk allocations are guaranteed to be 16 byte aligned.
 *
 * The video buffers are allocated high to avoid leaving a hole underneath
 * server allocations when changing to a higher video mode.
 */

void *Q_malloc(size_t size);
void *Q_calloc(size_t n, size_t size);
void *Q_realloc(void *ptr, size_t size);
char *Q_strdup(const char *str);

void *Hunk_Alloc(int size); // returns 0 filled memory
void *Hunk_AllocName(int size, const char *name);
void *Hunk_HighAllocName(int size, const char *name);
int Hunk_LowMark(void);
void Hunk_FreeToLowMark(int mark);
int Hunk_HighMark(void);
void Hunk_FreeToHighMark(int mark);
void *Hunk_TempAlloc(int size);
void Hunk_Check(void);

void Memory_Init(void *buf, int size);

#endif /* __ZONE_H */
