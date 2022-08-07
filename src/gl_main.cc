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
#include "glquake.h"

static const char *gl_vendor;
static const char *gl_renderer;
static const char *gl_version;
static const char *gl_extensions;
static char *gl_extensions_nice;

bool gl_swap_control = false;
bool gl_anisotropy_able = false;
bool gl_texture_NPOT = false;
float gl_max_anisotropy = 1.0f;

Q_Matrix modelViewMatrix;
Q_Matrix projectionMatrix;

cvar_t gl_farclip = { "gl_farclip", "16384", CVAR_ARCHIVE };
cvar_t gl_nearwater_fix = { "gl_nearwater_fix", "1", CVAR_ARCHIVE };
cvar_t gl_fadescreen_alpha = { "gl_fadescreen_alpha", "0.7", CVAR_ARCHIVE };
cvar_t gl_clear = { "gl_clear", "0", CVAR_ARCHIVE };
cvar_t gl_smoothmodels = { "gl_smoothmodels", "1" };
cvar_t gl_affinemodels = { "gl_affinemodels", "0" };
cvar_t gl_polyblend = { "gl_polyblend", "1", CVAR_ARCHIVE };
cvar_t gl_playermip = { "gl_playermip", "0", CVAR_ARCHIVE };
cvar_t gl_overbright = { "gl_overbright", "1", CVAR_ARCHIVE };

void GL_RotateForEntity(entity_t *ent, Q_Matrix &matrix)
{
	matrix.translate(ent->origin[0], ent->origin[1], ent->origin[2]);

	matrix.rotateZ(ent->angles[1]);
	matrix.rotateY(ent->angles[0]);
	matrix.rotateX(ent->angles[2]);
}

void R_Clear(void)
{
	GLbitfield clearbits = GL_DEPTH_BUFFER_BIT;

	// If gl_clear is 1 we clear the color buffer
	if (gl_clear.value)
		clearbits |= GL_COLOR_BUFFER_BIT;

	glClear(clearbits);
}

void GL_Setup(void)
{
	// set up viewpoint
	int x = vid.x + r_refdef.vrect.x;
	int y = vid.y + r_refdef.vrect.y + sb_lines;
	int w = r_refdef.vrect.width;
	int h = r_refdef.vrect.height;
	glViewport(x, y, w, h);

	// set up projection matrix
	float zmax = max(gl_farclip.value, 4096.0f);
	float zmin = 4.0f;
	float ymax = zmin * tanf(r_refdef.fov_y * M_PI / 360.0);
	float ymin = -ymax;
	float xmax = zmin * tanf(r_refdef.fov_x * M_PI / 360.0);
	float xmin = -xmax;
	projectionMatrix.identity();
	projectionMatrix.frustum(xmin, xmax, ymin, ymax, zmin, zmax);

// REMOVE
//	projectionMatrix.translate(0.0f, -200.0f, -1000.0f);
//	projectionMatrix.rotateX(45.0f);


	// set up modelview matrix
	modelViewMatrix.identity();
	modelViewMatrix.rotateX(-90.0f); // put Z going up
	modelViewMatrix.rotateZ(90.0f); // put Z going up
	modelViewMatrix.rotateX(-r_refdef.viewangles[2]);
	modelViewMatrix.rotateY(-r_refdef.viewangles[0]);
	modelViewMatrix.rotateZ(-r_refdef.viewangles[1]);
	modelViewMatrix.translate(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);
}

/* Translates a skin texture by the per-player color lookup */
void R_TranslatePlayerSkin(int playernum)
{
}

// TODO: Use list
static char *GL_MakeNiceExtensionsList(const char *in)
{
	char *copy, *token, *out;
	int i, count;

	if (!in)
		return (char *)Q_strdup("(none)");

	//each space will be replaced by 4 chars, so count the spaces before we malloc
	for (i = 0, count = 1; i < (int) strlen(in); i++)
	{
		if (in[i] == ' ')
			count++;
	}

	out = (char *)Q_malloc(strlen(in) + count * 3 + 1); //usually about 1-2k
	out[0] = 0;

	copy = (char *)Q_strdup(in);
	for (token = strtok(copy, " "); token; token = strtok(NULL, " "))
	{
		strcat(out, "\n   ");
		strcat(out, token);
	}

	free(copy);
	return out;
}

static void GL_Info_f(void)
{
	Con_SafePrintf("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf("GL_VERSION: %s\n", gl_version);
	Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions_nice);
}

static bool GL_ParseExtensionList(const char *list, const char *name)
{
	const char *start;
	const char *where, *terminator;

	if (!list || !name || !*name)
		return false;
	if (strchr(name, ' ') != NULL)
		return false;	// extension names must not have spaces

	start = list;
	while (1)
	{
		where = strstr(start, name);
		if (!where)
			break;
		terminator = where + strlen(name);
		if (where == start || where[-1] == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		start = terminator;
	}
	return false;
}

static void GL_CheckExtensions(void)
{
	// anisotropic filtering
	if (!GL_ParseExtensionList(gl_extensions, "GL_EXT_texture_filter_anisotropic"))
	{
		gl_max_anisotropy = 1;
		Con_Warning("texture_filter_anisotropic not supported\n");
	}
	else
	{
		float test1, test2;
		GLuint tex;

		// test to make sure we really have control over it
		// 1.0 and 2.0 should always be legal values
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
		glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test1);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
		glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test2);
		glDeleteTextures(1, &tex);

		if (test1 != 1 || test2 != 2)
		{
			Con_Warning("anisotropic filtering locked by driver. Current driver setting is %f\n", test1);
		}
		else
		{
			Con_Printf("FOUND: EXT_texture_filter_anisotropic\n");
			gl_anisotropy_able = true;
		}

		//get max value either way, so the menu and stuff know it
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_anisotropy);
		if (gl_max_anisotropy < 2)
		{
			gl_anisotropy_able = false;
			gl_max_anisotropy = 1;
			Con_Warning("anisotropic filtering broken: disabled\n");
		}
	}

	// texture_non_power_of_two
	if (COM_CheckParm("-notexturenpot"))
	{
		Con_Warning("texture_non_power_of_two disabled at command line\n");
	}
	else if (GL_ParseExtensionList(gl_extensions, "GL_ARB_texture_non_power_of_two"))
	{
		Con_Printf("FOUND: ARB_texture_non_power_of_two\n");
		gl_texture_NPOT = true;
	}
	else if (GL_ParseExtensionList(gl_extensions, "GL_OES_texture_npot"))
	{
		Con_Printf("FOUND: GL_OES_texture_npot\n");
		gl_texture_NPOT = true;
	}
	else
	{
		Con_Warning("texture_non_power_of_two not supported\n");
		gl_texture_NPOT = false;
	}
}

void GL_BeginRendering(void)
{
}

void GL_EndRendering(void)
{
	VID_Swap();
}

/* the stuff from GL_Init that needs to be done every time a new GL render context is created */
static void GL_SetupState(void)
{
#ifndef OPENGLES
	static GLuint abuffer;

	glGenVertexArrays(1, &abuffer);
	glBindVertexArray(abuffer);
#endif

	// set clear color to gray
	glClearColor(0.15, 0.15, 0.15, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

//	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

#ifndef OPENGLES
	glEnable(GL_PROGRAM_POINT_SIZE);
#endif

	GL_CreateAliasShaders();
	GL_CreateBrushShaders();
	GL_ParticleInit();
}

void GL_Init(void)
{
	Cmd_AddCommand("gl_info", GL_Info_f);

	Cvar_RegisterVariable(&gl_farclip);
	Cvar_RegisterVariable(&gl_smoothmodels);
	Cvar_RegisterVariable(&gl_affinemodels);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&gl_playermip);
	Cvar_RegisterVariable(&gl_overbright);
	Cvar_RegisterVariable(&gl_nearwater_fix);
	Cvar_RegisterVariable(&gl_fadescreen_alpha);
	Cvar_RegisterVariable(&gl_clear);

	gl_vendor = (const char *) glGetString(GL_VENDOR);
	gl_renderer = (const char *) glGetString(GL_RENDERER);
	gl_version = (const char *) glGetString(GL_VERSION);
	gl_extensions = (const char *) glGetString(GL_EXTENSIONS);

	Con_SafePrintf("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf("GL_VERSION: %s\n", gl_version);

	if (gl_extensions_nice != NULL)
		free(gl_extensions_nice);
	gl_extensions_nice = GL_MakeNiceExtensionsList(gl_extensions);

	Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions_nice);

	GL_CheckExtensions();

	GL_SetupState();
}
