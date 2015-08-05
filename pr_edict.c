/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_edict.c -- entity dictionary

#include "quakedef.h"

dprograms_t		*progs;
dfunction_t		*pr_functions;

char		*pr_strings;
static	int		pr_stringssize;
static	char	**pr_knownstrings;
static	int		pr_maxknownstrings;
static	int		pr_numknownstrings;
ddef_t		*pr_fielddefs;
ddef_t		*pr_globaldefs;

dstatement_t	*pr_statements;
globalvars_t	*pr_global_struct;
float			*pr_globals;			// same as pr_global_struct
int				pr_edict_size;	// in bytes

unsigned short		pr_crc;

int		type_size[8] = {1,sizeof(string_t)/4,1,3,1,1,sizeof(func_t)/4,sizeof(void *)/4};

ddef_t *ED_FieldAtOfs (int ofs);
qboolean	ED_ParseEpair (void *base, ddef_t *key, char *s);

cvar_t	nomonsters = {"nomonsters", "0"};
cvar_t	gamecfg = {"gamecfg", "0"};
cvar_t	scratch1 = {"scratch1", "0"};
cvar_t	scratch2 = {"scratch2", "0"};
cvar_t	scratch3 = {"scratch3", "0"};
cvar_t	scratch4 = {"scratch4", "0"};
cvar_t	savedgamecfg = {"savedgamecfg", "0", true};
cvar_t	saved1 = {"saved1", "0", true};
cvar_t	saved2 = {"saved2", "0", true};
cvar_t	saved3 = {"saved3", "0", true};
cvar_t	saved4 = {"saved4", "0", true};

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct {
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache	gefvCache[GEFV_CACHESIZE] = {{NULL, ""}, {NULL, ""}};

// evaluation shortcuts
int	eval_gravity, eval_items2, eval_ammo_shells1, eval_ammo_nails1;
int	eval_ammo_lava_nails, eval_ammo_rockets1, eval_ammo_multi_rockets;
int	eval_ammo_cells1, eval_ammo_plasma;

// nehahra specific
int	eval_alpha, eval_fullbright;

ddef_t *ED_FindField (char *name);

int FindFieldOffset (char *field)
{
	ddef_t	*d;

	if (!(d = ED_FindField(field)))
		return 0;

	return d->ofs*4;
}

void FindEdictFieldOffsets (void)
{
	eval_gravity = FindFieldOffset ("gravity");
	eval_items2 = FindFieldOffset ("items2");
	eval_ammo_shells1 = FindFieldOffset ("ammo_shells1");
	eval_ammo_nails1 = FindFieldOffset ("ammo_nails1");
	eval_ammo_lava_nails = FindFieldOffset ("ammo_lava_nails");
	eval_ammo_rockets1 = FindFieldOffset ("ammo_rockets1");
	eval_ammo_multi_rockets = FindFieldOffset ("ammo_multi_rockets");
	eval_ammo_cells1 = FindFieldOffset ("ammo_cells1");
	eval_ammo_plasma = FindFieldOffset ("ammo_plasma");

	eval_alpha = FindFieldOffset ("alpha");
	eval_fullbright = FindFieldOffset ("fullbright");
}

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
void ED_ClearEdict (edict_t *e)
{
	memset (&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED_Alloc (void)
{
	int			i;
	edict_t		*e;

	for ( i=svs.maxclients+1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) )
		{
			ED_ClearEdict (e);
			return e;
		}
	}

	if (i == MAX_EDICTS)
		Sys_Error ("ED_Alloc: no free edicts");

	sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict (e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (edict_t *ed)
{
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy (vec3_origin, ed->v.origin);
	VectorCopy (vec3_origin, ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef_t *ED_GlobalAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}

	return NULL;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t *ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}

	return NULL;
}

/*
============
ED_FindField
============
*/
ddef_t *ED_FindField (char *name)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if ( !strcmp(PR_GetString(def->s_name), name) )
			return def;
	}

	return NULL;
}


/*
============
ED_FindGlobal
============
*/
ddef_t *ED_FindGlobal (char *name)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if ( !strcmp(PR_GetString(def->s_name), name) )
			return def;
	}

	return NULL;
}


/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction (char *name)
{
	dfunction_t		*func;
	int				i;

	for (i=0 ; i<progs->numfunctions ; i++)
	{
		func = &pr_functions[i];
		if ( !strcmp(PR_GetString(func->s_name), name) )
			return func;
	}

		return NULL;
}


/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		SNPrintf(line, sizeof(line), "%s", PR_GetString(val->string));
		break;

	case ev_entity:
		SNPrintf(line, sizeof(line), "entity %i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)) );
		break;

	case ev_function:
		f = pr_functions + val->function;
		SNPrintf(line, sizeof(line), "%s()", PR_GetString(f->s_name));
		break;

	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		SNPrintf(line, sizeof(line), ".%s", PR_GetString(def->s_name));
		break;

	case ev_void:
		SNPrintf(line, sizeof(line), "void");
		break;

	case ev_float:
		SNPrintf(line, sizeof(line), "%5.1f", val->_float);
		break;

	case ev_vector:
		SNPrintf(line, sizeof(line), "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;

	case ev_pointer:
		SNPrintf(line, sizeof(line), "pointer");
		break;

	default:
		SNPrintf(line, sizeof(line), "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PR_UglyValueString (etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		SNPrintf(line, sizeof(line), "%s", PR_GetString(val->string));
		break;

	case ev_entity:
		SNPrintf(line, sizeof(line), "%i", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;

	case ev_function:
		f = pr_functions + val->function;
		SNPrintf(line, sizeof(line), "%s", PR_GetString(f->s_name));
		break;

	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		SNPrintf(line, sizeof(line), "%s", PR_GetString(def->s_name));
		break;

	case ev_void:
		SNPrintf(line, sizeof(line), "void");
		break;

	case ev_float:
		SNPrintf(line, sizeof(line), "%f", val->_float);
		break;

	case ev_vector:
		SNPrintf(line, sizeof(line), "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;

	default:
		SNPrintf(line, sizeof(line), "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
============
*/
char *PR_GlobalString (int ofs)
{
	char	*s;
	int		i;
	ddef_t	*def;
	void	*val;
	static char	line[128];

	val = (void *)&pr_globals[ofs];
	if (!(def = ED_GlobalAtOfs(ofs)))
	{
		SNPrintf(line, sizeof(line),"%i(\?\?\?)", ofs);
	}
	else
	{
		s = PR_ValueString (def->type, val);
		SNPrintf(line, sizeof(line),"%i(%s)%s", ofs, PR_GetString(def->s_name), s);
	}

	i = strlen(line);
	for ( ; i<20 ; i++)
		strlcat (line, " ", sizeof(line));
	strlcat (line, " ", sizeof(line));

	return line;
}

char *PR_GlobalStringNoContents (int ofs)
{
	int		i;
	ddef_t	*def;
	static char	line[128];

	if (!(def = ED_GlobalAtOfs(ofs)))
		SNPrintf(line, sizeof(line),"%i(\?\?\?)", ofs);
	else
		SNPrintf(line, sizeof(line),"%i(%s)", ofs, PR_GetString(def->s_name));

	i = strlen(line);
	for ( ; i<20 ; i++)
		strlcat (line, " ", sizeof(line));
	strlcat (line, " ", sizeof(line));

	return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print (edict_t *ed)
{
	int		l, *v, i, j, type;
	ddef_t	*d;
	char	*name;

	if (ed->free)
	{
		Con_SafePrintf ("FREE\n");
		return;
	}

	Con_SafePrintf("\nEDICT %i:\n", NUM_FOR_EDICT(ed));	// Baker: make it speedy!
	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = PR_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Con_SafePrintf ("%s",name);  // Baker: make it speedy!
		l = strlen (name);
		while (l++ < 15)
			Con_SafePrintf (" ");

		Con_SafePrintf ("%s\n", PR_ValueString(d->type, (eval_t *)v));
	}
}

/*
=============
ED_Write

For savegames
=============
*/
void ED_Write (FILE *f, edict_t *ed)
{
	ddef_t	*d;
	int	*v, i, j, type;
	char	*name;

	fprintf (f, "{\n");

	if (ed->free)
	{
		fprintf (f, "}\n");
		return;
	}

	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = PR_GetString(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		fprintf (f,"\"%s\" ",name);
		fprintf (f,"\"%s\"\n", PR_UglyValueString(d->type, (eval_t *)v));
	}

	fprintf (f, "}\n");
}

void ED_PrintNum (int ent)
{
	ED_Print (EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts_f

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts_f (void)
{
	int		i;

	Con_SafePrintf ("%i entities\n", sv.num_edicts);
	for (i=0 ; i<sv.num_edicts ; i++)
		ED_PrintNum (i);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edicy
=============
*/
void ED_PrintEdict_f (void)
{
	int		i;

	i = atoi (Cmd_Argv(1));
	if (i >= sv.num_edicts)
	{
		Con_SafePrintf("Bad edict number\n");
		return;
	}
	ED_PrintNum (i);
}

/*
=============
ED_Count_f

For debugging
=============
*/
void ED_Count_f (void)
{
	int		i, active, models, solid, step;
	edict_t	*ent;

	active = models = solid = step = 0;
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(i);
		if (ent->free)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Con_SafePrintf ("num_edicts:%3i\n", sv.num_edicts);
	Con_SafePrintf ("active    :%3i\n", active);
	Con_SafePrintf ("view      :%3i\n", models);
	Con_SafePrintf ("touch     :%3i\n", solid);
	Con_SafePrintf ("step      :%3i\n", step);

}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_WriteGlobals
=============
*/
void ED_WriteGlobals (FILE *f)
{
	ddef_t		*def;
	int			i, type;
	char		*name;

	fprintf (f,"{\n");
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		type = def->type;
		if ( !(def->type & DEF_SAVEGLOBAL) )
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString(def->s_name);
		fprintf (f,"\"%s\" ", name);
		fprintf (f,"\"%s\"\n", PR_UglyValueString(type, (eval_t *)&pr_globals[def->ofs]));
	}
	fprintf (f,"}\n");
}

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals (char *data)
{
	char	keyname[64];
	ddef_t	*key;

	while (1)
	{
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		strcpy (keyname, com_token);

	// parse value
		if (!(data = COM_Parse (data)))
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error ("ED_ParseEntity: closing brace without data");

		if (!(key = ED_FindGlobal (keyname)))
		{
			Con_SafePrintf ("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair ((void *)pr_globals, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
int ED_NewString (char *string)
{
	char	*new_p;
	int		i,l,new;

	l = strlen(string) + 1;
	new = PR_AllocString (l, &new_p);

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			*new_p++ = (string[i] == 'n') ? '\n' : '\\';
		}
		else
			*new_p++ = string[i];
	}

	return new;
}


/*
==============
ED_ParseEpair

Can parse either fields or globals
returns false if error
==============
*/
qboolean	ED_ParseEpair (void *base, ddef_t *key, char *s)
{
	int		i;
	char	string[128];
	ddef_t	*def;
	char	*v, *w;
	void	*d;
	dfunction_t	*func;

	d = (void *)((int *)base + key->ofs);

	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		*(string_t *)d = ED_NewString(s);
		break;

	case ev_float:
		*(float *)d = atof (s);
		break;

	case ev_vector:
		strcpy (string, s);
		v = string;
		w = string;
		for (i=0 ; i<3 ; i++)
		{
			while (*v && *v != ' ')
				v++;
			*v = 0;
			((float *)d)[i] = atof (w);
			w = v = v+1;
		}
		break;

	case ev_entity:
		*(int *)d = EDICT_TO_PROG(EDICT_NUM(atoi (s)));
		break;

	case ev_field:
		if (!(def = ED_FindField (s)))
		{
			Con_SafePrintf ("Can't find field %s\n", s);
			return false;
		}
		*(int *)d = G_INT(def->ofs);
		break;

	case ev_function:
		if (!(func = ED_FindFunction (s)))
		{
			Con_SafePrintf ("Can't find function %s\n", s);
			return false;
		}
		*(func_t *)d = func - pr_functions;
		break;

	default:
		break;
	}

	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	ddef_t		*key;
	qboolean	anglehack;
	qboolean	init;
	char		keyname[256];
	int			n;

	init = false;

// clear it
	if (ent != sv.edicts)	// hack
		memset (&ent->v, 0, progs->entityfields * 4);

// go through all the dictionary pairs
	while (1)
	{
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

// anglehack is to allow QuakeEd to write single scalar angles
// and allow them to be turned into vectors. (FIXME...)
if (!strcmp(com_token, "angle"))
{
	strcpy (com_token, "angles");
	anglehack = true;
}
else
	anglehack = false;

// FIXME: change light to _light to get rid of this hack
if (!strcmp(com_token, "light"))
	strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix keynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

	// parse value
		if (!(data = COM_Parse (data)))
			Sys_Error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error ("ED_ParseEntity: closing brace without data");

		init = true;

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		if (!(key = ED_FindField (keyname)))
		{
			if (strcmp(keyname, "sky") && strcmp(keyname, "fog")) // Now supported in worldspawn
			Con_SafePrintf ("'%s' is not a field\n", keyname);
			continue;
		}

		if (anglehack)
		{
			char	temp[32];

			strncpy (temp, com_token, sizeof(temp));
			SNPrintf (com_token, sizeof(com_token), "0 %s 0", temp);
		}

		if (!ED_ParseEpair ((void *)&ent->v, key, com_token))
			Host_Error ("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = true;

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile (char *data)
{
	edict_t		*ent;
	int			inhibit;
	dfunction_t	*func;

	ent = NULL;
	inhibit = 0;
	pr_global_struct->time = sv.time;

// parse ents
	while (1)
	{
// parse the opening brace
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Sys_Error ("ED_LoadFromFile: found %s when expecting {",com_token);

		ent = (!ent) ? EDICT_NUM(0) : ED_Alloc ();
		data = ED_ParseEdict (data, ent);

// remove things from different skill levels or deathmatch
		if (deathmatch.value)
		{
			if (((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
			{
				ED_Free (ent);
				inhibit++;
				continue;
			}
		}
		else if ((current_skill == 0 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD)) )
		{
			ED_Free (ent);
			inhibit++;
			continue;
		}

// immediately call spawn function
		if (!ent->v.classname)
		{
			Con_SafePrintf ("No classname for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

	// look for the spawn function
		func = ED_FindFunction ( PR_GetString(ent->v.classname) );

		if (!func)
		{
			Con_SafePrintf ("No spawn function for:\n"); // Baker: makes it speedy!
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram (func - pr_functions);
	}

	Con_DPrintf ("%i entities inhibited\n", inhibit);
}


/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs (char *progsname)
{
	int		i;

// flush the non-C variable lookup cache
	for (i=0 ; i<GEFV_CACHESIZE ; i++)
		gefvCache[i].field[0] = 0;

	if (!progsname || !*progsname)
		Host_Error("PR_LoadProgs: passed empty progsname");

	CRC_Init (&pr_crc);

	if (!(progs = (dprograms_t *)COM_LoadHunkFile (progsname)))
		Sys_Error ("PR_LoadProgs: couldn't load %s", progsname);

	Con_DPrintf ("Programs occupy %iK.\n", com_filesize/1024);

	for (i=0 ; i<com_filesize ; i++)
		CRC_ProcessByte (&pr_crc, ((byte *)progs)[i]);

// byte swap the header
	for (i=0 ; i<sizeof(*progs)/4 ; i++)
		((int *)progs)[i] = LittleLong ( ((int *)progs)[i] );

	if (progs->version != PROG_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)", progsname, progs->version, PROG_VERSION);

	if (progs->crc != PROGHEADER_CRC)
		Sys_Error ("%s system vars have been modified, progdefs.h is out of date", progsname);

	pr_functions = (dfunction_t *)((byte *)progs + progs->ofs_functions);
	pr_strings = (char *)progs + progs->ofs_strings;
	if (progs->ofs_strings + progs->numstrings >= com_filesize)
		Host_Error ("progs.dat strings go past end of file\n");

	// initialize the strings
	pr_numknownstrings = 0;
	pr_maxknownstrings = 0;
	pr_stringssize = progs->numstrings;
	if (pr_knownstrings)
		Z_Free ((void *)pr_knownstrings);
	pr_knownstrings = NULL;
	PR_SetEngineString("");

	pr_globaldefs = (ddef_t *)((byte *)progs + progs->ofs_globaldefs);
	pr_fielddefs = (ddef_t *)((byte *)progs + progs->ofs_fielddefs);
	pr_statements = (dstatement_t *)((byte *)progs + progs->ofs_statements);

	pr_global_struct = (globalvars_t *)((byte *)progs + progs->ofs_globals);
	pr_globals = (float *)pr_global_struct;

	pr_edict_size = progs->entityfields * 4 + sizeof (edict_t) - sizeof(entvars_t);

// byte swap the lumps
	for (i=0 ; i<progs->numstatements ; i++)
	{
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (i=0 ; i<progs->numfunctions; i++)
	{
	pr_functions[i].first_statement = LittleLong (pr_functions[i].first_statement);
	pr_functions[i].parm_start = LittleLong (pr_functions[i].parm_start);
	pr_functions[i].s_name = LittleLong (pr_functions[i].s_name);
	pr_functions[i].s_file = LittleLong (pr_functions[i].s_file);
	pr_functions[i].numparms = LittleLong (pr_functions[i].numparms);
	pr_functions[i].locals = LittleLong (pr_functions[i].locals);
	}

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		pr_globaldefs[i].type = LittleShort (pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort (pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong (pr_globaldefs[i].s_name);
	}

	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		pr_fielddefs[i].type = LittleShort (pr_fielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Sys_Error ("PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = LittleShort (pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong (pr_fielddefs[i].s_name);
	}

	for (i=0 ; i<progs->numglobals ; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);

	FindEdictFieldOffsets ();
}


/*
===============
PR_Init
===============
*/
void PR_Init (void)
{
	Cmd_AddCommand ("edict", ED_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED_PrintEdicts_f);
	Cmd_AddCommand ("edictcount", ED_Count_f);
	Cmd_AddCommand ("profile", PR_Profile_f);
	Cvar_RegisterVariable (&nomonsters, NULL);
	Cvar_RegisterVariable (&gamecfg, NULL);
	Cvar_RegisterVariable (&scratch1, NULL);
	Cvar_RegisterVariable (&scratch2, NULL);
	Cvar_RegisterVariable (&scratch3, NULL);
	Cvar_RegisterVariable (&scratch4, NULL);
	Cvar_RegisterVariable (&savedgamecfg, NULL);
	Cvar_RegisterVariable (&saved1, NULL);
	Cvar_RegisterVariable (&saved2, NULL);
	Cvar_RegisterVariable (&saved3, NULL);
	Cvar_RegisterVariable (&saved4, NULL);
}


edict_t *EDICT_NUM(int n)
{
	if (n < 0 || n >= sv.max_edicts)
		Sys_Error ("EDICT_NUM: bad number %i", n);

	return (edict_t *)((byte *)sv.edicts+ (n)*pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e)
{
	int		b;

	b = (byte *)e - (byte *)sv.edicts;
	b = b / pr_edict_size;

	if (b < 0 || b >= sv.num_edicts)
		Sys_Error ("NUM_FOR_EDICT: bad pointer");

	return b;
}

//===========================================================================


#define	PR_STRING_ALLOCSLOTS	1024

static void PR_AllocStringSlots (void)
{
	pr_maxknownstrings += PR_STRING_ALLOCSLOTS;
	//Con_DPrintf2("PR_AllocStringSlots: realloc'ing for %d slots\n", pr_maxknownstrings);
	//pr_knownstrings = (const char **) Z_Realloc ((void *)pr_knownstrings, pr_maxknownstrings * sizeof(char *));
	pr_knownstrings = (char **) malloc (pr_maxknownstrings * sizeof(char *));
}

char *PR_GetString (int num)
{
	if (num >= 0 && num < pr_stringssize)
		return pr_strings + num;
	else if (num < 0 && num >= -pr_numknownstrings)
	{
		if (!pr_knownstrings[-1 - num])
		{
			Host_Error ("PR_GetString: attempt to get a non-existant string %d\n", num);
			return "";
		}
		return pr_knownstrings[-1 - num];
	}
	else
	{
		Host_Error("PR_GetString: invalid string offset %d\n", num);
		return "";
	}
}

int PR_SetEngineString (char *s)
{
	int		i;

	if (!s)
		return 0;
#if 0	/* can't: sv.model_precache & sv.sound_precache points to pr_strings */
	if (s >= pr_strings && s <= pr_strings + pr_stringssize)
		Host_Error("PR_SetEngineString: \"%s\" in pr_strings area\n", s);
#else
	if (s >= pr_strings && s <= pr_strings + pr_stringssize - 2)
		return (int)(s - pr_strings);
#endif
	for (i = 0; i < pr_numknownstrings; i++)
	{
		if (pr_knownstrings[i] == s)
			return -1 - i;
	}
	// new unknown engine string
	//Con_DPrintf ("PR_SetEngineString: new engine string %p\n", s);
#if 0
	for (i = 0; i < pr_numknownstrings; i++)
	{
		if (!pr_knownstrings[i])
			break;
	}
#endif
//	if (i >= pr_numknownstrings)
//	{
		if (i >= pr_maxknownstrings)
			PR_AllocStringSlots();
		pr_numknownstrings++;
//	}
	pr_knownstrings[i] = s;
	return -1 - i;
}

int PR_AllocString (int size, char **ptr)
{
	int		i;

	if (!size)
		return 0;
	for (i = 0; i < pr_numknownstrings; i++)
	{
		if (!pr_knownstrings[i])
			break;
	}
//	if (i >= pr_numknownstrings)
//	{
		if (i >= pr_maxknownstrings)
			PR_AllocStringSlots();
		pr_numknownstrings++;
//	}
	pr_knownstrings[i] = (char *)Hunk_AllocName(size, "string");
	if (ptr)
		*ptr = (char *) pr_knownstrings[i];
	return -1 - i;
}

