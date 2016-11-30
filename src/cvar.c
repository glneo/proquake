/*
 * Dynamic variable tracking
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

cvar_t *cvar_vars;

cvar_t *Cvar_FindVar(const char *var_name)
{
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next)
		if (!strcmp(var_name, var->name))
			return var;

	return NULL;
}

cvar_t *Cvar_FindVarAfter(const char *prev_name, unsigned int with_flags)
{
	cvar_t	*var;

	if (*prev_name)
	{
		var = Cvar_FindVar (prev_name);
		if (!var)
			return NULL;
		var = var->next;
	}
	else
		var = cvar_vars;

	// search for the next cvar matching the needed flags
	while (var)
	{
		if ((var->flags & with_flags) || !with_flags)
			break;
		var = var->next;
	}

	return var;
}

float Cvar_VariableValue(const char *var_name)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
		return 0;

	return var->value;
}

char *Cvar_VariableString(const char *var_name)
{
	static char *cvar_null_string = "";

	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
		return cvar_null_string;

	return var->string;
}

char *Cvar_CompleteVariable(const char *partial)
{
	cvar_t *cvar;

	int len = strlen(partial);

	if (!len)
		return NULL;

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncmp(partial, cvar->name, len))
			return cvar->name;

	return NULL;
}

void Cvar_SetQuick (cvar_t *var, const char *value)
{
	// Read-only or Locked
	if (var->flags & (CVAR_ROM | CVAR_LOCKED))
		return;
	// Not registered yet
	if (!(var->flags & CVAR_REGISTERED))
		return;
	// Not changed
	if (!strcmp(var->string, value))
		return;

	free(var->string); // free the old value string
	var->string = Q_strdup(value);
	var->value = atof(var->string);

	// during initialization, update default too
	if (!host_initialized)
	{
		free(var->default_string);
		var->default_string = Q_strdup(value);
	}

	if (sv.active && (var->flags & CVAR_NOTIFY))
		SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", var->name, var->string);

	if (var->callback)
		var->callback(var);

	// JPG 3.00 - rcon (64 doesn't mean anything special, but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - strlen(var->name) - strlen(var->string) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString(&rcon_message, va("\"%s\" set to \"%s\"\n", var->name, var->string));
	}
}

void Cvar_SetValueQuick (cvar_t *var, float value)
{
	Cvar_SetQuick(var, COM_NiceFloatString(value));
}

void Cvar_Set(const char *var_name, const char *value)
{
	cvar_t *var = Cvar_FindVar(var_name);
	if (!var)
	{	// there is an error in C code if this happens
		Con_Printf("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	Cvar_SetQuick(var, value);
}

void Cvar_SetValue(const char *var_name, float value)
{
	Cvar_Set(var_name, COM_NiceFloatString(value));
}

/* Adds a freestanding variable to the variable list. */
void Cvar_RegisterVariable(cvar_t *variable)
{
	// first check to see if it has already been defined
	if (Cvar_FindVar(variable->name))
	{
		Con_Printf("Can't register variable %s, already defined\n", variable->name);
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists(variable->name))
	{
		Con_Printf("Can't register variable %s is a command\n", variable->name);
		return;
	}

	// link the variable in
	if (cvar_vars == NULL || strcmp(variable->name, cvar_vars->name) < 0) // insert at front
	{
		variable->next = cvar_vars;
		cvar_vars = variable;
	}
	else //insert later
	{
		cvar_t *prev = cvar_vars;
		cvar_t *cursor = cvar_vars->next;
		while (cursor && (strcmp(variable->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		variable->next = prev->next;
		prev->next = variable;
	}

	// save initial value for "reset" command
	variable->default_string = Q_strdup(variable->string);

	// copy the value off, because future sets will Z_Free it
	variable->string = Q_strdup(variable->string);

	variable->value = atof(variable->string);

	if (!(variable->flags & CVAR_CALLBACK))
		variable->callback = NULL;

	variable->flags |= CVAR_REGISTERED;
}

/* Set a callback function to the var */
void Cvar_SetCallback(cvar_t *var, cvarcallback_t func)
{
	var->callback = func;
	if (func)
		var->flags |= CVAR_CALLBACK;
	else
		var->flags &= ~CVAR_CALLBACK;
}

/* Handles variable inspection and changing from the console */
bool Cvar_Command(void)
{
	cvar_t *v;

	// check variables
	v = Cvar_FindVar(Cmd_Argv(0));
	if (!v)
		return false;

	// perform a variable print or set
	switch (Cmd_Argc())
	{
	case 1:
		Con_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		break;
	case 2:
		Cvar_Set(v->name, Cmd_Argv(1));
		break;
	default:
		Con_Printf("<cvar> [value] : print or set cvar\n");
		break;
	}

	return true;
}

/*
 * Writes lines containing "set variable value" for all variables
 * with the archive flag set to true.
 */
void Cvar_WriteVariables(FILE *f)
{
	cvar_t *var;

	fprintf(f, "\n// Variables\n\n");
	for (var = cvar_vars; var; var = var->next)
		if (var->flags & CVAR_ARCHIVE)
			fprintf(f, "%s \"%s\"\n", var->name, var->string);
}

static void Cvar_Reset(const char *name)
{
	cvar_t *var = Cvar_FindVar(name);
	if (!var)
		Con_Printf("variable \"%s\" not found\n", name);
	else
		Cvar_Set(var->name, var->default_string);
}

static void Cvar_List_f(void)
{
	cvar_t *cvar;
	char *partial = NULL;
	int len = 0, count = 0;

	if (Cmd_Argc() > 1)
	{
		partial = Cmd_Argv(1);
		len = strlen(partial);
	}

	Con_Printf("\n");

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		if (partial && strncmp(partial, cvar->name, len))
			continue;

		Con_Printf("%s is [%s]\n", cvar->name, cvar->string);
		count++;
	}

	Con_Printf("\n%i cvar(s)", count);

	if (partial)
		Con_Printf(" beginning with \"%s\"", partial);

	Con_Printf("\n\n");
}

static void Cvar_Inc_f(void)
{
	switch (Cmd_Argc())
	{
	case 2:
		Cvar_SetValue(Cmd_Argv(1), Cvar_VariableValue(Cmd_Argv(1)) + 1);
		break;
	case 3:
		Cvar_SetValue(Cmd_Argv(1), Cvar_VariableValue(Cmd_Argv(1)) + atof(Cmd_Argv(2)));
		break;
	default:
		Con_Printf("inc <cvar> [amount] : increment cvar\n");
		break;
	}
}

static void Cvar_Toggle_f(void)
{
	switch (Cmd_Argc())
	{
	case 2:
		if (Cvar_VariableValue(Cmd_Argv(1)))
			Cvar_Set(Cmd_Argv(1), "0");
		else
			Cvar_Set(Cmd_Argv(1), "1");
		break;
	default:
		Con_Printf("toggle <cvar> : toggle cvar\n");
		break;
	}
}

static void Cvar_Cycle_f(void)
{
	int i;

	if (Cmd_Argc() < 3)
	{
		Con_Printf("cycle <cvar> <value list>: cycle cvar through a list of values\n");
		return;
	}

	//loop through the args until you find one that matches the current cvar value.
	//yes, this will get stuck on a list that contains the same value twice.
	//it's not worth dealing with, and i'm not even sure it can be dealt with.

	for (i = 2; i < Cmd_Argc(); i++)
	{
		//zero is assumed to be a string, even though it could actually be zero.  The worst case
		//is that the first time you call this command, it won't match on zero when it should, but after that,
		//it will be comparing strings that all had the same source (the user) so it will work.
		if (atof(Cmd_Argv(i)) == 0)
		{
			if (!strcmp(Cmd_Argv(i), Cvar_VariableString(Cmd_Argv(1))))
				break;
		}
		else
		{
			if (atof(Cmd_Argv(i)) == Cvar_VariableValue(Cmd_Argv(1)))
				break;
		}
	}

	if (i == Cmd_Argc())
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(2)); // no match
	else if (i + 1 == Cmd_Argc())
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(2)); // matched last value in list
	else
		Cvar_Set(Cmd_Argv(1), Cmd_Argv(i + 1)); // matched earlier in list
}

void Cvar_Reset_f(void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("reset <cvar> : reset cvar to default\n");
		break;
	case 2:
		Cvar_Reset(Cmd_Argv(1));
		break;
	}
}

void Cvar_ResetAll_f(void)
{
	cvar_t *var;

	for (var = cvar_vars; var; var = var->next)
		Cvar_Reset(var->name);
}

void Cvar_Init(void)
{
	Cmd_AddCommand("cvarlist", Cvar_List_f);
	Cmd_AddCommand("inc", Cvar_Inc_f);
	Cmd_AddCommand("toggle", Cvar_Toggle_f);
	Cmd_AddCommand("cycle", Cvar_Cycle_f);
	Cmd_AddCommand("reset", Cvar_Reset_f);
	Cmd_AddCommand("resetall", Cvar_ResetAll_f);
}
