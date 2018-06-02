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
float gl_max_anisotropy;

int r_framecount; // used for dlight push checking

int d_lightstylevalue[256]; // 8.8 fraction of base light value

cvar_t r_wateralpha = { "r_wateralpha", "1", true };
cvar_t r_dynamic = { "r_dynamic", "1" };
cvar_t r_novis = { "r_novis", "0" };
cvar_t r_interpolate_animation = { "r_interpolate_animation", "0", true };
cvar_t r_interpolate_transform = { "r_interpolate_transform", "0", true };
cvar_t r_interpolate_weapon = { "r_interpolate_weapon", "0", true };
cvar_t gl_farclip = { "gl_farclip", "16384", true };
cvar_t gl_nearwater_fix = { "gl_nearwater_fix", "1", true };
cvar_t gl_fadescreen_alpha = { "gl_fadescreen_alpha", "0.7", true };

cvar_t gl_clear = { "gl_clear", "0", true};
cvar_t gl_cull = { "gl_cull", "1" };
cvar_t gl_smoothmodels = { "gl_smoothmodels", "1" };
cvar_t gl_affinemodels = { "gl_affinemodels", "0" };
cvar_t gl_polyblend = { "gl_polyblend", "1", true };
cvar_t gl_playermip = { "gl_playermip", "0", true };

cvar_t r_truegunangle = { "r_truegunangle", "0", true };  // Baker 3.80x - Optional "true" gun positioning on viewmodel
cvar_t r_drawviewmodel = { "r_drawviewmodel", "1", true };  // Baker 3.80x - Save to config
cvar_t r_ringalpha = { "r_ringalpha", "0.4", true }; // Baker 3.80x - gl_ringalpha
cvar_t r_fullbright = { "r_fullbright", "0" };
cvar_t r_lightmap = { "r_lightmap", "0" };

cvar_t gl_fullbright = { "gl_fullbright", "1", true };
cvar_t gl_overbright = { "gl_overbright", "1", true };

cvar_t r_waterwarp = { "r_waterwarp", "0", true }; // Baker 3.60 - Save this to config now

void GL_RotateForEntity(entity_t *ent)
{
	glTranslatef(ent->origin[0], ent->origin[1], ent->origin[2]);

	glRotatef(ent->angles[1], 0, 0, 1);
	glRotatef(-ent->angles[0], 0, 1, 0);
	glRotatef(ent->angles[2], 1, 0, 0);
}

void GL_PolyBlend(void)
{
	if (!v_blend[3]) // No blends ... get outta here
		return;

	if (!gl_polyblend.value)
		return;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	glPushMatrix();
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); // put Z going up
	glRotatef(90, 0, 0, 1); // put Z going up

	glColor4f(v_blend[0], v_blend[1], v_blend[2], v_blend[3]);

	GLfloat verts[] = {
		10,  100,  100,
		10, -100,  100,
		10, -100, -100,
		10,  100, -100,
	};

	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glPopMatrix();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
}

void R_Clear(void)
{
	GLbitfield clearbits = GL_DEPTH_BUFFER_BIT;

	// If gl_clear is 1 we clear the color buffer
	if (gl_clear.value)
		clearbits |= GL_COLOR_BUFFER_BIT;

	glClear(clearbits);
}

static void Q_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar)
{
	GLfloat matrix[] = {
		(2.0f * zNear) / (right - left), 0.0f, 0.0f, 0.0f,
		0.0f, (2.0f * zNear) / (top - bottom), 0.0f, 0.0f,
		(right + left) / (right - left), (top + bottom) / (top - bottom), -(zFar + zNear) / (zFar - zNear), -1.0f,
		0.0f, 0.0f, -(2 * zFar * zNear) / (zFar - zNear), 0.0f,
	};

	glMultMatrixf(matrix);
}

static void Q_gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	Q_glFrustumf(xmin, xmax, ymin, ymax, zNear, zFar);
}

void GL_Setup(void)
{
	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	int x = vid.x + r_refdef.vrect.x;
	int y = vid.y + (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height));
	int w = r_refdef.vrect.width;
	int h = r_refdef.vrect.height;

	glViewport(x, y, w, h);

	float screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	float farclip = max(gl_farclip.value, 4096.0f);
	Q_gluPerspective(r_refdef.fov_y, screenaspect, 4, farclip);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); // put Z going up
	glRotatef(90, 0, 0, 1); // put Z going up

	glRotatef(-r_refdef.viewangles[2], 1, 0, 0);
	glRotatef(-r_refdef.viewangles[0], 0, 1, 0);
	glRotatef(-r_refdef.viewangles[1], 0, 0, 1);
	glTranslatef(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	// set drawing parms
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}

/* Translates a skin texture by the per-player color lookup */
void R_TranslatePlayerSkin(int playernum)
{
}

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
		Con_Warning("texture_non_power_of_two disabled at command line\n");
	else if (!GL_ParseExtensionList(gl_extensions, "GL_ARB_texture_non_power_of_two"))
		Con_Warning("texture_non_power_of_two not supported\n");
	else
	{
		Con_Printf("FOUND: ARB_texture_non_power_of_two\n");
		gl_texture_NPOT = true;
	}
}

void GL_BeginRendering(void)
{
//	glEnableClientState(GL_VERTEX_ARRAY);
//	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void GL_EndRendering(void)
{
//	glDisableClientState(GL_VERTEX_ARRAY);
//	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	VID_Swap();
}

/* the stuff from GL_Init that needs to be done every time a new GL render context is created */
static void GL_SetupState(void)
{
	glClearColor(0.15, 0.15, 0.15, 0);
	glCullFace(GL_FRONT);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.1);

//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel(GL_FLAT);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void GL_Init(void)
{
	Cmd_AddCommand("gl_info", GL_Info_f);

	Cvar_RegisterVariable(&gl_farclip);
	Cvar_RegisterVariable(&gl_cull);
	Cvar_RegisterVariable(&gl_smoothmodels);
	Cvar_RegisterVariable(&gl_affinemodels);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&gl_playermip);
	Cvar_RegisterVariable(&gl_fullbright);
	Cvar_RegisterVariable(&gl_overbright);
	Cvar_RegisterVariable(&gl_nearwater_fix);
	Cvar_RegisterVariable(&gl_fadescreen_alpha);

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

	GL_CheckExtensions();

	GL_SetupState();
}
