/*
Copyright (C) 2002, Anton Gavrilov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// version.h

#ifndef VERSION_H
#define VERSION_H

// Messages: 
// MSVC: #pragma message ( "text" )
// GCC version: #warning "hello"
// #error to terminate compilation

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

#define CHASE_CAM_FIX
#define SUPPORTS_TRANSFORM_INTERPOLATION // We are switching to r_interpolate_transform

// Define Support For Cheat-Free Mode
#if defined(_WIN32) || defined(Linux)
	#define SUPPORTS_CHEATFREE_MODE // Only Windows and Linux have security modules.
#endif

// Define Renderer Name
#define RENDERER_NAME "GL"

#ifdef MACOSX
	#define MACOSX_EXTRA_FEATURES
	#define MACOSX_UNKNOWN_DIFFERENCE
	#define MACOSX_NETWORK_DIFFERENCE
	#define MACOSX_KEYBOARD_EXTRAS
	#define MACOSX_KEYBOARD_KEYPAD
	#define MACOSX_PASTING
	#define MACOSX_SENS_RANGE
#endif

// Define Specific General Capabilities
#ifdef _WIN32
	#define SUPPORTS_AVI_CAPTURE                // Hopelessly Windows locked
	#define SUPPORTS_INTERNATIONAL_KEYBOARD     // I only know how to detect and address on Windows

	#define SUPPORTS_DEMO_AUTOPLAY              // Windows only.  Uses file association
	#define SUPPORTS_DIRECTINPUT 
	#define SUPPORTS_INTERNATIONAL_KEYBOARD     // Windows only implementation for now?; the extra key byte

	#define WINDOWS_SCROLLWHEEL_PEEK            // CAPTURES MOUSEWHEEL WHEN keydest != game

	// GLQUAKE additive features on top of _WIN32 only

//	# define SUPPORTS_GLVIDEO_MODESWITCH        // Windows only for now.  Probably can be multiplat in future.
	#define SUPPORTS_VSYNC                      // Vertical sync; only GL does this for now
	#define SUPPORTS_TRANSPARENT_SBAR           // Not implemented in OSX?

	#define RELEASE_MOUSE_FULLSCREEN            // D3DQUAKE gets an error if it loses focus in fullscreen, so that'd be stupid
	#define HTTP_DOWNLOAD

	#define OLD_SGIS                            // Old multitexture ... for now.
	#define INTEL_OPENGL_DRIVER_WORKAROUND		// Windows only issue?  Or is Linux affected too?  OS X is not affected
#endif


// Define Specific Rendering Capabilities
# define SUPPORTS_HARDWARE_ANIM_INTERPOLATION	// The hardware interpolation route
# define SUPPORTS_2DPICS_ALPHA					// Transparency of 2D pics
# define SUPPORTS_GL_OVERBRIGHTS				// Overbright method GLQuake is using, WinQuake always had them


#endif
