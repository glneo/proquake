/*
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2005 John Fitzgibbons and others
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

#ifndef __CVAR_H
#define __CVAR_H

/*
 * cvar_t variables are used to hold scalar or string variables that can be
 * changed or displayed at the console or prog code as well as accessed
 * directly in C code.
 *
 * it is sufficient to initialize a cvar_t with just the first two fields, or
 * you can add a CVAR_ARCHIVE flag for variables that you want saved to the
 * configuration file when the game is quit:
 *
 *   cvar_t r_draworder = { "r_draworder", "1" };
 *   cvar_t scr_screensize = { "screensize", "1", CVAR_ARCHIVE };
 *
 * Cvars must be registered before use, or they will have a 0 value instead of
 * the float interpretation of the string. Generally, all cvar_t declarations
 * should be registered in the appropriate init function before any console
 * commands are executed:
 *
 *   Cvar_RegisterVariable (&r_draworder);
 *
 * C code usually just references a cvar in place:
 *
 *   if ( r_draworder.value )
 *
 * It could optionally ask for the value to be looked up for a string name:
 *
 *   if (Cvar_VariableValue ("r_draworder"))
 *
 * Interpreted prog code can access cvars with the cvar(name) or
 * cvar_set (name, value) internal functions:
 *
 *   teamplay = cvar("teamplay");
 *   cvar_set ("registered", "1");
 *
 * The user can access cvars from the console in two ways:
 *
 *   r_draworder                prints the current value
 *   r_draworder 0              sets the current value to 0
 *
 * Cvars are restricted from having the same names as commands to keep this
 * interface from being ambiguous.
 */

#define	CVAR_NONE		0
#define	CVAR_ARCHIVE		BIT(0) // if set, causes it to be saved to config
#define	CVAR_NOTIFY		BIT(1) // changes will be broadcasted to all players (q1)
#define	CVAR_SERVERINFO		BIT(2) // added to serverinfo will be sent to clients (q1/net_dgrm.c and qwsv)
#define	CVAR_USERINFO		BIT(3) // added to userinfo, will be sent to server (qwcl)
#define	CVAR_CHANGED		BIT(4)
#define	CVAR_ROM		BIT(6) // read-only
#define	CVAR_LOCKED		BIT(8) // locked temporarily
#define	CVAR_REGISTERED		BIT(10) // the var is added to the list of variables
#define	CVAR_CALLBACK		BIT(16) // var has a callback

typedef void (*cvarcallback_t) (struct cvar_s *);

typedef struct cvar_s
{
	const char *name;
	const char *string;
	unsigned int flags;
	float value;
	char *default_string;
	cvarcallback_t callback;
	struct cvar_s *next;
} cvar_t;

cvar_t *Cvar_FindVar(const char *var_name);
cvar_t *Cvar_FindVarAfter(const char *prev_name, unsigned int with_flags);

// returns 0 if not defined or non numeric
float Cvar_VariableValue(const char *var_name);

// returns an empty string if not defined
const char *Cvar_VariableString(const char *var_name);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
const char *Cvar_CompleteVariable(const char *partial);

// equivelant to "<name> <variable>" typed at the console
void Cvar_Set(const char *var_name, const char *value);
void Cvar_SetQuick (cvar_t *var, const char *value);

// expands value to a string and calls Cvar_Set
void Cvar_SetValue(const char *var_name, float value);
void Cvar_SetValueQuick (cvar_t *var, float value);

// registers a cvar that already has the name, string, and optionally the
// flags set.
void Cvar_RegisterVariable(cvar_t *variable);
void Cvar_SetCallback(cvar_t *var, cvarcallback_t func);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
bool Cvar_Command(void);

// Writes lines containing "set variable value" for all variables
// with the archive flag set to true.
void Cvar_WriteVariables(FILE *f);

void Cvar_Init(void);

#endif /* __CVAR_H */
