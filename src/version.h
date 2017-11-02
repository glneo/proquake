/*
 * Copyright (C) 2002, Anton Gavrilov
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

#ifndef __VERSION_H
#define __VERSION_H

#define ENGINE_NAME "ProQuake"
#define ENGINE_VERSION "4.93 Beta"
#define ENGINE_HOMEPAGE_URL "http:////www.quakeone.com//proquake"
#define PROQUAKE_SERIES_VERSION 4.93

// Create our own define for Mac OS X
#if defined(__APPLE__) && defined(__MACH__)
	#define MACOSX
#endif

// Define Operating System Names
#ifdef _WIN32
	#define OS_NAME "Windows"
#elif defined(MACOSX)
	#define OS_NAME "Mac OSX"
#else 
	#define OS_NAME "Linux"
	#define LINUX // Everything else gets to be Linux for now ;)
#endif

#define SUPPORTS_TRANSFORM_INTERPOLATION // We are switching to r_interpolate_transform

// Define Renderer Name
#ifdef OPENGLES
#define RENDERER_NAME "GLes"
#else
#define RENDERER_NAME "GL"
#endif

// Define Capabilities
#define SUPPORTS_HARDWARE_ANIM_INTERPOLATION // The hardware interpolation route
#define SUPPORTS_GL_OVERBRIGHTS // Overbright method GLQuake is using, WinQuake always had them
//#define SUPPORTS_VSYNC
#define SUPPORTS_TRANSPARENT_SBAR
//#define PARANOID // speed sapping error checking

#endif /* __VERSION_H */
