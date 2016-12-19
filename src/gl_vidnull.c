/*
 * Null vid component
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
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

#define MAX_MODE_LIST	600
#define MAX_BPPS_LIST	5
#define WARP_WIDTH	320
#define WARP_HEIGHT	200
#define MAXWIDTH	10000
#define MAXHEIGHT	10000

unsigned d_8to24table[256];

int texture_extension_number = 1;

int texture_mode = GL_LINEAR;

viddef_t vid; // global video state
modestate_t modestate = MODE_NONE;
bool scr_skipupdate;

// view origin and direction
vec3_t r_origin, vright, vpn, vup;

bool scr_disabled_for_loading;

float scr_centertime_off;

int m_return_state;
bool m_return_onerror;
char m_return_reason[64];

texture_t *r_notexture_mip;

int clearnotify;

refdef_t r_refdef;

cvar_t default_fov;
cvar_t scr_fov;

cvar_t pq_timer = { "pq_timer", "1", false }; // JPG - show timer
cvar_t pq_scoreboard_pings = { "pq_scoreboard_pings", "1", false };	// JPG - show ping times in the scoreboard

int r_framecount;

cvar_t r_interpolate_transform = { "r_interpolate_transform", "0", true };

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{

}

void GL_EndRendering(void)
{

}

void VID_Shutdown(void)
{

}

void VID_Init(unsigned char *palette)
{

}

void V_Init(void) {}
void M_Init(void) {}
void Draw_Init(void) {}
void SCR_Init(void) {}
void R_Init(void) {}
void Sbar_Init(void) {}

void Sbar_Changed(void) {}

void Draw_BeginDisc(void) {}
void Draw_EndDisc(void) {}
void SCR_EndLoadingPlaque(void) {}
void SCR_UpdateScreen(void) {}

void M_Menu_Quit_f(void) {}

void SCR_BeginLoadingPlaque(void) {}

int SCR_ModalMessage(char *text, float timeout) { return 0; }

void M_Keydown(int key, int ascii, bool down) {}
void M_ToggleMenu_f(void) {}

float V_CalcRoll(vec3_t angles, vec3_t velocity) { return 0.0f; }

int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode) { return 0; }

void GL_SubdivideSurface(brush_model_t *brushmodel, msurface_t *fa) {}

void ConvertPixels (byte *pixels, int num_pixels) {}

int FindFullbrightTexture (byte *pixels, int num_pix)  { return 0; }

void R_InitSky(texture_t *mt, byte *src) {}

void GL_FreeTextures(void) {}

void V_StopPitchDrift(void) {}

void Draw_Character(int x, int y, int num) {}
void Draw_ConsoleBackground(int lines) {}
void Draw_String(int x, int y, char *str) {}

void M_Menu_Main_f(void) {}

void V_StartPitchDrift_f(void) {}

void SCR_CenterPrint(char *str) {}

void V_ParseDamage(void) {}

void R_TranslatePlayerSkin(int playernum) {}

void R_NewMap(void) {}
