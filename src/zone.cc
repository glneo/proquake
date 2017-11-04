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

#define	ZONE_DEFAULT_SIZE	0x100000	// 1Mb

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

typedef struct memblock_s
{
	int size;           // including the header and possibly tiny fragments
	int tag;            // a tag of 0 is a free block
	int id;        		// should be ZONEID
	struct memblock_s *next, *prev;
	int pad;			// pad to 64 bit boundary
} memblock_t;

typedef struct
{
	int size;		// total bytes malloced, including header
	memblock_t blocklist;		// start / end cap for linked list
	memblock_t *rover;
} memzone_t;

void Cache_FreeLow(int new_low_hunk);
void Cache_FreeHigh(int new_high_hunk);

/*
 ===================
 Q_malloc

 Use it instead of malloc so that if memory allocation fails,
 the program exits with a message saying there's not enough memory
 instead of crashing after trying to use a NULL pointer
 ===================
 */
void *Q_malloc(size_t size)
{
	void *p;

	if (!(p = malloc(size)))
		Sys_Error("Not enough free memory");

	return p;
}

void *Q_calloc(size_t n, size_t size)
{
	void *p;

	if (!(p = calloc(n, size)))
		Sys_Error("Not enough free memory");

	return p;
}

void *Q_realloc(void *ptr, size_t size)
{
	void *p;

	if (!(p = realloc(ptr, size)))
		Sys_Error("Not enough free memory");

	return p;
}

char *Q_strdup(const char *str)
{
	char *p;

	if (!(p = strdup(str)))
		Sys_Error("Not enough free memory");

	return p;
}

//============================================================================

#define	HUNK_SENTINAL	0x1df001ed

typedef struct
{
	int sentinal;
	int size;		// including sizeof(hunk_t), -1 = not allocated
	char name[8];
} hunk_t;

byte *hunk_base;
int hunk_size;

int hunk_low_used;
int hunk_high_used;

bool hunk_tempactive;
int hunk_tempmark;

/*
 ==============
 Hunk_Check

 Run consistency and sentinal trashing checks
 ==============
 */
void Hunk_Check(void)
{
	hunk_t *h;

	for (h = (hunk_t *) hunk_base; (byte *) h != hunk_base + hunk_low_used;)
	{
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("trashed sentinal");
		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error("bad size");
		h = (hunk_t *) ((byte *) h + h->size);
	}
}

/*
 ==============
 Hunk_Print

 If "all" is specified, every single allocation is printed.
 Otherwise, allocations with the same name will be totaled up before printing.
 ==============
 */
void Hunk_Print(bool all)
{
	hunk_t *h, *next, *endlow, *starthigh, *endhigh;
	int count, sum, totalblocks;
	char name[9];

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunk_t *) hunk_base;
	endlow = (hunk_t *) (hunk_base + hunk_low_used);
	starthigh = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *) (hunk_base + hunk_size);

	Con_Printf("          :%8i total hunk size\n", hunk_size);
	Con_Printf("-------------------------\n");

	while (1)
	{
		// skip to the high hunk if done with low hunk
		if (h == endlow)
		{
			Con_Printf("-------------------------\n");
			Con_Printf("          :%8i REMAINING\n", hunk_size - hunk_low_used - hunk_high_used);
			Con_Printf("-------------------------\n");
			h = starthigh;
		}

		// if totally done, break
		if (h == endhigh)
			break;

		// run consistency checks
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error("trashed sentinal");

		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error("bad size");

		next = (hunk_t *) ((byte *) h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		// print the single block
		memcpy(name, h->name, 8);
		if (all)
			Con_Printf("%8p :%8i %8s\n", h, h->size, name);

		// print the total
		if (next == endlow || next == endhigh || strncmp(h->name, next->name, 8))
		{
			if (!all)
				Con_Printf("          :%8i %8s (TOTAL)\n", sum, name);
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Con_Printf("-------------------------\n");
	Con_Printf("%8i total blocks\n", totalblocks);

}

/*
 ===================
 Hunk_Print_f -- Baker 3.76 - Hunk Print from FitzQuake
 ===================
 */
void Hunk_Print_f(void)
{
	Hunk_Print(false);
}

void *Hunk_AllocName(int size, char *name)
{
	hunk_t *h;

#ifdef PARANOID
	Hunk_Check ();
#endif

	if (size < 0)
		Sys_Error("bad size: %i", size);

	size = sizeof(hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error("Not enough RAM allocated.  Try using \"-mem 64\" on the command line.");
//		Sys_Error ("failed on %i bytes",size);

	h = (hunk_t *) (hunk_base + hunk_low_used);
	hunk_low_used += size;

	Cache_FreeLow(hunk_low_used);

	memset(h, 0, size);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy(h->name, name, 8);

	return (void *) (h + 1);
}

void *Hunk_Alloc(int size)
{
	return Hunk_AllocName(size, "unknown");
}

int Hunk_LowMark(void)
{
	return hunk_low_used;
}

void Hunk_FreeToLowMark(int mark)
{
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error("bad mark %i", mark);
	memset(hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

int Hunk_HighMark(void)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}

	return hunk_high_used;
}

void Hunk_FreeToHighMark(int mark)
{
	if (hunk_tempactive)
	{
		hunk_tempactive = false;
		Hunk_FreeToHighMark(hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error("bad mark %i", mark);
	memset(hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}

void *Hunk_HighAllocName(int size, char *name)
{
	hunk_t *h;

	if (size < 0)
		Sys_Error("bad size: %i", size);

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

#ifdef PARANOID
	Hunk_Check ();
#endif

	size = sizeof(hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error("Not enough RAM allocated.  Try using \"-mem 64\" on the command line.");
//	{
//		Con_Printf ("Hunk_HighAlloc: failed on %i bytes\n",size);
//		return NULL;
//	}

	hunk_high_used += size;
	Cache_FreeHigh(hunk_high_used);

	h = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);

	memset(h, 0, size);
	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy(h->name, name, 8);

	return (void *) (h + 1);
}

/* Return space from the top of the hunk */
void *Hunk_TempAlloc(int size)
{
	void *buf;

	size = (size + 15) & ~15;

	if (hunk_tempactive)
	{
		Hunk_FreeToHighMark(hunk_tempmark);
		hunk_tempactive = false;
	}

	hunk_tempmark = Hunk_HighMark();

	buf = Hunk_HighAllocName(size, "temp");

	hunk_tempactive = true;

	return buf;
}

/*
 ===============================================================================

 CACHE MEMORY

 ===============================================================================
 */

typedef struct cache_system_s
{
	int size;		// including this header
	cache_user_t *user;
	char name[16];
	struct cache_system_s *prev, *next;
	struct cache_system_s *lru_prev, *lru_next;	// for LRU flushing
} cache_system_t;

cache_system_t *Cache_TryAlloc(int size, bool nobottom);

cache_system_t cache_head;

void Cache_Move(cache_system_t *c)
{
	cache_system_t *new_item;

	// we are clearing up space at the bottom, so only allocate it late

	if ((new_item = Cache_TryAlloc(c->size, true)))
	{
//		Con_Printf ("cache_move ok\n");

		memcpy(new_item + 1, c + 1, c->size - sizeof(cache_system_t));
		new_item->user = c->user;
		memcpy(new_item->name, c->name, sizeof(new_item->name));
		Cache_Free(c->user);
		new_item->user->data = (void *) (new_item + 1);
	}
	else
	{
//		Con_Printf ("cache_move failed\n");

		Cache_Free(c->user);		// tough luck...
	}
}

/* Throw things out until the hunk can be expanded to the given point */
void Cache_FreeLow(int new_low_hunk)
{
	cache_system_t *c;

	while (1)
	{
		c = cache_head.next;
		if (c == &cache_head)
			return;		// nothing in cache at all
		if ((byte *) c >= hunk_base + new_low_hunk)
			return;		// there is space to grow the hunk
		Cache_Move(c);	// reclaim the space
	}
}

/* Throw things out until the hunk can be expanded to the given point */
void Cache_FreeHigh(int new_high_hunk)
{
	cache_system_t *c, *prev;

	prev = NULL;
	while (1)
	{
		c = cache_head.prev;
		if (c == &cache_head)
			return;		// nothing in cache at all
		if ((byte *) c + c->size <= hunk_base + hunk_size - new_high_hunk)
			return;		// there is space to grow the hunk
		if (c == prev)
			Cache_Free(c->user);	// didn't move out of the way
		else
		{
			Cache_Move(c);	// try to move it
			prev = c;
		}
	}
}

void Cache_UnlinkLRU(cache_system_t *cs)
{
	if (!cs->lru_next || !cs->lru_prev)
		Sys_Error("NULL link");

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = NULL;
}

void Cache_MakeLRU(cache_system_t *cs)
{
	if (cs->lru_next || cs->lru_prev)
		Sys_Error("active link");

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

/*
 * Looks for a free block of memory between the high and low hunk marks
 * Size should already include the header and padding
 */
cache_system_t *Cache_TryAlloc(int size, bool nobottom)
{
	cache_system_t *cs, *new_item;

	// is the cache completely empty?

	if (!nobottom && cache_head.prev == &cache_head)
	{
		if (hunk_size - hunk_high_used - hunk_low_used < size)
			Sys_Error("%i is greater then free hunk", size);

		new_item = (cache_system_t *) (hunk_base + hunk_low_used);
		memset(new_item, 0, sizeof(*new_item));
		new_item->size = size;

		cache_head.prev = cache_head.next = new_item;
		new_item->prev = new_item->next = &cache_head;

		Cache_MakeLRU(new_item);
		return new_item;
	}

	// search from the bottom up for space

	new_item = (cache_system_t *) (hunk_base + hunk_low_used);
	cs = cache_head.next;

	do
	{
		if (!nobottom || cs != cache_head.next)
		{
			if ((byte *) cs - (byte *) new_item >= size)
			{	// found space
				memset(new_item, 0, sizeof(*new_item));
				new_item->size = size;

				new_item->next = cs;
				new_item->prev = cs->prev;
				cs->prev->next = new_item;
				cs->prev = new_item;

				Cache_MakeLRU(new_item);

				return new_item;
			}
		}

		// continue looking
		new_item = (cache_system_t *) ((byte *) cs + cs->size);
		cs = cs->next;

	} while (cs != &cache_head);

	// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - (byte *) new_item >= size)
	{
		memset(new_item, 0, sizeof(*new_item));
		new_item->size = size;

		new_item->next = &cache_head;
		new_item->prev = cache_head.prev;
		cache_head.prev->next = new_item;
		cache_head.prev = new_item;

		Cache_MakeLRU(new_item);

		return new_item;
	}

	return NULL; // couldn't allocate
}

/* Throw everything out, so new data will be demand cached */
void Cache_Flush_f(void)
{
	while (cache_head.next != &cache_head)
		Cache_Free(cache_head.next->user);	// reclaim the space
}

void Cache_Print(void)
{
	cache_system_t *cd;

	for (cd = cache_head.next; cd != &cache_head; cd = cd->next)
		Con_Printf("%8i : %s\n", cd->size, cd->name);
}

void Cache_Report(void)
{
	Con_DPrintf("%4.1f megabyte data cache\n", (hunk_size - hunk_high_used - hunk_low_used) / (float) (1024 * 1024));
}

void Cache_Init(void)
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand("flush", Cache_Flush_f);
}

/* Frees the memory and removes it from the LRU list */
void Cache_Free(cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		Sys_Error("not allocated");

	cs = ((cache_system_t *) c->data) - 1;

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU(cs);
}

void *Cache_Check(cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		return NULL;

	cs = ((cache_system_t *) c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU(cs);
	Cache_MakeLRU(cs);

	return c->data;
}

void *Cache_Alloc(cache_user_t *c, int size, char *name)
{
	cache_system_t *cs;

	if (c->data)
		Sys_Error("already allocated");

	if (size <= 0)
		Sys_Error("size %i", size);

	size = (size + sizeof(cache_system_t) + 15) & ~15;

	// find memory for it
	while (true)
	{
		if ((cs = Cache_TryAlloc(size, false)))
		{
			strncpy(cs->name, name, sizeof(cs->name) - 1);
			c->data = (void *) (cs + 1);
			cs->user = c;
			break;
		}

		// free the least recently used cahedat
		if (cache_head.lru_prev == &cache_head)
			Sys_Error("out of memory"); // not enough memory at all

		Cache_Free(cache_head.lru_prev->user);
	}

	return Cache_Check(c);
}

//============================================================================

void Memory_Init(void *buf, int size)
{
	hunk_base = (byte *)buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;

	Cache_Init();

	Cmd_AddCommand("hunk_print", Hunk_Print_f);
}
