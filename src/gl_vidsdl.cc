/*
 * SDL OpenGL Video Component
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
#include "glquake.h"

#include <SDL.h>

#define MAX_MODE_LIST	600
#define MAX_BPPS_LIST	5
#define WARP_WIDTH	320
#define WARP_HEIGHT	200
#define MAXWIDTH	10000
#define MAXHEIGHT	10000

static vmode_t modelist[MAX_MODE_LIST];
static int nummodes;

int texture_extension_number = 1;

static bool vid_initialized = false;

static SDL_Window *draw_context;
static SDL_GLContext gl_context;

static bool vid_changed = false;

int texture_mode = GL_LINEAR;

viddef_t vid; // global video state

static cvar_t vid_width = { "vid_width", "1440", CVAR_ARCHIVE };
static cvar_t vid_height = { "vid_height", "900", CVAR_ARCHIVE };
static cvar_t vid_fullscreen = { "vid_fullscreen", "0", CVAR_ARCHIVE };
static cvar_t vid_bpp = { "vid_bpp", "16", CVAR_ARCHIVE };
cvar_t vid_vsync = { "vid_vsync", "0", CVAR_ARCHIVE };
static cvar_t vid_desktopfullscreen = { "vid_desktopfullscreen", "0", CVAR_ARCHIVE };

cvar_t vid_gamma = {"gamma", "1", CVAR_ARCHIVE};
cvar_t vid_contrast = {"contrast", "1", CVAR_ARCHIVE};

static void VID_GetCurrentMode(vmode_t *mode)
{
	SDL_GetWindowSize(draw_context, &mode->width, &mode->height);
	const Uint32 pixelFormat = SDL_GetWindowPixelFormat(draw_context);
	mode->bpp = SDL_BITSPERPIXEL(pixelFormat);
	mode->type = (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN) ? MODE_FULLSCREEN : MODE_WINDOWED;
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
		if (SDL_GetDisplayMode(0, i, &mode) == 0 &&
		    mode.w == width &&
		    mode.h == height &&
		    ((int)SDL_BITSPERPIXEL(mode.format)) == bpp)
			return &mode;

	return NULL;
}

static bool VID_ValidMode(int width, int height, int bpp, bool fullscreen)
{
	if (width < 320 ||
	    height < 200)
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

extern bool gl_swap_control;

static void VID_SetMode(int width, int height, int bpp, bool fullscreen)
{
	int temp;
	Uint32 flags;
	char caption[50];
	int depthbits, stencilbits;
	vmode_t mode;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

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

//	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
//	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

#ifdef OPENGLES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

	snprintf(caption, sizeof(caption), "QuickQuake %s", ENGINE_VERSION);

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

	VID_GetCurrentMode(&mode);

	/* Ensure the window is not fullscreen */
	if (mode.type == MODE_FULLSCREEN)
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

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
	}
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	if (SDL_GL_SetSwapInterval((vid_vsync.value) ? 1 : 0) == -1)
	{
		gl_swap_control = false;
		Con_Warning("vertical sync not supported (SDL_GL_SetSwapInterval failed)\n");
	}
	else if (vid_vsync.value != SDL_GL_GetSwapInterval())
	{
		gl_swap_control = false;
		Con_Warning("vertical sync not supported (swap_control doesn't match vid_vsync)\n");
	}
	else
	{
		gl_swap_control = true;
		Con_Printf("FOUND: SDL_GL_SetSwapInterval\n");
	}

	vid.width = mode.width;
	vid.height = mode.height;
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;

	// read the obtained z-buffer depth
	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthbits) == -1)
		depthbits = 0;

	// read stencil bits
	if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencilbits) == -1)
		stencilbits = 0;

	scr_disabled_for_loading = temp;

	Con_SafePrintf("Video mode %dx%dx%d (%d-bit z-buffer, %d-bit stencil-buffer) initialized\n",
	               mode.width,
	               mode.height,
	               mode.bpp,
	               depthbits, stencilbits);

	vid.recalc_refdef = true;

	// no pending changes
	vid_changed = false;
}

/* notify us that a value has changed that requires a vid_restart */
static void VID_Changed_f(cvar_t *var)
{
	vid_changed = true;
}

/* set vid cvars to match current video mode */
static void VID_SyncCvars(void)
{
	if (!draw_context)
		return;

	vmode_t mode;
	VID_GetCurrentMode(&mode);

	Cvar_SetValueQuick(&vid_width, mode.width);
	Cvar_SetValueQuick(&vid_height, mode.height);
	Cvar_SetValueQuick(&vid_bpp, mode.bpp);
	Cvar_SetQuick(&vid_fullscreen, (mode.type == MODE_FULLSCREEN) ? "1" : "0");
	Cvar_SetQuick(&vid_vsync, VID_GetVSync() ? "1" : "0");
}

void VID_Swap(void)
{
	SDL_GL_SwapWindow(draw_context);
}

static void VID_DescribeCurrentMode_f(void)
{
	if (!draw_context)
		return;

	vmode_t mode;
	VID_GetCurrentMode(&mode);

	Con_Printf("   %dx%dx%d %s\n",
	           mode.width,
	           mode.height,
	           mode.bpp,
	           (mode.type == MODE_FULLSCREEN) ? "fullscreen" : "windowed");
}

static void VID_DescribeModes_f(void)
{
	int lastwidth = 0,
	    lastheight = 0,
	    lastbpp = 0,
	    count = 0;

	for (int i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width ||
		    lastheight != modelist[i].height ||
		    lastbpp != modelist[i].bpp)
		{
			Con_SafePrintf("   %4i x %4i x %i\n", modelist[i].width, modelist[i].height, modelist[i].bpp);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}

	Con_Printf("%i modes\n", count);
}

static void VID_InitModelist(void)
{
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

	nummodes = 0;
	for (i = 0; i < sdlmodes && nummodes < MAX_MODE_LIST; i++)
	{
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(0, i, &mode) == 0)
		{
			modelist[nummodes].width = mode.w;
			modelist[nummodes].height = mode.h;
			modelist[nummodes].bpp = SDL_BITSPERPIXEL(mode.format);
			modelist[nummodes].refreshrate = mode.refresh_rate;
			nummodes++;
		}
	}
}

void VID_Init(void)
{
	int p, width, height, bpp, display_width, display_height, display_bpp;
	bool fullscreen;

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
	Cvar_RegisterVariable (&vid_gamma);
	Cvar_RegisterVariable (&vid_contrast);

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

	Cvar_SetValueQuick(&vid_bpp, (float) display_bpp);

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

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *) vid.colormap + 2048));

	vid_initialized = true;

	VID_SetMode(width, height, bpp, fullscreen);

	VID_SyncCvars();
}

void VID_Shutdown(void)
{
	if (!vid_initialized)
		return;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	draw_context = NULL;
	gl_context = NULL;
}
