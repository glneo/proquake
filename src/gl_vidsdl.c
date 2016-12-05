/*
 * SDL GL vid component
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2007-2008 Kristian Duske
 * Copyright (C) 2010-2014 QuakeSpasm developers
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

#include <SDL2/SDL.h>

#define MAX_MODE_LIST	600
#define MAX_BPPS_LIST	5
#define WARP_WIDTH	320
#define WARP_HEIGHT	200
#define MAXWIDTH	10000
#define MAXHEIGHT	10000

static const char *gl_vendor;
static const char *gl_renderer;
static const char *gl_version;
static int gl_version_major;
static int gl_version_minor;
static const char *gl_extensions;
static char *gl_extensions_nice;

static vmode_t modelist[MAX_MODE_LIST];
static int nummodes;

unsigned d_8to24table[256];

int texture_extension_number = 1;

static bool vid_initialized = false;

static SDL_Window *draw_context;
static SDL_GLContext gl_context;

static bool vid_locked = false; //johnfitz
static bool vid_changed = false;

int texture_mode = GL_LINEAR;

byte color_white[4] = { 255, 255, 255, 255 };
byte color_black[4] = { 0, 0, 0, 255 };

static void GL_Init(void);
static void GL_SetupState(void); //johnfitz

viddef_t vid; // global video state
modestate_t modestate = MODE_NONE;
bool scr_skipupdate;

bool gl_mtexable = true;
bool gl_texture_env_combine = false; //johnfitz
bool gl_texture_env_add = false; //johnfitz
bool gl_swap_control = false; //johnfitz
bool gl_anisotropy_able = false; //johnfitz
float gl_max_anisotropy; //johnfitz
bool gl_texture_NPOT = false; //ericw
bool gl_vbo_able = false; //ericw
bool gl_glsl_able = false; //ericw
GLint gl_max_texture_units = 0; //ericw
bool gl_glsl_gamma_able = false; //ericw
bool gl_glsl_alias_able = false; //ericw
int gl_stencilbits;

float gldepthmin, gldepthmax;

static cvar_t vid_fullscreen = { "vid_fullscreen", "0", true };
static cvar_t vid_width = { "vid_width", "800", true };
static cvar_t vid_height = { "vid_height", "600", true };
static cvar_t vid_bpp = { "vid_bpp", "16", true };
static cvar_t vid_vsync = { "vid_vsync", "0", true };
static cvar_t vid_desktopfullscreen = { "vid_desktopfullscreen", "0", true };

cvar_t vid_gamma = { "gamma", "1", true }; //johnfitz -- moved here from view.c
cvar_t vid_contrast = { "contrast", "1", true }; //QuakeSpasm, MarkV

void VID_SyncCvars(void);
void VID_SetPaletteOld(unsigned char *palette);

static int VID_GetCurrentWidth(void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return w;
}

static int VID_GetCurrentHeight(void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return h;
}

static int VID_GetCurrentBPP(void)
{
	const Uint32 pixelFormat = SDL_GetWindowPixelFormat(draw_context);
	return SDL_BITSPERPIXEL(pixelFormat);
}

static bool VID_GetFullscreen(void)
{
	return (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN) != 0;
}

static bool VID_GetDesktopFullscreen(void)
{
	return (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

static bool VID_GetVSync(void)
{
	return SDL_GL_GetSwapInterval() == 1;
}

bool VID_HasMouseOrInputFocus(void)
{
	return (SDL_GetWindowFlags(draw_context) & (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS)) != 0;
}

bool VID_IsMinimized(void)
{
	return !(SDL_GetWindowFlags(draw_context) & SDL_WINDOW_SHOWN);
}

/*
 ================
 VID_SDL2_GetDisplayMode

 Returns a pointer to a statically allocated SDL_DisplayMode structure
 if there is one with the requested params on the default display.
 Otherwise returns NULL.

 This is passed to SDL_SetWindowDisplayMode to specify a pixel format
 with the requested bpp. If we didn't care about bpp we could just pass NULL.
 ================
 */
static SDL_DisplayMode *VID_SDL2_GetDisplayMode(int width, int height, int bpp)
{
	static SDL_DisplayMode mode;
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

	for (i = 0; i < sdlmodes; i++)
	{
		if (SDL_GetDisplayMode(0, i, &mode) == 0 && mode.w == width && mode.h == height && SDL_BITSPERPIXEL(mode.format) == bpp)
		{
			return &mode;
		}
	}
	return NULL;
}

static bool VID_ValidMode(int width, int height, int bpp, bool fullscreen)
{
	if (width < 320)
		return false;

	if (height < 200)
		return false;

	if (fullscreen && VID_SDL2_GetDisplayMode(width, height, bpp) == NULL)
		bpp = 0;

	switch (bpp)
	{
	case 16:
	case 24:
	case 32:
		break;
	default:
		return false;
	}

	return true;
}

/*
 ================
 VID_SetMode
 ================
 */
static bool VID_SetMode(int width, int height, int bpp, bool fullscreen)
{
	int temp;
	Uint32 flags;
	char caption[50];
	int depthbits, stencilbits;
	int fsaa_obtained;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

//	CDAudio_Pause ();
//	BGM_Pause ();

	/* z-buffer depth */
	if (bpp == 16)
	{
		depthbits = 16;
		stencilbits = 0;
	}
	else
	{
		depthbits = 24;
		stencilbits = 8;
	}
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthbits);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilbits);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	snprintf(caption, sizeof(caption), "QuickQuake %1.2f", (float)PROQUAKE_SERIES_VERSION);

	/* Create the window if needed, hidden */
	if (!draw_context)
	{
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

		draw_context = SDL_CreateWindow(caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		if (!draw_context)
		{ // scale back SDL_GL_DEPTH_SIZE
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
			draw_context = SDL_CreateWindow(caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		}
		if (!draw_context)
		{ // scale back SDL_GL_STENCIL_SIZE
			SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
			draw_context = SDL_CreateWindow(caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		}
		if (!draw_context)
			Sys_Error("Couldn't create window");
	}

	/* Ensure the window is not fullscreen */
	if (VID_GetFullscreen())
	{
		if (SDL_SetWindowFullscreen(draw_context, 0) != 0)
			Sys_Error("Couldn't set fullscreen state mode");
	}

	/* Set window size and display mode */
	SDL_SetWindowSize(draw_context, width, height);
	SDL_SetWindowPosition(draw_context, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SetWindowDisplayMode(draw_context, VID_SDL2_GetDisplayMode(width, height, bpp));

	/* Make window fullscreen if needed, and show the window */

	if (fullscreen)
	{
		Uint32 flags = vid_desktopfullscreen.value ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
		if (SDL_SetWindowFullscreen(draw_context, flags) != 0)
			Sys_Error("Couldn't set fullscreen state mode");
	}

	SDL_ShowWindow(draw_context);

	/* Create GL context if needed */
	if (!gl_context)
	{
		gl_context = SDL_GL_CreateContext(draw_context);
		if (!gl_context)
			Sys_Error("Couldn't create GL context");
	}

	gl_swap_control = true;
	if (SDL_GL_SetSwapInterval((vid_vsync.value) ? 1 : 0) == -1)
		gl_swap_control = false;

	vid.width = VID_GetCurrentWidth();
	vid.height = VID_GetCurrentHeight();
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;

// read the obtained z-buffer depth
	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthbits) == -1)
		depthbits = 0;

// read obtained fsaa samples
	if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &fsaa_obtained) == -1)
		fsaa_obtained = 0;

// read stencil bits
	if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &gl_stencilbits) == -1)
		gl_stencilbits = 0;

	modestate = VID_GetFullscreen() ? MODE_FULLSCREEN : MODE_WINDOWED;

//	CDAudio_Resume ();
//	BGM_Resume ();
	scr_disabled_for_loading = temp;

	// fix the leftover Alt from any Alt-Tab or the like that switched us away

//	Key_ClearStates ();
//	IN_ClearStates ();

	Con_SafePrintf("Video mode %dx%dx%d (%d-bit z-buffer, %dx FSAA) initialized\n", VID_GetCurrentWidth(), VID_GetCurrentHeight(), VID_GetCurrentBPP(),
			depthbits, fsaa_obtained);

	vid.recalc_refdef = 1;

// no pending changes
	vid_changed = false;

	return true;
}

/*
 ===================
 VID_Changed_f -- kristian -- notify us that a value has changed that requires a vid_restart
 ===================
 */
static void VID_Changed_f(cvar_t *var)
{
	vid_changed = true;
}

///*
//===================
//VID_Restart -- johnfitz -- change video modes on the fly
//===================
//*/
//static void VID_Restart (void)
//{
//	int width, height, bpp;
//	bool fullscreen;
//
//	if (vid_locked || !vid_changed)
//		return;
//
//	width = (int)vid_width.value;
//	height = (int)vid_height.value;
//	bpp = (int)vid_bpp.value;
//	fullscreen = vid_fullscreen.value ? true : false;
//
////
//// validate new mode
////
//	if (!VID_ValidMode (width, height, bpp, fullscreen))
//	{
//		Con_Printf ("%dx%dx%d %s is not a valid mode\n",
//				width, height, bpp, fullscreen? "fullscreen" : "windowed");
//		return;
//	}
//
//// ericw -- OS X, SDL1: textures, VBO's invalid after mode change
////          OS X, SDL2: still valid after mode change
//// To handle both cases, delete all GL objects (textures, VBO, GLSL) now.
//// We must not interleave deleting the old objects with creating new ones, because
//// one of the new objects could be given the same ID as an invalid handle
//// which is later deleted.
//
//	TexMgr_DeleteTextureObjects ();
//	GLSLGamma_DeleteTexture ();
//	R_DeleteShaders ();
//	GL_DeleteBModelVertexBuffer ();
//	GLMesh_DeleteVertexBuffers ();
//
////
//// set new mode
////
//	VID_SetMode (width, height, bpp, fullscreen);
//
//	GL_Init ();
//	TexMgr_ReloadImages ();
//	GL_BuildBModelVertexBuffer ();
//	GLMesh_LoadVertexBuffers ();
//	GL_SetupState ();
//	Fog_SetupState ();
//
//	//warpimages needs to be recalculated
//	TexMgr_RecalcWarpImageSize ();
//
//	//conwidth and conheight need to be recalculated
////	vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : (scr_conscale.value > 0) ? (int)(vid.width/scr_conscale.value) : vid.width;
//	vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
//	vid.conwidth &= 0xFFFFFFF8;
//	vid.conheight = vid.conwidth * vid.height / vid.width;
////
//// keep cvars in line with actual mode
////
//	VID_SyncCvars();
////
//// update mouse grab
////
//	if (key_dest == key_console || key_dest == key_menu)
//	{
//		if (modestate == MODE_WINDOWED)
//			IN_Deactivate(true);
//		else if (modestate == MODE_FULLSCREEN)
//			IN_Activate();
//	}
//}

/*
 ================
 VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
 ================
 */
static void VID_Test(void)
{
	int old_width, old_height, old_bpp, old_fullscreen;

	if (vid_locked || !vid_changed)
		return;
//
// now try the switch
//
	old_width = VID_GetCurrentWidth();
	old_height = VID_GetCurrentHeight();
	old_bpp = VID_GetCurrentBPP();
	old_fullscreen = VID_GetFullscreen() ? true : false;

//	VID_Restart ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_SetValue("vid_width", old_width);
		Cvar_SetValue("vid_height", old_height);
		Cvar_SetValue("vid_bpp", old_bpp);
		Cvar_Set("vid_fullscreen", old_fullscreen ? "1" : "0");
//		VID_Restart ();
	}
}

/*
 ================
 VID_Unlock -- johnfitz
 ================
 */
static void VID_Unlock(void)
{
	vid_locked = false;
	VID_SyncCvars();
}

//==============================================================================
//
//	OPENGL STUFF
//
//==============================================================================

/*
 ===============
 GL_MakeNiceExtensionsList -- johnfitz
 ===============
 */
static char *GL_MakeNiceExtensionsList(const char *in)
{
	char *copy, *token, *out;
	int i, count;

	if (!in)
		return (char *) strdup("(none)");

	//each space will be replaced by 4 chars, so count the spaces before we malloc
	for (i = 0, count = 1; i < (int) strlen(in); i++)
	{
		if (in[i] == ' ')
			count++;
	}

	out = (char *) Z_Malloc(strlen(in) + count * 3 + 1); //usually about 1-2k
	out[0] = 0;

	copy = (char *) strdup(in);
	for (token = strtok(copy, " "); token; token = strtok(NULL, " "))
	{
		strcat(out, "\n   ");
		strcat(out, token);
	}

	free(copy);
	return out;
}

/*
 ===============
 GL_Info_f -- johnfitz
 ===============
 */
static void GL_Info_f(void)
{
	Con_SafePrintf("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf("GL_VERSION: %s\n", gl_version);
	Con_Printf("GL_EXTENSIONS: %s\n", gl_extensions_nice);
}

/*
 ===============
 GL_CheckExtensions
 ===============
 */
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
	int swap_control;

	// ARB_vertex_buffer_object
	//
//	if (COM_CheckParm("-novbo"))
//		Con_Warning ("Vertex buffer objects disabled at command line\n");
//	else if (gl_version_major < 1 || (gl_version_major == 1 && gl_version_minor < 5))
//		Con_Warning ("OpenGL version < 1.5, skipping ARB_vertex_buffer_object check\n");
//	else
//	{
//		GL_BindBufferFunc = (PFNGLBINDBUFFERARBPROC) SDL_GL_GetProcAddress("glBindBufferARB");
//		GL_BufferDataFunc = (PFNGLBUFFERDATAARBPROC) SDL_GL_GetProcAddress("glBufferDataARB");
//		GL_BufferSubDataFunc = (PFNGLBUFFERSUBDATAARBPROC) SDL_GL_GetProcAddress("glBufferSubDataARB");
//		GL_DeleteBuffersFunc = (PFNGLDELETEBUFFERSARBPROC) SDL_GL_GetProcAddress("glDeleteBuffersARB");
//		GL_GenBuffersFunc = (PFNGLGENBUFFERSARBPROC) SDL_GL_GetProcAddress("glGenBuffersARB");
//		if (GL_BindBufferFunc && GL_BufferDataFunc && GL_BufferSubDataFunc && GL_DeleteBuffersFunc && GL_GenBuffersFunc)
//		{
//			Con_Printf("FOUND: ARB_vertex_buffer_object\n");
//			gl_vbo_able = true;
//		}
//		else
//		{
//			Con_Warning ("ARB_vertex_buffer_object not available\n");
//		}
//	}

	// swap control
	//
	if (!gl_swap_control)
	{
		Con_Warning("vertical sync not supported (SDL_GL_SetSwapInterval failed)\n");
	}
	else if ((swap_control = SDL_GL_GetSwapInterval()) == -1)
	{
		gl_swap_control = false;
		Con_Warning("vertical sync not supported (SDL_GL_GetSwapInterval failed)\n");
	}
	else if ((vid_vsync.value && swap_control != 1) || (!vid_vsync.value && swap_control != 0))
	{
		gl_swap_control = false;
		Con_Warning("vertical sync not supported (swap_control doesn't match vid_vsync)\n");
	}
	else
	{
		Con_Printf("FOUND: SDL_GL_SetSwapInterval\n");
	}

	// anisotropic filtering
	//
	if (GL_ParseExtensionList(gl_extensions, "GL_EXT_texture_filter_anisotropic"))
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

		if (test1 == 1 && test2 == 2)
		{
			Con_Printf("FOUND: EXT_texture_filter_anisotropic\n");
			gl_anisotropy_able = true;
		}
		else
		{
			Con_Warning("anisotropic filtering locked by driver. Current driver setting is %f\n", test1);
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
	else
	{
		gl_max_anisotropy = 1;
		Con_Warning("texture_filter_anisotropic not supported\n");
	}

	// texture_non_power_of_two
	//
	if (COM_CheckParm("-notexturenpot"))
		Con_Warning("texture_non_power_of_two disabled at command line\n");
	else if (GL_ParseExtensionList(gl_extensions, "GL_ARB_texture_non_power_of_two"))
	{
		Con_Printf("FOUND: ARB_texture_non_power_of_two\n");
		gl_texture_NPOT = true;
	}
	else
	{
		Con_Warning("texture_non_power_of_two not supported\n");
	}
}

/*
 ===============
 GL_SetupState -- johnfitz

 does all the stuff from GL_Init that needs to be done every time a new GL render context is created
 ===============
 */
static void GL_SetupState(void)
{
	glClearColor(0.15, 0.15, 0.15, 0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);
//	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_FLAT);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glDepthRangef(0.0f, 1.0f);
	glDepthFunc(GL_LEQUAL);
}

/*
 ===============
 GL_Init
 ===============
 */
static void GL_Init(void)
{
	gl_vendor = (const char *) glGetString(GL_VENDOR);
	gl_renderer = (const char *) glGetString(GL_RENDERER);
	gl_version = (const char *) glGetString(GL_VERSION);
	gl_extensions = (const char *) glGetString(GL_EXTENSIONS);

	Con_SafePrintf("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf("GL_VERSION: %s\n", gl_version);

	if (gl_version == NULL || sscanf(gl_version, "%d.%d", &gl_version_major, &gl_version_minor) < 2)
	{
		gl_version_major = 0;
		gl_version_minor = 0;
	}

	if (gl_extensions_nice != NULL)
		Z_Free(gl_extensions_nice);
	gl_extensions_nice = GL_MakeNiceExtensionsList(gl_extensions);

	GL_CheckExtensions();

//	GLAlias_CreateShaders ();
//	GL_ClearBufferBindings ();
}

/*
 =================
 GL_BeginRendering -- sets values of glx, gly, glwidth, glheight
 =================
 */
void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void GL_EndRendering(void)
{
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	SDL_GL_SwapWindow(draw_context);
}

void VID_Shutdown(void)
{
	if (!vid_initialized)
		return;

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	draw_context = NULL;
	gl_context = NULL;
//	PL_VID_Shutdown();
}

//==========================================================================
//
//  COMMANDS
//
//==========================================================================

/*
 =================
 VID_DescribeCurrentMode_f
 =================
 */
static void VID_DescribeCurrentMode_f(void)
{
	if (draw_context)
		Con_Printf("%dx%dx%d %s\n", VID_GetCurrentWidth(), VID_GetCurrentHeight(), VID_GetCurrentBPP(),
				VID_GetFullscreen() ? "fullscreen" : "windowed");
}

/*
 =================
 VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
 =================
 */
static void VID_DescribeModes_f(void)
{
	int i;
	int lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_SafePrintf("\n");
			Con_SafePrintf("   %4i x %4i x %i", modelist[i].width, modelist[i].height, modelist[i].bpp);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf("\n%i modes\n", count);
}

//==========================================================================
//
//  INIT
//
//==========================================================================

static void VID_InitModelist(void)
{
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

	nummodes = 0;
	for (i = 0; i < sdlmodes; i++)
	{
		SDL_DisplayMode mode;

		if (nummodes >= MAX_MODE_LIST)
			break;
		if (SDL_GetDisplayMode(0, i, &mode) == 0)
		{
			modelist[nummodes].width = mode.w;
			modelist[nummodes].height = mode.h;
			modelist[nummodes].bpp = SDL_BITSPERPIXEL(mode.format);
			nummodes++;
		}
	}
}

void VID_Init(unsigned char *palette)
{
	int p, width, height, bpp, display_width, display_height, display_bpp;
	bool fullscreen;
//	const char *read_vars[] = {
//		"vid_fullscreen",
//		"vid_width",
//		"vid_height",
//		"vid_bpp",
//		"vid_vsync",
//	};

#define num_readvars	( sizeof(read_vars)/sizeof(read_vars[0]) )

	Cvar_RegisterVariable(&vid_fullscreen);
	Cvar_SetCallback(&vid_fullscreen, VID_Changed_f);
	Cvar_RegisterVariable(&vid_width);
	Cvar_SetCallback(&vid_width, VID_Changed_f);
	Cvar_RegisterVariable(&vid_height);
	Cvar_SetCallback(&vid_height, VID_Changed_f);
	Cvar_RegisterVariable(&vid_bpp);
	Cvar_SetCallback(&vid_bpp, VID_Changed_f);
	Cvar_RegisterVariable(&vid_vsync);
	Cvar_SetCallback(&vid_vsync, VID_Changed_f);

	Cmd_AddCommand("vid_unlock", VID_Unlock); //johnfitz
//	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand("vid_describemodes", VID_DescribeModes_f);

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error("Couldn't init SDL video: %s", SDL_GetError());

	SDL_DisplayMode mode;
	if (SDL_GetDesktopDisplayMode(0, &mode) != 0)
		Sys_Error("Could not get desktop display mode");

	display_width = mode.w;
	display_height = mode.h;
	display_bpp = SDL_BITSPERPIXEL(mode.format);

	Cvar_SetValue("vid_bpp", (float) display_bpp);

//	if (CFG_OpenConfig("config.cfg") == 0)
//	{
//		CFG_ReadCvars(read_vars, num_readvars);
//		CFG_CloseConfig();
//	}
//	CFG_ReadCvarOverrides(read_vars, num_readvars);

	VID_InitModelist();

	width = (int) vid_width.value;
	height = (int) vid_height.value;
	bpp = (int) vid_bpp.value;
	fullscreen = (int) vid_fullscreen.value;

	if (COM_CheckParm("-current"))
	{
		width = display_width;
		height = display_height;
		bpp = display_bpp;
		fullscreen = true;
	}
	else
	{
		p = COM_CheckParm("-width");
		if (p && p < com_argc - 1)
		{
			width = atoi(com_argv[p + 1]);

			if (!COM_CheckParm("-height"))
				height = width * 3 / 4;
		}

		p = COM_CheckParm("-height");
		if (p && p < com_argc - 1)
		{
			height = atoi(com_argv[p + 1]);

			if (!COM_CheckParm("-width"))
				width = height * 4 / 3;
		}

		p = COM_CheckParm("-bpp");
		if (p && p < com_argc - 1)
			bpp = atoi(com_argv[p + 1]);

		if (COM_CheckParm("-window") || COM_CheckParm("-w"))
			fullscreen = false;
		else if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
			fullscreen = true;
	}

	if (!VID_ValidMode(width, height, bpp, fullscreen))
	{
		width = (int) vid_width.value;
		height = (int) vid_height.value;
		bpp = (int) vid_bpp.value;
		fullscreen = (int) vid_fullscreen.value;
	}

	if (!VID_ValidMode(width, height, bpp, fullscreen))
	{
		width = 640;
		height = 480;
		bpp = display_bpp;
		fullscreen = false;
	}

	vid_initialized = true;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *) vid.colormap + 2048));

	// set window icon
//	PL_SetWindowIcon();

	VID_SetMode(width, height, bpp, fullscreen);

	GL_Init();
	GL_SetupState();
	Cmd_AddCommand("gl_info", GL_Info_f); //johnfitz

	//johnfitz -- removed code creating "glquake" subdirectory

	//vid_menucmdfn = VID_Menu_f; //johnfitz
	//vid_menudrawfn = VID_MenuDraw;
	//vid_menukeyfn = VID_MenuKey;

	//Check_GammaOld(palette);
	VID_SetPaletteOld(palette);

//	VID_Gamma_Init(); //johnfitz
//	VID_Menu_Init(); //johnfitz

	//QuakeSpasm: current vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
	vid_locked = true;
}

// new proc by S.A., called by alt-return key binding.
void VID_Toggle(void)
{
	// disabling the fast path completely because SDL_SetWindowFullscreen was changing
	// the window size on SDL2/WinXP and we weren't set up to handle it. --ericw
	//
	// TODO: Clear out the dead code, reinstate the fast path using SDL_SetWindowFullscreen
	// inside VID_SetMode, check window size to fix WinXP issue. This will
	// keep all the mode changing code in one place.
	static bool vid_toggle_works = false;
	bool toggleWorked;
	Uint32 flags = 0;

	S_ClearBuffer();

	if (!vid_toggle_works)
		goto vrestart;
	else if (gl_vbo_able)
	{
		// disabling the fast path because with SDL 1.2 it invalidates VBOs (using them
		// causes a crash, sugesting that the fullscreen toggle created a new GL context,
		// although texture objects remain valid for some reason).
		//
		// SDL2 does promise window resizes / fullscreen changes preserve the GL context,
		// so we could use the fast path with SDL2. --ericw
		vid_toggle_works = false;
		goto vrestart;
	}

	if (!VID_GetFullscreen())
	{
		flags = vid_desktopfullscreen.value ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
	}

	toggleWorked = SDL_SetWindowFullscreen(draw_context, flags) == 0;

	if (toggleWorked)
	{
		Sbar_Changed();	// Sbar seems to need refreshing

		modestate = VID_GetFullscreen() ? MODE_FULLSCREEN : MODE_WINDOWED;

		VID_SyncCvars();

		// update mouse grab
		if (key_dest == key_console || key_dest == key_menu)
		{
			if (modestate == MODE_WINDOWED)
				IN_Deactivate(true);
			else if (modestate == MODE_FULLSCREEN)
				IN_Activate();
		}
	}
	else
	{
		vid_toggle_works = false;
		Con_DPrintf("SDL_WM_ToggleFullScreen failed, attempting VID_Restart\n");
		vrestart: Cvar_Set("vid_fullscreen", VID_GetFullscreen() ? "0" : "1");
		Cbuf_AddText("vid_restart\n");
	}
}

/*
 ================
 VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
 ================
 */
void VID_SyncCvars(void)
{
	if (draw_context)
	{
		if (!VID_GetDesktopFullscreen())
		{
			Cvar_SetValue("vid_width", VID_GetCurrentWidth());
			Cvar_SetValue("vid_height", VID_GetCurrentHeight());
		}
		Cvar_SetValue("vid_bpp", VID_GetCurrentBPP());
		Cvar_Set("vid_fullscreen", VID_GetFullscreen() ? "1" : "0");
		Cvar_Set("vid_vsync", VID_GetVSync() ? "1" : "0");
	}

	vid_changed = false;
}

void VID_SetPaletteOld(unsigned char *palette)
{
	byte *pal = palette;
	unsigned r, g, b, v;
	int i;
	unsigned *table = d_8to24table;

// 8 8 8 encoding

	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++, pal += 3)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		*table++ = v;
	}

	d_8to24table[255] = 0;  // 255 is transparent "MH: says this fixes pink edges"
	//d_8to24table[255] &= 0xffffff;        // 255 is transparent
}

void VID_ShiftPaletteOld(unsigned char *palette)
{
//     extern  byte rampsold[3][256];
//     VID_SetPaletteOld (palette);
//     gammaworks = SetDeviceGammaRamp (maindc, ramps);
}

