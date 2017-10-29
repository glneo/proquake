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

#include "pr_comp.h"			// defs shared with qcc
#include "progdefs.h"			// generated by program cdefs

typedef union eval_s {
	string_t string;
	float _float;
	float vector[3];
	func_t function;
	int _int;
	int edict;
} eval_t;

#define	MAX_ENT_LEAFS 16
typedef struct edict_s {
	bool free;
	link_t area;				// linked to a division node or leaf

	int num_leafs;
	short leafnums[MAX_ENT_LEAFS];

	entity_state_t baseline;

	float freetime;			// sv.time when the object was freed
	entvars_t v;					// C exported fields from progs
// other fields from progs come immediately after
} edict_t;

#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)
#define	GETEDICTFIELDVALUE(ed, fieldoffset) (fieldoffset ? (eval_t *)((byte *)&ed->v + fieldoffset) : NULL)

extern int eval_gravity, eval_items2, eval_ammo_shells1, eval_ammo_nails1;
extern int eval_ammo_lava_nails, eval_ammo_rockets1, eval_ammo_multi_rockets;
extern int eval_ammo_cells1, eval_ammo_plasma;

// nehahra specific
extern int eval_alpha, eval_fullbright;

//============================================================================

extern dprograms_t *progs;
extern dfunction_t *pr_functions;
extern char *pr_strings;
extern ddef_t *pr_globaldefs;
extern ddef_t *pr_fielddefs;
extern dstatement_t *pr_statements;
extern globalvars_t *pr_global_struct;
extern float *pr_globals;			// same as pr_global_struct

extern int pr_edict_size;	// in bytes

//============================================================================

void PR_Init(void);

void PR_ExecuteProgram(func_t fnum);
void PR_LoadProgs(char *progsname);

char *PR_GetString(int num);
int PR_SetEngineString(char *s);
int PR_AllocString(int bufferlength, char **ptr);

void PR_Profile_f(void);

edict_t *ED_Alloc(void);
void ED_Free(edict_t *ed);

int ED_NewString(char *string);
// returns a copy of the string allocated from the server's string heap

void ED_Print(edict_t *ed);
void ED_Write(FILE *f, edict_t *ed);
char *ED_ParseEdict(char *data, edict_t *ent);

void ED_WriteGlobals(FILE *f);
void ED_ParseGlobals(char *data);

void ED_LoadFromFile(char *data);

//define EDICT_NUM(n) ((edict_t *)(sv.edicts+ (n)*pr_edict_size))
//define NUM_FOR_EDICT(e) (((byte *)(e) - sv.edicts)/pr_edict_size)

edict_t *EDICT_NUM(int n);
int NUM_FOR_EDICT(edict_t *e);

#define	NEXT_EDICT(e) ((edict_t *)( (byte *)e + pr_edict_size))

#define	EDICT_TO_PROG(e) ((byte *)e - (byte *)sv.edicts)
#define PROG_TO_EDICT(e) ((edict_t *)((byte *)sv.edicts + e))

#define	G_FLOAT(o)		(pr_globals[o])
#define	G_INT(o)		(*(int *)&pr_globals[o])
#define	G_EDICT(o)		((edict_t *)((byte *)sv.edicts+ *(int *)&pr_globals[o]))
#define G_EDICTNUM(o)		NUM_FOR_EDICT(G_EDICT(o))
#define	G_VECTOR(o)		(&pr_globals[o])
#define	G_STRING(o)		(PR_GetString(*(string_t *)&pr_globals[o]))
#define	G_FUNCTION(o)		(*(func_t *)&pr_globals[o])

#define	E_FLOAT(e,o)		(((float*)&e->v)[o])
#define	E_INT(e,o)		(*(int *)&((float*)&e->v)[o])
#define	E_VECTOR(e,o)		(&((float*)&e->v)[o])
#define	E_STRING(e,o)		(PR_GetString(*(string_t *)&((float*)&e->v)[o]))

extern int type_size[8];

typedef void (*builtin_t)(void);
extern builtin_t *pr_builtins;
extern int pr_numbuiltins;

extern int pr_argc;

extern bool pr_trace;
extern dfunction_t *pr_xfunction;
extern int pr_xstatement;

extern unsigned short pr_crc;

void PR_RunError(char *error, ...);
void PF_changeyaw(void);

void ED_PrintEdicts_f(void);
void ED_PrintNum(int ent);

