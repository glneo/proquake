/*
 * Copyright (C) 2015 Andrew F. Davis
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

#include <GLFW/glfw3.h>

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

static GLFWwindow* window;

void window_close_callback(GLFWwindow* window)
{
	Sys_Quit();
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	int press, q_key;

	if (action == GLFW_PRESS)
		press = true;
	else if (action == GLFW_RELEASE)
		press = false;
	else
		return;

	switch (button)
	{
	case GLFW_MOUSE_BUTTON_LEFT: q_key = K_MOUSE1; break;
	case GLFW_MOUSE_BUTTON_RIGHT: q_key = K_MOUSE2; break;
	default: q_key = 0; break;
	}

	Key_Event(q_key, 0, press);
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
	if (vid.conheight > height)
		vid.conheight = height;

	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 200.0);
	vid.numpages = 2;

	window = glfwCreateWindow(width, height, "QleanQuake", NULL, NULL);

	if (!window)
	{
		fprintf(stderr, "Error couldn't create window\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	glfwMakeContextCurrent(window);

	GL_Init();

	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetWindowCloseCallback(window, window_close_callback);

	snprintf(gldir, sizeof(gldir), "%s/OpenGL", com_gamedir);
//	Sys_mkdir (gldir);

        Check_GammaOld(palette);
	VID_SetPaletteOld(palette);

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	vid.recalc_refdef = 1; // force a surface cache flush

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
	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	glfwSetCursorPos(window, vid.width/2, vid.height/2);

	mouse_x -= (vid.width/2);
	mouse_y -= (vid.height/2);

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

	/* add mouse X/Y movement to cmd */
	if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))    // Baker 3.60 - Freelook cvar support
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (mlook_active)
		V_StopPitchDrift();    // Baker 3.60 - Freelook cvar support

	if (mlook_active && !(in_strafe.state & 1))     // Baker 3.60 - Freelook cvar support
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;

		// JPG 1.05 - added pq_fullpitch
		if (pq_fullpitch.value)
		{
			if (cl.viewangles[PITCH] > 90)
				cl.viewangles[PITCH] = 90;
			if (cl.viewangles[PITCH] < -90)
				cl.viewangles[PITCH] = -90;
		}
		else
		{
			if (cl.viewangles[PITCH] > 80)
				cl.viewangles[PITCH] = 80;
			if (cl.viewangles[PITCH] < -70)
				cl.viewangles[PITCH] = -70;
		}
	}
	else
	{
		cmd->forwardmove -= m_forward.value * mouse_y;
	}
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
