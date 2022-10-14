/*
 * Primary header for client
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

#define	QUAKE_GAME // as opposed to utilities

#include "version.h"

#define	GAMENAME "id1" // directory to look in by default

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <vector>

#define BIT(x) (1 << (x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define	MINIMUM_MEMORY		0x550000
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x100000)

#define MAX_NUM_ARGVS		50

// up / down
#define	PITCH			0

// left / right
#define	YAW			1

// fall over
#define	ROLL			2

#define	MAX_QPATH		64	// max length of a quake game pathname
#define	MAX_OSPATH		128	// max length of a filesystem pathname

#define	ON_EPSILON		0.1	// point on plane side epsilon

#define	MAX_MSGLEN		8000	// max length of a reliable message
#define	MAX_DATAGRAM		1024	// max length of unreliable message

// per-level limits
#define	MAX_EDICTS              2048
#define	MAX_LIGHTSTYLES         64
#define	MAX_MODELS              2048 // these are sent over the net as bytes
#define	MAX_SOUNDS              2048 // so they cannot be blindly increased

#define	SAVEGAME_COMMENT_LENGTH 39

#define	MAX_STYLESTRING		64

#define	MAXPRINTMSG		4096

// stats are integers communicated to the client by the server
#define	MAX_CL_STATS		32
#define	STAT_HEALTH		0
#define	STAT_FRAGS		1
#define	STAT_WEAPON		2
#define	STAT_AMMO		3
#define	STAT_ARMOR		4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS		6
#define	STAT_NAILS		7
#define	STAT_ROCKETS		8
#define	STAT_CELLS		9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13	// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14	// bumped by svc_killedmonster

// stock defines
#define	IT_SHOTGUN		BIT(0)
#define	IT_SUPER_SHOTGUN	BIT(1)
#define	IT_NAILGUN		BIT(2)
#define	IT_SUPER_NAILGUN	BIT(3)
#define	IT_GRENADE_LAUNCHER	BIT(4)
#define	IT_ROCKET_LAUNCHER	BIT(5)
#define	IT_LIGHTNING		BIT(6)
#define IT_SUPER_LIGHTNING      BIT(7)
#define IT_SHELLS               BIT(8)
#define IT_NAILS                BIT(9)
#define IT_ROCKETS              BIT(10)
#define IT_CELLS                BIT(11)
#define IT_AXE                  BIT(12)
#define IT_ARMOR1               BIT(13)
#define IT_ARMOR2               BIT(14)
#define IT_ARMOR3               BIT(15)
#define IT_SUPERHEALTH          BIT(16)
#define IT_KEY1                 BIT(17)
#define IT_KEY2                 BIT(18)
#define	IT_INVISIBILITY		BIT(19)
#define	IT_INVULNERABILITY	BIT(20)
#define	IT_SUIT			BIT(21)
#define	IT_QUAD			BIT(22)
#define IT_SIGIL1               BIT(28)
#define IT_SIGIL2               BIT(29)
#define IT_SIGIL3               BIT(30)
#define IT_SIGIL4               BIT(31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              BIT(7)
#define RIT_NAILS               BIT(8)
#define RIT_ROCKETS             BIT(9)
#define RIT_CELLS               BIT(10)
#define RIT_AXE                 BIT(11)
#define RIT_LAVA_NAILGUN        BIT(12)
#define RIT_LAVA_SUPER_NAILGUN  BIT(13)
#define RIT_MULTI_GRENADE       BIT(14)
#define RIT_MULTI_ROCKET        BIT(15)
#define RIT_PLASMA_GUN          BIT(16)
#define RIT_ARMOR1              BIT(23)
#define RIT_ARMOR2              BIT(24)
#define RIT_ARMOR3              BIT(25)
#define RIT_LAVA_NAILS          BIT(26)
#define RIT_PLASMA_AMMO         BIT(27)
#define RIT_MULTI_ROCKETS       BIT(28)
#define RIT_SHIELD              BIT(29)
#define RIT_ANTIGRAV            BIT(30)
#define RIT_SUPERHEALTH         BIT(31)

//===========================================
//MED 01/04/97 added hipnotic defines

#define HIT_PROXIMITY_GUN_BIT	16
#define HIT_MJOLNIR_BIT		7
#define HIT_LASER_CANNON_BIT	23
#define HIT_PROXIMITY_GUN	BIT(HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR		BIT(HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON	BIT(HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT		BIT(23+2)
#define HIT_EMPATHY_SHIELDS	BIT(23+3)

//===========================================

#define	MAX_SCOREBOARD		16
#define	MAX_SCOREBOARDNAME	32

#define	SOUND_CHANNELS		8

#include "common.h"
#include "vid.h"
#include "sys.h"
#include "zone.h"
#include "mathlib.h"
#include "matrix.h"
#include "wad.h"
#include "cvar.h"
#include "host.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "client.h"
#include "progs.h"
#include "server.h"
#include "model.h"
#include "input.h"
#include "sv_world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "crc.h"
#include "chase.h"
