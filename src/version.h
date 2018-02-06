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

#define ENGINE_NAME "QuickQuake"
#define ENGINE_VERSION "0.01 Beta"
#define ENGINE_HOMEPAGE_URL "https://github.com/glneo/proquake"
#define PROQUAKE_SERIES_VERSION 0.01

// Define Operating System Names
#ifdef _WIN32
	#define OS_NAME "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
	#define OS_NAME "Mac OSX"
#else 
	#define OS_NAME "Linux" // Everything else gets to be Linux for now ;)
#endif

// Define Renderer Name
#ifdef OPENGLES
#define RENDERER_NAME "GLes"
#else
#define RENDERER_NAME "GL"
#endif

// Define Capabilities
//#define PARANOID // speed sapping error checking

#endif /* __VERSION_H */
