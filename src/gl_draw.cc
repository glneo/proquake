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

cvar_t gl_smoothfont = { "gl_smoothfont", "1", CVAR_ARCHIVE };

byte *draw_chars; // 8*8 graphic characters

gltexture_t *translate_texture;
gltexture_t *char_texture;

qpic_t crosshairpic;

byte conback_buffer[sizeof(qpic_t)];
qpic_t *conback = (qpic_t *) &conback_buffer;

canvastype currentcanvas = CANVAS_NONE; //johnfitz -- for GL_SetCanvas

/*
 =============================================================================

 scrap allocation

 Allocate all the little status bar objects into a single texture

 to crutch up stupid hardware / drivers

 =============================================================================
 */

// some cards have low quality of alpha pics, so load the pics
// without transparent pixels into a different scrap block.
// scrap 0 is solid pics, 1 is transparent
#define	MAX_SCRAPS      2
#define	BLOCK_WIDTH     256
#define	BLOCK_HEIGHT    256

byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT * 4];
gltexture_t *scrap_textures[MAX_SCRAPS]; //johnfitz
int scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
int scrap_texnum;

/* returns a texture number and the position inside it */
static int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	for (int texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		int best = BLOCK_HEIGHT;

		for (int i = 0; i < BLOCK_WIDTH - w; i++)
		{
			int j, local_best = 0;
			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i + j] >= best)
					break;
				if (scrap_allocated[texnum][i + j] > local_best)
					local_best = scrap_allocated[texnum][i + j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = local_best;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (int i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("Scrap full");
	return 0;
}

void Scrap_Upload(void)
{
	int i;

	for (i = 0; i < MAX_SCRAPS; i++)
	{
		char name[8];
		sprintf(name, "scrap%i", i);
		scrap_textures[i] = TexMgr_LoadImage(name, BLOCK_WIDTH, BLOCK_HEIGHT, SRC_INDEXED, scrap_texels[i], TEX_ALPHA | TEX_OVERWRITE | TEX_NOPICMIP);
	}
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char name[MAX_QPATH];
	qpic_t pic;
} cachepic_t;

#define	MAX_CACHED_PICS		128
static cachepic_t menu_cachepics[MAX_CACHED_PICS];
static int menu_numcachepics;

qpic_t *Draw_PicFromWad(const char *name)
{
	dqpic_t *p = (dqpic_t *) W_GetLumpName(name);
	qpic_t *gl = (qpic_t *) Q_malloc(sizeof(*gl));

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int x, y;
		int texnum = Scrap_AllocBlock(p->width, p->height, &x, &y);
		int k = 0;
		for (int i = 0; i < p->height; i++)
			for (int j = 0; j < p->width; j++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k++];
		gl->gltexture = scrap_textures[texnum];
		gl->sl =  x              / (float) BLOCK_WIDTH;
		gl->sh = (x + p->width)  / (float) BLOCK_WIDTH;
		gl->tl =  y              / (float) BLOCK_WIDTH;
		gl->th = (y + p->height) / (float) BLOCK_WIDTH;
	}
	else
	{
		char texturename[64];
		snprintf(texturename, sizeof(texturename), "%s", name);

		gl->gltexture = TexMgr_LoadImage(texturename, p->width, p->height, SRC_INDEXED, p->data, TEX_ALPHA | TEX_PAD | TEX_NOPICMIP);
		gl->sl = 0;
		gl->sh = (float) p->width / (float) TexMgr_PadConditional(p->width);
		gl->tl = 0;
		gl->th = (float) p->height / (float) TexMgr_PadConditional(p->height);
	}
	gl->width = p->width;
	gl->height = p->height;

	return gl;
}

qpic_t *Draw_CachePic(const char *path)
{
	for (int i = 0; i < menu_numcachepics; i++)
		if (!strcmp(path, menu_cachepics[i].name))
			return &menu_cachepics[i].pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");

	cachepic_t *cpic = &menu_cachepics[menu_numcachepics];
	menu_numcachepics++;

	strcpy(cpic->name, path);
	qpic_t *pic = &cpic->pic;

	// load the pic from disk
	dqpic_t *dat = (dqpic_t *) COM_LoadMallocFile(path);
	if (!dat)
		Sys_Error("failed to load %s", path);
	SwapPic(dat);

	// FIXME: do we need to pad this image like above?
	pic->gltexture = TexMgr_LoadImage("", dat->width, dat->height, SRC_INDEXED, dat->data, TEX_ALPHA);
	pic->sl = 0;
	pic->sh = 1;
	pic->tl = 0;
	pic->th = 1;
	pic->width = dat->width;
	pic->height = dat->height;

	free(dat);

	return pic;
}

static void OnChange_gl_smoothfont(struct cvar_s *cvar)
{
	GL_Bind(char_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, cvar->value ? GL_NEAREST : GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, cvar->value ? GL_LINEAR : GL_NEAREST);
}

static bool IsValid(int y, int num)
{
	if ((num & 127) == 32)
		return false; // space

	if (y <= -8)
		return false; // totally off screen

	return true;
}

static void Character(int x, int y, int num, float alpha)
{
	num &= 255;
	int row = num >> 4;
	int col = num & 15;

	float size = 1/16.0f;
	float frow = row * size;
	float fcol = col * size;
//	float offset = size + 1/32.0f; // offset to match expanded charset texture
	float offset = size;

	GLfloat texts[] = {
		fcol,          frow,
		fcol + offset, frow,
		fcol + offset, frow + offset,
		fcol,          frow + offset,
	};

	GLfloat verts[] = { (GLfloat)x,     (GLfloat)y,
			    (GLfloat)x + 8, (GLfloat)y,
			    (GLfloat)x + 8, (GLfloat)y + 8,
			    (GLfloat)x,     (GLfloat)y + 8,
	};

	alpha = CLAMP(0, alpha, 1.0f);
	if (alpha < 1.0f)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
	}

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	if (alpha < 1.0f)
	{
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void Draw_Character(int x, int y, int num, float alpha)
{
	if (!IsValid(y, num))
		return;

	GL_Bind(char_texture);

	Character(x, y, num, alpha);
}

void Draw_String(int x, int y, const char *str, float alpha)
{
	GL_Bind(char_texture);

	while (*str)
	{
		if (IsValid(y, *str))
			Character(x, y, *str, alpha);

		str++;
		x += 8;
	}
}

void Draw_Pic(int x, int y, qpic_t *pic, float alpha)
{
	GLfloat texts[] = {
		pic->sl, pic->tl,
		pic->sh, pic->tl,
		pic->sh, pic->th,
		pic->sl, pic->th,
	};

	GLfloat verts[] = {
		(GLfloat)x,              (GLfloat)y,
		(GLfloat)x + pic->width, (GLfloat)y,
		(GLfloat)x + pic->width, (GLfloat)y + pic->height,
		(GLfloat)x,              (GLfloat)y + pic->height,
	};

	alpha = CLAMP(0, alpha, 1.0f);
	if (alpha < 1.0f)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
	}

	GL_Bind(pic->gltexture);

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);

// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

// clean up
	if (alpha < 1.0f)
	{
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
}

void Draw_TransPic(int x, int y, qpic_t *pic, float alpha)
{
	if (x < 0 ||
	    y < 0 ||
	    (unsigned)(x + pic->width) > vid.width ||
	    (unsigned)(y + pic->height) > vid.height)
		Sys_Error("bad coordinates");

	Draw_Pic(x, y, pic, alpha);
}

/* Only used for the player color selection menu */
//void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation)
//{
//	int v, u;
//	unsigned trans[64 * 64], *dest;
//	byte *src;
//	int p;
//
//	GL_Bind(translate_texture);
//
//	dest = trans;
//	for (v = 0; v < 64; v++, dest += 64)
//	{
//		src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
//		for (u = 0; u < 64; u++)
//		{
//			p = src[(u * pic->width) >> 6];
//			if (p == 255)
//				dest[u] = p;
//			else
//				dest[u] = d_8to24table[translation[p]];
//		}
//	}
//
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
//
//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//	GLfloat texts[] = {
//		0.0f, 0.0f,
//		1.0f, 0.0f,
//		1.0f, 1.0f,
//		0.0f, 1.0f,
//	};
//
//	GLfloat verts[] = {
//		(GLfloat)x,              (GLfloat)y,
//		(GLfloat)x + pic->width, (GLfloat)y,
//		(GLfloat)x + pic->width, (GLfloat)y + pic->height,
//		(GLfloat)x,              (GLfloat)y + pic->height,
//	};
//
//	glTexCoordPointer(2, GL_FLOAT, 0, texts);
//	glVertexPointer(2, GL_FLOAT, 0, verts);
//	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
//}

/* Tile graphic to fill the screen */
void Draw_PicTile(int x, int y, int w, int h, qpic_t *pic, float alpha)
{
	GL_Bind(pic->gltexture);

	GLfloat texts[] = {
		(GLfloat)x / 64.0f,       (GLfloat)y / 64.0f,
		(GLfloat)(x + w) / 64.0f, (GLfloat)y / 64.0f,
		(GLfloat)(x + w) / 64.0f, (GLfloat)(y + h) / 64.0f,
		(GLfloat)x / 64.0f,       (GLfloat)(y + h) / 64.0f,
	};

	GLfloat verts[] = {
		(GLfloat)x,     (GLfloat)y,
		(GLfloat)x + w, (GLfloat)y,
		(GLfloat)x + w, (GLfloat)y + h,
		(GLfloat)x,     (GLfloat)y + h,
	};

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);

// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* Fills a box of pixels with a single color */
void Draw_Fill(int x, int y, int w, int h, int c, float alpha)
{
	byte *pal = (byte *) d_8to24table;
	alpha = CLAMP(0, alpha, 1.0f);

	glDisable(GL_TEXTURE_2D);
	glColor4f(pal[c * 4] / 255.0,
		  pal[c * 4 + 1] / 255.0,
		  pal[c * 4 + 2] / 255.0,
		  alpha);

	GLfloat verts[] = {
		(GLfloat)x,     (GLfloat)y,
		(GLfloat)x + w, (GLfloat)y,
		(GLfloat)x + w, (GLfloat)y + h,
		(GLfloat)x,     (GLfloat)y + h,
	};

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, 0, verts);

// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

// clean up
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
}

//=============================================================================

void Draw_SetCanvas(canvastype newcanvas)
{
	float s;
	int lines;

	if (newcanvas == currentcanvas)
		return;

	Q_Matrix projectionMatrix;

	switch (newcanvas)
	{
	case CANVAS_DEFAULT:
		projectionMatrix.ortho(0, vid.width, vid.height, 0, -1.0f, 1.0f);
		glViewport(vid.x, vid.y, vid.width, vid.height);
		break;
	case CANVAS_CONSOLE:
		lines = vid.conheight - (scr_con_current * vid.conheight / vid.height);
		projectionMatrix.ortho(0, vid.conwidth, vid.conheight + lines, lines, -1.0f, 1.0f);
		glViewport(vid.x, vid.y, vid.width, vid.height);
		break;
	case CANVAS_MENU:
		s = min((float )vid.width / 320.0, (float )vid.height / 200.0);
		s = CLAMP(1.0, scr_menuscale.value, s);
		// ericw -- doubled width to 640 to accommodate long keybindings
		projectionMatrix.ortho(0, 640, 200, 0, -1.0f, 1.0f);
		glViewport(vid.x + (vid.width - 320 * s) / 2, vid.y + (vid.height - 200 * s) / 2, 640 * s, 200 * s);
		break;
	case CANVAS_SBAR:
		s = CLAMP(1.0, scr_sbarscale.value, (float )vid.width / 320.0);
		if (cl.gametype == GAME_DEATHMATCH)
		{
			projectionMatrix.ortho(0, vid.width / s, 48, 0, -1.0f, 1.0f);
			glViewport(vid.x, vid.y, vid.width, 48 * s);
		}
		else
		{
			projectionMatrix.ortho(0, 320, 48, 0, -1.0f, 1.0f);
			glViewport(vid.x + (vid.width - 320 * s) / 2, vid.y, 320 * s, 48 * s);
		}
		break;
//	case CANVAS_WARPIMAGE:
//		projectionMatrix.ortho(0, 128, 0, 128, -1.0f, 1.0f);
//		glViewport (vid.x, vid.y+vid.height-gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);
//		break;
	case CANVAS_CROSSHAIR: //0,0 is center of viewport
		s = CLAMP(1.0, scr_crosshairscale.value, 10.0);
		projectionMatrix.ortho(scr_vrect.width / -2 / s, scr_vrect.width / 2 / s, scr_vrect.height / 2 / s, scr_vrect.height / -2 / s, -1.0f, 1.0f);
		glViewport(scr_vrect.x, vid.height - scr_vrect.y - scr_vrect.height, scr_vrect.width & ~1, scr_vrect.height & ~1);
		break;
	case CANVAS_BOTTOMLEFT: //used by devstats
		s = (float) vid.width / vid.conwidth; //use console scale
		projectionMatrix.ortho(0, 320, 200, 0, -1.0f, 1.0f);
		glViewport(vid.x, vid.y, 320 * s, 200 * s);
		break;
	case CANVAS_BOTTOMRIGHT: //used by fps/clock
		s = (float) vid.width / vid.conwidth; //use console scale
		projectionMatrix.ortho(0, 320, 200, 0, -1.0f, 1.0f);
		glViewport(vid.x + vid.width - 320 * s, vid.y, 320 * s, 200 * s);
		break;
	default:
		Sys_Error("bad canvas type");
	}

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(projectionMatrix.get());

	Q_Matrix modelViewMatrix;
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(modelViewMatrix.get());

	currentcanvas = newcanvas;
}

void GL_Begin2D(void)
{
	currentcanvas = CANVAS_INVALID;
	Draw_SetCanvas(CANVAS_DEFAULT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

void GL_End2D(void)
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}

/* generate pics from internal data */
qpic_t *Draw_MakePic(const char *name, int width, int height, byte *data)
{
	int flags = TEX_NEAREST | TEX_ALPHA | TEX_PERSIST | TEX_NOPICMIP | TEX_PAD;

	qpic_t *pic = (qpic_t *) Hunk_Alloc(sizeof(qpic_t));

	pic->gltexture = TexMgr_LoadImage(name, width, height, SRC_INDEXED, data, flags);
	pic->width = (float) TexMgr_PadConditional(width);
	pic->height = (float) TexMgr_PadConditional(height);
	pic->sl = 0;
	pic->sh = (float) width / pic->width;
	pic->tl = 0;
	pic->th = (float) height / pic->height;

	return pic;
}

static void Load_CharSet(void)
{
	draw_chars = (byte *) W_GetLumpName("conchars");
	for (int i = 0; i < 128 * 128; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255; // proper transparent color

	// now turn them into textures
	char_texture = TexMgr_LoadImage("charset", 128, 128, SRC_INDEXED, draw_chars, TEX_ALPHA | TEX_NOPICMIP);
}

void Draw_Init(void)
{
	Cvar_RegisterVariable(&gl_smoothfont);
	Cvar_SetCallback(&gl_smoothfont, OnChange_gl_smoothfont);

	Load_CharSet();
}
