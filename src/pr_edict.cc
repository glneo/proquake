/*
 * Entity dictionary
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

#include "quakedef.h"

dprograms_t *progs;
dfunction_t *pr_functions;

char *pr_strings;
static int pr_stringssize;
static const char **pr_knownstrings;
static int pr_maxknownstrings;
static int pr_numknownstrings;
ddef_t *pr_fielddefs;
ddef_t *pr_globaldefs;

dstatement_t *pr_statements;
globalvars_t *pr_global_struct;
float *pr_globals; // same as pr_global_struct
int pr_edict_size; // in bytes

unsigned short pr_crc;

int type_size[8] = { 1, sizeof(string_t) / 4, 1, 3, 1, 1, sizeof(func_t) / 4, sizeof(void *) / 4 };

ddef_t *ED_FieldAtOfs(int ofs);

cvar_t nomonsters = { "nomonsters", "0" };
cvar_t gamecfg = { "gamecfg", "0" };
cvar_t scratch1 = { "scratch1", "0" };
cvar_t scratch2 = { "scratch2", "0" };
cvar_t scratch3 = { "scratch3", "0" };
cvar_t scratch4 = { "scratch4", "0" };
cvar_t savedgamecfg = { "savedgamecfg", "0", true };
cvar_t saved1 = { "saved1", "0", true };
cvar_t saved2 = { "saved2", "0", true };
cvar_t saved3 = { "saved3", "0", true };
cvar_t saved4 = { "saved4", "0", true };

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct
{
	ddef_t *pcache;
	char field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE] = { { NULL, "" }, { NULL, "" } };

// evaluation shortcuts
int eval_gravity, eval_items2, eval_ammo_shells1, eval_ammo_nails1;
int eval_ammo_lava_nails, eval_ammo_rockets1, eval_ammo_multi_rockets;
int eval_ammo_cells1, eval_ammo_plasma;

// nehahra specific
int eval_alpha, eval_fullbright;

ddef_t *ED_FindField(const char *name);

int FindFieldOffset(const char *field)
{
	ddef_t *d;

	if (!(d = ED_FindField(field)))
		return 0;

	return d->ofs * 4;
}

void FindEdictFieldOffsets(void)
{
	eval_gravity = FindFieldOffset("gravity");
	eval_items2 = FindFieldOffset("items2");
	eval_ammo_shells1 = FindFieldOffset("ammo_shells1");
	eval_ammo_nails1 = FindFieldOffset("ammo_nails1");
	eval_ammo_lava_nails = FindFieldOffset("ammo_lava_nails");
	eval_ammo_rockets1 = FindFieldOffset("ammo_rockets1");
	eval_ammo_multi_rockets = FindFieldOffset("ammo_multi_rockets");
	eval_ammo_cells1 = FindFieldOffset("ammo_cells1");
	eval_ammo_plasma = FindFieldOffset("ammo_plasma");

	eval_alpha = FindFieldOffset("alpha");
	eval_fullbright = FindFieldOffset("fullbright");
}

//===========================================================================


#define	PR_STRING_ALLOCSLOTS 1024

static void PR_AllocStringSlots(void)
{
	pr_maxknownstrings += PR_STRING_ALLOCSLOTS;
	Con_DPrintf("PR_AllocStringSlots: realloc'ing for %d slots\n", pr_maxknownstrings);
	pr_knownstrings = (const char **) Q_realloc((void *)pr_knownstrings, pr_maxknownstrings * sizeof(char *));
}

static int PR_AllocString(int size, char **ptr)
{
	int i;

	if (!size)
		return 0;
	for (i = 0; i < pr_numknownstrings; i++)
	{
		if (!pr_knownstrings[i])
			break;
	}
	if (i >= pr_numknownstrings)
	{
		if (i >= pr_maxknownstrings)
			PR_AllocStringSlots();

		pr_knownstrings[i] = (char *) Hunk_AllocName(size, "string");
		pr_numknownstrings++;
	}

	if (ptr)
		*ptr = (char *) pr_knownstrings[i];

	return -1 - i;
}

/* returns a copy of the string allocated from the server's string heap */
static int ED_NewString(char *string)
{
	char *new_p;
	int l = strlen(string) + 1;
	int new_int = PR_AllocString(l, &new_p);

	for (int i = 0; i < l; i++)
	{
		if (string[i] == '\\' && i < l - 1)
		{
			i++;
			*new_p++ = (string[i] == 'n') ? '\n' : '\\';
		}
		else
			*new_p++ = string[i];
	}

	return new_int;
}

const char *PR_GetString(int num)
{
	if (num >= 0 && num < pr_stringssize)
		return pr_strings + num;
	else if (num < 0 && num >= -pr_numknownstrings)
	{
		if (!pr_knownstrings[-1 - num])
		{
			Host_Error("PR_GetString: attempt to get a non-existant string %d\n", num);
			return NULL;
		}
		return pr_knownstrings[-1 - num];
	}
	else
	{
		Host_Error("PR_GetString: invalid string offset %d\n", num);
		return NULL;
	}
}

int PR_SetEngineString(const char *s)
{
	if (!s)
		return 0;

	if (s >= pr_strings && s <= pr_strings + pr_stringssize - 2)
		return (int) (s - pr_strings);

	int i;
	for (i = 0; i < pr_numknownstrings; i++)
	{
		if (pr_knownstrings[i] == s)
			return -1 - i;
	}

	// new unknown engine string
	Con_DPrintf("PR_SetEngineString: new engine string %s\n", s);

	if (i >= pr_maxknownstrings)
		PR_AllocStringSlots();

	pr_knownstrings[i] = s;
	pr_numknownstrings++;

	return -1 - i;
}



// ----------------------------------------------------

/* Sets everything to NULL */
static void ED_ClearEdict(edict_t *e)
{
	memset(&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
 * Either finds a free edict, or allocates a new one.
 * Try to avoid reusing an entity that was recently freed, because it
 * can cause the client to think the entity morphed into something else
 * instead of being removed and recreated, which can cause interpolated
 * angles and bad trails.
 */
edict_t *ED_Alloc(void)
{
	int i;
	edict_t *e;

	for (i = svs.maxclients + 1; i < sv.num_edicts; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || sv.time - e->freetime > 0.5))
		{
			ED_ClearEdict(e);
			return e;
		}
	}
	if (i == MAX_EDICTS)
		Sys_Error("no free edicts");

	sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict(e);

	return e;
}

/* Marks the edict as free */
/* FIXME: walk all entities and NULL out references to this entity */
void ED_Free(edict_t *ed)
{
	SV_UnlinkEdict(ed); // unlink from world bsp

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy(vec3_origin, ed->v.origin);
	VectorCopy(vec3_origin, ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

//===========================================================================

ddef_t *ED_GlobalAtOfs(int ofs)
{
	for (int i = 0; i < progs->numglobaldefs; i++)
	{
		ddef_t *def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}

	return NULL;
}

ddef_t *ED_FieldAtOfs(int ofs)
{
	for (int i = 0; i < progs->numfielddefs; i++)
	{
		ddef_t *def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}

	return NULL;
}

ddef_t *ED_FindGlobal(char *name)
{
	for (int i = 0; i < progs->numglobaldefs; i++)
	{
		ddef_t *def = &pr_globaldefs[i];
		if (!strcmp(PR_GetString(def->s_name), name))
			return def;
	}

	return NULL;
}

ddef_t *ED_FindField(const char *name)
{
	for (int i = 0; i < progs->numfielddefs; i++)
	{
		ddef_t *def = &pr_fielddefs[i];
		if (!strcmp(PR_GetString(def->s_name), name))
			return def;
	}

	return NULL;
}

dfunction_t *ED_FindFunction(const char *name)
{
	for (int i = 0; i < progs->numfunctions; i++)
	{
		dfunction_t *func = &pr_functions[i];
		if (!strcmp(PR_GetString(func->s_name), name))
			return func;
	}

	return NULL;
}

/* Returns a string describing *data in a type specific manner */
char *PR_ValueString(etype_t type, eval_t *val)
{
	static char line[256];
	ddef_t *def;
	dfunction_t *f;

	type = (etype_t)(type & ~DEF_SAVEGLOBAL);

	switch (type)
	{
	case ev_string:
		snprintf(line, sizeof(line), "%s", PR_GetString(val->string));
		break;

	case ev_entity:
		snprintf(line, sizeof(line), "entity %li", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;

	case ev_function:
		f = pr_functions + val->function;
		snprintf(line, sizeof(line), "%s()", PR_GetString(f->s_name));
		break;

	case ev_field:
		def = ED_FieldAtOfs(val->_int);
		snprintf(line, sizeof(line), ".%s", PR_GetString(def->s_name));
		break;

	case ev_void:
		snprintf(line, sizeof(line), "void");
		break;

	case ev_float:
		snprintf(line, sizeof(line), "%5.1f", val->_float);
		break;

	case ev_vector:
		snprintf(line, sizeof(line), "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;

	case ev_pointer:
		snprintf(line, sizeof(line), "pointer");
		break;

	default:
		snprintf(line, sizeof(line), "bad type %i", type);
		break;
	}

	return line;
}

/* Returns a string with a description and the contents of a global */
char *PR_GlobalString(int ofs)
{
	static char line[128];

	ddef_t *def = ED_GlobalAtOfs(ofs);
	if (!def)
	{
		snprintf(line, sizeof(line), "%i(\?\?\?)", ofs);
	}
	else
	{
		void *val = (void *) &pr_globals[ofs];
		char *s = PR_ValueString((etype_t)def->type, (eval_t *)val);
		snprintf(line, sizeof(line), "%i(%s)%s", ofs, PR_GetString(def->s_name), s);
	}

	for (int i = strlen(line); i < 20; i++)
		strlcat(line, " ", sizeof(line));
	strlcat(line, " ", sizeof(line));

	return line;
}

char *PR_GlobalStringNoContents(int ofs)
{
	int i;
	ddef_t *def;
	static char line[128];

	if (!(def = ED_GlobalAtOfs(ofs)))
		snprintf(line, sizeof(line), "%i(\?\?\?)", ofs);
	else
		snprintf(line, sizeof(line), "%i(%s)", ofs, PR_GetString(def->s_name));

	i = strlen(line);
	for (; i < 20; i++)
		strlcat(line, " ", sizeof(line));
	strlcat(line, " ", sizeof(line));

	return line;
}

/*
 * Returns a string describing *data in a type specific manner
 * Easier to parse than PR_ValueString
 */
static char *PR_UglyValueString(etype_t type, eval_t *val)
{
	static char line[256];
	ddef_t *def;
	dfunction_t *f;

	type = (etype_t)(type & ~DEF_SAVEGLOBAL);

	switch (type)
	{
	case ev_string:
		snprintf(line, sizeof(line), "%s", PR_GetString(val->string));
		break;

	case ev_entity:
		snprintf(line, sizeof(line), "%li", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;

	case ev_function:
		f = pr_functions + val->function;
		snprintf(line, sizeof(line), "%s", PR_GetString(f->s_name));
		break;

	case ev_field:
		def = ED_FieldAtOfs(val->_int);
		snprintf(line, sizeof(line), "%s", PR_GetString(def->s_name));
		break;

	case ev_void:
		snprintf(line, sizeof(line), "void");
		break;

	case ev_float:
		snprintf(line, sizeof(line), "%f", val->_float);
		break;

	case ev_vector:
		snprintf(line, sizeof(line), "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;

	default:
		snprintf(line, sizeof(line), "bad type %i", type);
		break;
	}

	return line;
}

/* For debugging */
void ED_Print(edict_t *ed)
{
	if (ed->free)
	{
		Con_SafePrintf("FREE\n");
		return;
	}

	Con_SafePrintf("\nEDICT %i:\n", NUM_FOR_EDICT(ed));
	for (int i = 1; i < progs->numfielddefs; i++)
	{
		ddef_t *d = &pr_fielddefs[i];
		const char *name = PR_GetString(d->s_name);
		if (name[strlen(name) - 2] == '_')
			continue;	// skip _x, _y, _z vars

		int *v = (int *) ((char *) &ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		int type = d->type & ~DEF_SAVEGLOBAL;
		int j;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Con_SafePrintf("%s", name);  // Baker: make it speedy!
		int l = strlen(name);
		while (l++ < 15)
			Con_SafePrintf(" ");

		Con_SafePrintf("%s\n", PR_ValueString((etype_t)d->type, (eval_t *) v));
	}
}

/* For savegames */
void ED_Write(FILE *f, edict_t *ed)
{
	fprintf(f, "{\n");

	if (ed->free)
	{
		fprintf(f, "}\n");
		return;
	}

	for (int i = 1; i < progs->numfielddefs; i++)
	{
		ddef_t *d = &pr_fielddefs[i];
		const char *name = PR_GetString(d->s_name);
		if (name[strlen(name) - 2] == '_')
			continue;	// skip _x, _y, _z vars

		int *v = (int *) ((char *) &ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		int j;
		int type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		fprintf(f, "\"%s\" ", name);
		fprintf(f, "\"%s\"\n", PR_UglyValueString((etype_t)d->type, (eval_t *) v));
	}

	fprintf(f, "}\n");
}

void ED_PrintNum(int ent)
{
	ED_Print(EDICT_NUM(ent));
}



/*
 * Can parse either fields or globals
 * returns false if error
 */
static bool ED_ParseEpair(void *base, ddef_t *key, char *s)
{
	int i;
	char string[128];
	ddef_t *def;
	char *v, *w;
	void *d;
	dfunction_t *func;

	d = (void *) ((int *) base + key->ofs);

	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		*(string_t *) d = ED_NewString(s);
		break;

	case ev_float:
		*(float *) d = atof(s);
		break;

	case ev_vector:
		strcpy(string, s);
		v = string;
		w = string;
		for (i = 0; i < 3; i++)
		{
			while (*v && *v != ' ')
				v++;
			*v = 0;
			((float *) d)[i] = atof(w);
			w = v = v + 1;
		}
		break;

	case ev_entity:
		*(int *) d = EDICT_TO_PROG(EDICT_NUM(atoi(s)));
		break;

	case ev_field:
		if (!(def = ED_FindField(s)))
		{
			Con_SafePrintf("Can't find field %s\n", s);
			return false;
		}
		*(int *) d = G_INT(def->ofs);
		break;

	case ev_function:
		if (!(func = ED_FindFunction(s)))
		{
			Con_SafePrintf("Can't find function %s\n", s);
			return false;
		}
		*(func_t *) d = func - pr_functions;
		break;

	default:
		break;
	}

	return true;
}

/*
 ==============================================================================

 ARCHIVING GLOBALS

 FIXME: need to tag constants, doesn't really work
 ==============================================================================
 */

void ED_WriteGlobals(FILE *f)
{
	ddef_t *def;
	int i, type;
	const char *name;

	fprintf(f, "{\n");
	for (i = 0; i < progs->numglobaldefs; i++)
	{
		def = &pr_globaldefs[i];
		type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString(def->s_name);
		fprintf(f, "\"%s\" ", name);
		fprintf(f, "\"%s\"\n", PR_UglyValueString((etype_t)type, (eval_t *) &pr_globals[def->ofs]));
	}
	fprintf(f, "}\n");
}

void ED_ParseGlobals(const char *data)
{
	char keyname[64];
	ddef_t *key;

	while (true)
	{
		// parse key
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("EOF without closing brace");

		strcpy(keyname, com_token);

		// parse value
		if (!(data = COM_Parse(data)))
			Sys_Error("EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error("closing brace without data");

		if (!(key = ED_FindGlobal(keyname)))
		{
			Con_SafePrintf("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair((void *) pr_globals, key, com_token))
			Host_Error("ED_ParseGlobals: parse error");
	}
}

//============================================================================




/*
 * Parses an edict out of the given string, returning the new position
 * ent should be a properly initialized empty edict.
 * Used for initial level load and for savegames.
 */
const char *ED_ParseEdict(const char *data, edict_t *ent)
{
	bool anglehack;
	bool init = false;

	// clear it
	if (ent != sv.edicts)	// hack
		memset(&ent->v, 0, progs->entityfields * 4);

	// go through all the dictionary pairs
	while (true)
	{
		// parse key
		data = COM_Parse(data);
		if (com_token[0] == '}')
			break;
		if (!data)
			Sys_Error("EOF without closing brace");

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(com_token, "angle"))
		{
			strcpy(com_token, "angles");
			anglehack = true;
		}
		else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light"))
			strcpy(com_token, "light_lev");	// hack for single light def

		char keyname[256];
		strcpy(keyname, com_token);

		// another hack to fix keynames with trailing spaces
		int n = strlen(keyname);
		while (n && keyname[n - 1] == ' ')
		{
			keyname[n - 1] = 0;
			n--;
		}

		// parse value
		if (!(data = COM_Parse(data)))
			Sys_Error("EOF without closing brace");

		if (com_token[0] == '}')
			Sys_Error("closing brace without data");

		init = true;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		ddef_t *key = ED_FindField(keyname);
		if (!key)
		{
			if (strcmp(keyname, "sky") && strcmp(keyname, "fog")) // Now supported in worldspawn
				Con_SafePrintf("'%s' is not a field\n", keyname);
			continue;
		}

		if (anglehack)
		{
			char temp[32];

			strncpy(temp, com_token, sizeof(temp));
			snprintf(com_token, sizeof(com_token), "0 %s 0", temp);
		}

		if (!ED_ParseEpair((void *) &ent->v, key, com_token))
			Host_Error("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = true;

	return data;
}

/*
 * The entities are directly placed in the array, rather than allocated with
 * ED_Alloc, because otherwise an error loading the map would have entity
 * number references out of order.
 *
 * Creates a server's entity / program execution context by
 * parsing textual entity definitions out of an ent file.
 *
 * Used for both fresh maps and savegame loads.  A fresh map would also need
 * to call ED_CallSpawnFunctions () to let the objects initialize themselves.
 */
void ED_LoadFromFile(const char *data)
{
	int inhibit = 0;
	edict_t *ent = NULL;
	pr_global_struct->time = sv.time;

	// parse ents
	while (true)
	{
		// parse the opening brace
		data = COM_Parse(data);
		if (!data)
			break;
		if (com_token[0] != '{')
			Sys_Error("found %s when expecting {", com_token);

		ent = (!ent) ? EDICT_NUM(0) : ED_Alloc();
		data = ED_ParseEdict(data, ent);

		// remove things from different skill levels or deathmatch
		if (( deathmatch.value && /* no skill check */  ((int) ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH)) ||
		    (!deathmatch.value && current_skill == 0 && ((int) ent->v.spawnflags & SPAWNFLAG_NOT_EASY)) ||
		    (!deathmatch.value && current_skill == 1 && ((int) ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM)) ||
		    (!deathmatch.value && current_skill >= 2 && ((int) ent->v.spawnflags & SPAWNFLAG_NOT_HARD)))
		{
			ED_Free(ent);
			inhibit++;
			continue;
		}

		// immediately call spawn function
		if (!ent->v.classname)
		{
			Con_SafePrintf("No classname for:\n");
			ED_Print(ent);
			ED_Free(ent);
			continue;
		}

		// look for the spawn function
		dfunction_t *func = ED_FindFunction(PR_GetString(ent->v.classname));
		if (!func)
		{
			Con_SafePrintf("No spawn function for:\n"); // Baker: makes it speedy!
			ED_Print(ent);
			ED_Free(ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram(func - pr_functions);
	}

	Con_DPrintf("%i entities inhibited\n", inhibit);
}

void PR_LoadProgs(const char *progsname)
{
	// flush the non-C variable lookup cache
	for (int i = 0; i < GEFV_CACHESIZE; i++)
		gefvCache[i].field[0] = 0;

	if (!progsname || !*progsname)
		Host_Error("PR_LoadProgs: passed empty progsname");

	CRC_Init(&pr_crc);

	if (!(progs = (dprograms_t *) COM_LoadHunkFile(progsname)))
		Sys_Error("couldn't load %s", progsname);

	Con_DPrintf("Programs occupy %iK.\n", com_filesize / 1024);

	for (int i = 0; i < com_filesize; i++)
		CRC_ProcessByte(&pr_crc, ((byte *) progs)[i]);

	// byte swap the header
	for (unsigned int i = 0; i < sizeof(*progs) / 4; i++)
		((int *) progs)[i] = LittleLong(((int *) progs)[i]);

	if (progs->version != PROG_VERSION)
		Sys_Error("%s has wrong version number (%i should be %i)", progsname, progs->version, PROG_VERSION);

	if (progs->crc != PROGHEADER_CRC)
		Sys_Error("%s system vars have been modified, progdefs.h is out of date", progsname);

	pr_functions = (dfunction_t *) ((byte *) progs + progs->ofs_functions);
	pr_strings = (char *) progs + progs->ofs_strings;
	if (progs->ofs_strings + progs->numstrings >= com_filesize)
		Host_Error("progs.dat strings go past end of file\n");

	// initialize the strings
	pr_numknownstrings = 0;
	pr_maxknownstrings = 0;
	pr_stringssize = progs->numstrings;
	if (pr_knownstrings)
		free((void *) pr_knownstrings);
	pr_knownstrings = NULL;
	PR_SetEngineString("");

	pr_globaldefs = (ddef_t *) ((byte *) progs + progs->ofs_globaldefs);
	pr_fielddefs = (ddef_t *) ((byte *) progs + progs->ofs_fielddefs);
	pr_statements = (dstatement_t *) ((byte *) progs + progs->ofs_statements);

	pr_global_struct = (globalvars_t *) ((byte *) progs + progs->ofs_globals);
	pr_globals = (float *) pr_global_struct;

	pr_edict_size = progs->entityfields * 4 + sizeof(edict_t) - sizeof(entvars_t);

	// byte swap the lumps
	for (int i = 0; i < progs->numstatements; i++)
	{
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (int i = 0; i < progs->numfunctions; i++)
	{
		pr_functions[i].first_statement = LittleLong(pr_functions[i].first_statement);
		pr_functions[i].parm_start = LittleLong(pr_functions[i].parm_start);
		pr_functions[i].s_name = LittleLong(pr_functions[i].s_name);
		pr_functions[i].s_file = LittleLong(pr_functions[i].s_file);
		pr_functions[i].numparms = LittleLong(pr_functions[i].numparms);
		pr_functions[i].locals = LittleLong(pr_functions[i].locals);
	}

	for (int i = 0; i < progs->numglobaldefs; i++)
	{
		pr_globaldefs[i].type = LittleShort(pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort(pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong(pr_globaldefs[i].s_name);
	}

	for (int i = 0; i < progs->numfielddefs; i++)
	{
		pr_fielddefs[i].type = LittleShort(pr_fielddefs[i].type);
		if (pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			Sys_Error("pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr_fielddefs[i].ofs = LittleShort(pr_fielddefs[i].ofs);
		pr_fielddefs[i].s_name = LittleLong(pr_fielddefs[i].s_name);
	}

	for (int i = 0; i < progs->numglobals; i++)
		((int *) pr_globals)[i] = LittleLong(((int *) pr_globals)[i]);

	FindEdictFieldOffsets();
}

/* For debugging, prints all the entities in the current server */
void ED_PrintEdicts_f(void)
{
	Con_SafePrintf("%i entities\n", sv.num_edicts);
	for (int i = 0; i < sv.num_edicts; i++)
		ED_PrintNum(i);
}

/* For debugging, prints a single edict */
static void ED_PrintEdict_f(void)
{
	int i = atoi(Cmd_Argv(1));
	if (i >= sv.num_edicts)
	{
		Con_SafePrintf("Bad edict number\n");
		return;
	}
	ED_PrintNum(i);
}

/* For debugging */
static void ED_Count_f(void)
{
	int active = 0;
	int models = 0;
	int solid = 0;
	int step = 0;

	for (int i = 0; i < sv.num_edicts; i++)
	{
		edict_t *ent = EDICT_NUM(i);
		if (ent->free)
			continue;

		active++;
		if (ent->v.model)
			models++;
		if (ent->v.solid)
			solid++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Con_SafePrintf("num_edicts:%3i\n", sv.num_edicts);
	Con_SafePrintf("active    :%3i\n", active);
	Con_SafePrintf("view      :%3i\n", models);
	Con_SafePrintf("touch     :%3i\n", solid);
	Con_SafePrintf("step      :%3i\n", step);
}

void PR_Init(void)
{
	Cmd_AddCommand("edict", ED_PrintEdict_f);
	Cmd_AddCommand("edicts", ED_PrintEdicts_f);
	Cmd_AddCommand("edictcount", ED_Count_f);
	Cmd_AddCommand("profile", PR_Profile_f);

	Cvar_RegisterVariable(&nomonsters);
	Cvar_RegisterVariable(&gamecfg);
	Cvar_RegisterVariable(&scratch1);
	Cvar_RegisterVariable(&scratch2);
	Cvar_RegisterVariable(&scratch3);
	Cvar_RegisterVariable(&scratch4);
	Cvar_RegisterVariable(&savedgamecfg);
	Cvar_RegisterVariable(&saved1);
	Cvar_RegisterVariable(&saved2);
	Cvar_RegisterVariable(&saved3);
	Cvar_RegisterVariable(&saved4);
}
