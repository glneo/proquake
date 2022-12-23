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

#include <sstream>
#include <string>
#include <unordered_set>

#include "quakedef.h"
#include "glquake.h"

static std::string gl_vendor;
static std::string gl_renderer;
static std::string gl_version;
static std::unordered_set<std::string> gl_extensions;

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
	int y = vid.y + r_refdef.vrect.y;
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

// FOR DEBUG
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

static void GL_GetInfo(void)
{
	gl_vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	gl_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
	gl_version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

#ifdef OPENGLES
	std::stringstream ss(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
	std::string gl_extension;

	while (std::getline(ss, gl_extension, ' '))
		gl_extensions.insert(gl_extension);
#else
	GLint n;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (GLint i = 0; i < n; i++)
		gl_extensions.insert(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
#endif
}

static void GL_Info_f(void)
{
	Con_SafePrintf("GL_VENDOR: %s\n", gl_vendor.c_str());
	Con_SafePrintf("GL_RENDERER: %s\n", gl_renderer.c_str());
	Con_SafePrintf("GL_VERSION: %s\n", gl_version.c_str());
	Con_SafePrintf("GL_EXTENSIONS:%s\n", gl_extensions.empty() ? " (none)" : "");
	for (auto gl_extension : gl_extensions)
		Con_SafePrintf("    %s\n", gl_extension.c_str());
}

static void GL_CheckExtensions(void)
{
	// anisotropic filtering
	if (!gl_extensions.count("GL_EXT_texture_filter_anisotropic"))
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
		gl_texture_NPOT = false;
	}
	else if (gl_extensions.count("GL_ARB_texture_non_power_of_two"))
	{
		Con_Printf("FOUND: ARB_texture_non_power_of_two\n");
		gl_texture_NPOT = true;
	}
	else if (gl_extensions.count("GL_OES_texture_npot"))
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
	R_Clear();
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
//	glClearColor(0.15, 0.15, 0.15, 0);

//	glDisable(GL_DITHER);

//	glEnable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

//	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

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

	GL_GetInfo();
	GL_Info_f();
	GL_CheckExtensions();

	GL_SetupState();
}
