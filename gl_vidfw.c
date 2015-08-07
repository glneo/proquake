/*
Copyright (C) 2015 Andrew F. Davis
Please see the file "AUTHORS" for a list of contributors

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

#include "quakedef.h"

#include <GLFW/glfw3.h>

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

GLFWwindow* window;

void window_close_callback(GLFWwindow* window)
{
	Sys_Quit();
	exit(0);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	int key_down, q_key;
	if (action == GLFW_PRESS)
		key_down = true;
	else if (action == GLFW_RELEASE)
		key_down = false;
	else
		return;

	switch (key)
	{
	case GLFW_KEY_PAGE_UP: q_key = K_PGUP; break;
	case GLFW_KEY_PAGE_DOWN: q_key = K_PGDN; break;
	case GLFW_KEY_HOME: q_key = K_HOME; break;
	case GLFW_KEY_END: q_key = K_END; break;
	case GLFW_KEY_LEFT: q_key = K_LEFTARROW; break;
	case GLFW_KEY_RIGHT: q_key = K_RIGHTARROW; break;
	case GLFW_KEY_DOWN: q_key = K_DOWNARROW; break;
	case GLFW_KEY_UP: q_key = K_UPARROW; break;
	case GLFW_KEY_ESCAPE: q_key = K_ESCAPE; break;
	case GLFW_KEY_ENTER: q_key = K_ENTER; break;
	case GLFW_KEY_TAB: q_key = K_TAB; break;
	case GLFW_KEY_F1: q_key = K_F1; break;
	case GLFW_KEY_F2: q_key = K_F2; break;
	case GLFW_KEY_F3: q_key = K_F3; break;
	case GLFW_KEY_F4: q_key = K_F4; break;
	case GLFW_KEY_F5: q_key = K_F5; break;
	case GLFW_KEY_F6: q_key = K_F6; break;
	case GLFW_KEY_F7: q_key = K_F7; break;
	case GLFW_KEY_F8: q_key = K_F8; break;
	case GLFW_KEY_F9: q_key = K_F9; break;
	case GLFW_KEY_F10: q_key = K_F10; break;
	case GLFW_KEY_F11: q_key = K_F11; break;
	case GLFW_KEY_F12: q_key = K_F12; break;
	case GLFW_KEY_BACKSPACE: q_key = K_BACKSPACE; break;
	case GLFW_KEY_DELETE: q_key = K_DEL; break;
	case GLFW_KEY_PAUSE: q_key = K_PAUSE; break;
	case GLFW_KEY_LEFT_SHIFT: 
	case GLFW_KEY_RIGHT_SHIFT: q_key = K_SHIFT; break;
	case GLFW_KEY_LEFT_CONTROL:
	case GLFW_KEY_RIGHT_CONTROL: q_key = K_CTRL; break;
	case GLFW_KEY_LEFT_ALT:
	case GLFW_KEY_RIGHT_ALT: q_key = K_ALT; break;
	case GLFW_KEY_INSERT: q_key = K_INS; break;
	case GLFW_KEY_KP_MULTIPLY: q_key = '*'; break;
	case GLFW_KEY_KP_ADD: q_key = '+'; break;
	case GLFW_KEY_KP_SUBTRACT: q_key = '-'; break;
	case GLFW_KEY_KP_DIVIDE: q_key = '/'; break;
	case GLFW_KEY_SPACE: q_key = 32; break;
	default: q_key = 0; break;
	}

	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
		q_key = key + ('a' - 'A');

	Key_Event(q_key, 0, key_down);
}

/*
 * VID_Init
 */
void VID_Init(unsigned char *palette)
{
	int i;
	char gldir[MAX_OSPATH];
	int width = 640, height = 480;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if (!glfwInit())
	{
		fprintf(stderr, "Error couldn't initialize GLFW\n");
		exit(EXIT_FAILURE);
	}
	/*
	 * Interpret command-line params
	 */

	/* Set vid parameters */
	if ((i = COM_CheckParm("-width")) != 0)
		width = atoi(com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		height = atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = atoi(com_argv[i+1]);
	else
		vid.conwidth = width;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	vid.width = width;
	vid.height = height;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

	window = glfwCreateWindow(width, height, "QleanQuake", glfwGetPrimaryMonitor(), NULL);

	if (!window)
	{
		fprintf(stderr, "Error couldn't create window\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);

	GL_Init();

	glfwSetKeyCallback(window, key_callback);
	glfwSetWindowCloseCallback(window, window_close_callback);

	snprintf(gldir, sizeof(gldir), "%s/OpenGL", com_gamedir);
	Sys_mkdir (gldir);

        Check_GammaOld(palette);
	VID_SetPaletteOld(palette);

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	vid.recalc_refdef = 1;		// force a surface cache flush

	return;
}

/*
 * VID_Shutdown
 */
void VID_Shutdown(void)
{
	glfwDestroyWindow(window);
	glfwTerminate();
	return;
}

/*
 * GL_BeginRendering
 */
void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	glfwGetFramebufferSize(window, width, height);
	glViewport(0, 0, *width, *height);
}

/*
 * GL_EndRendering
 */
void GL_EndRendering(void)
{
	glFlush();
	glfwSwapBuffers(window);
	return;
}

/*
 * Sys_SendKeyEvents
 */
void Sys_SendKeyEvents(void)
{
	glfwPollEvents();
	return;
}

/*
 * IN_Move
 */
void IN_Move(usercmd_t *cmd)
{
	return;
}

/*
 * IN_Commands
 */
void IN_Commands(void)
{
	return;
}

/*
 * IN_Init
 */
void IN_Init(void)
{
	return;
}

/*
 * IN_Shutdown
 */
void IN_Shutdown(void)
{
	return;
}
