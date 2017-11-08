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

cvar_t crosshair = { "crosshair", "1", CVAR_ARCHIVE };
cvar_t cl_crossx = { "cl_crossx", "0", CVAR_ARCHIVE };
cvar_t cl_crossy = { "cl_crossy", "0", CVAR_ARCHIVE };
cvar_t cl_crosshaircentered = { "cl_crosshaircentered", "1", CVAR_ARCHIVE };
cvar_t gl_crosshairalpha = { "crosshairalpha", "1", CVAR_ARCHIVE };
cvar_t crosshairsize = { "crosshairsize", "1", CVAR_ARCHIVE };
cvar_t gl_smoothfont = { "gl_smoothfont", "1", CVAR_ARCHIVE };
cvar_t scr_conalpha = {"scr_conalpha", "0.5", CVAR_ARCHIVE}; //johnfitz

byte *draw_chars;				// 8*8 graphic characters
qpic_t *draw_backtile;

gltexture_t *translate_texture;
gltexture_t *char_texture;

qpic_t crosshairpic;

typedef struct
{
	gltexture_t *gltexture;
	float sl, tl, sh, th;
} glpic_t;

byte conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t *conback = (qpic_t *) &conback_buffer;

static gltexture_t *GL_LoadPicTexture(qpic_t *pic)
{
	return TexMgr_LoadImage("", pic->width, pic->height, SRC_INDEXED, pic->data, TEX_ALPHA);
}

#define NUMCROSSHAIRS 5
qpic_t *crosshair_pics[NUMCROSSHAIRS];

static byte crosshairdata[NUMCROSSHAIRS][64] = {
	{
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},
	{
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},
	{
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},
	{
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
		0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	},
	{
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff,
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	},
};

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
bool scrap_dirty;
gltexture_t	*scrap_textures[MAX_SCRAPS]; //johnfitz
int scrap_texnum;

// returns a texture number and the position inside it
static int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;
	static int scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];

	for (int texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i + j] >= best)
					break;
				if (scrap_allocated[texnum][i + j] > best2)
					best2 = scrap_allocated[texnum][i + j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error("full");
	return (0);
}

static void Scrap_Upload (void)
{
	int i;

	for (i = 0; i < MAX_SCRAPS; i++)
	{
		char name[8];
		sprintf (name, "scrap%i", i);
		scrap_textures[i] = TexMgr_LoadImage(name, BLOCK_WIDTH, BLOCK_HEIGHT, SRC_INDEXED, scrap_texels[i],
		                                     TEX_ALPHA | TEX_OVERWRITE | TEX_NOPICMIP);
	}

	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char name[MAX_QPATH];
	qpic_t pic;
	byte padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128

byte menuplyr_pixels[4096];

qpic_t *Draw_PicFromWad(const char *name)
{
	qpic_t *p;
	glpic_t gl;
//	src_offset_t offset;

	p = (qpic_t *) W_GetLumpName(name);
//	if (!p)
//		return pic_nul;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int x, y;
		int i, j, k;
		int texnum;

		texnum = Scrap_AllocBlock(p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i = 0; i < p->height; i++)
		{
			for (j = 0; j < p->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k];
		}
		gl.gltexture = scrap_textures[texnum];
		gl.sl = x / (float) BLOCK_WIDTH;
		gl.sh = (x + p->width) / (float) BLOCK_WIDTH;
		gl.tl = y / (float) BLOCK_WIDTH;
		gl.th = (y + p->height) / (float) BLOCK_WIDTH;
	}
	else
	{
		char texturename[64];
		snprintf(texturename, sizeof(texturename), "%s", name);

		gl.gltexture = TexMgr_LoadImage(texturename, p->width, p->height, SRC_INDEXED, p->data, TEX_ALPHA | TEX_PAD | TEX_NOPICMIP);
		gl.sl = 0;
		gl.sh = (float) p->width / (float) TexMgr_PadConditional(p->width);
		gl.tl = 0;
		gl.th = (float) p->height / (float) TexMgr_PadConditional(p->height);
	}

	memcpy(p->data, &gl, sizeof(glpic_t));

	return p;
}

qpic_t *Draw_CachePic(char *path)
{
	cachepic_t *pic;
	int i;
	qpic_t *dat;
	glpic_t *gl;
	static cachepic_t menu_cachepics[MAX_CACHED_PICS];
	static int menu_numcachepics;

	for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
		if (!strcmp(path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy(pic->name, path);

	// load the pic from disk
	dat = (qpic_t *) COM_LoadTempFile(path);
	if (!dat)
		Sys_Error("failed to load %s", path);
	SwapPic(dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp(path, "gfx/menuplyr.lmp"))
		memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *) pic->pic.data;
	gl->gltexture = GL_LoadPicTexture(dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

static void OnChange_gl_smoothfont(struct cvar_s *cvar)
{
	GL_Bind(char_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, cvar->value ? GL_LINEAR : GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, cvar->value ? GL_LINEAR : GL_NEAREST);
}

/*
 ================
 Draw_Character

 Draws one 8*8 graphics character with 0 being transparent.
 It can be clipped to the top of the screen to allow the console to be
 smoothly scrolled off.
 ================
 */

static bool IsValid(int y, int num)
{
	if ((num & 127) == 32)
		return false; // space

	if (y <= -8)
		return false; // totally off screen

	return true;
}

static void Character(int x, int y, int num)
{
	num &= 255;
	int row = num >> 4;
	int col = num & 15;

	float frow = row * 0.0625;
	float fcol = col * 0.0625;
	float size = 0.0625;
//	float offset = 0.002; // slight offset to avoid in-between lines distortion
	float offset = 0.03125; // offset to match expanded charset texture

	GLfloat texts[] = {
		fcol,        frow,
		fcol + size, frow,
		fcol + size, frow + size - offset,
		fcol,        frow + size - offset,
	};

	GLfloat verts[] = {
		x,     y,
		x + 8, y,
		x + 8, y + 8,
		x,     y + 8,
	};

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Draw_Character(int x, int y, int num)
{
	if (!IsValid(y, num))
		return;

	GL_Bind(char_texture);

	Character(x, y, num);
}

void Draw_String(int x, int y, const char *str)
{
	GL_Bind(char_texture);

	while (*str)
	{
		if (IsValid(y, *str))
			Character(x, y, *str);

		str++;
		x += 8;
	}
}

void Draw_Crosshair(void)
{
	float x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value;
	float y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;

	if (crosshair.value >= 2 && (crosshair.value <= NUMCROSSHAIRS + 1))
		Draw_Pic(x, y, crosshair_pics[(int)crosshair.value - 2], gl_crosshairalpha.value);
	else if (crosshair.value)
	{
		if (cl_crosshaircentered.value) // Centered crosshair
			Draw_Character(x - 4, y - 4, '+');
		else // Standard off-center Quake crosshair
			Draw_Character(x, y, '+');
	}

}

void Draw_Pic(int x, int y, qpic_t *pic, float alpha)
{
	if (scrap_dirty)
		Scrap_Upload();

	alpha = CLAMP(0, alpha, 1.0f);

	if (alpha < 1.0f)
	{
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
	}
	glColor4f(1.0f, 1.0f, 1.0f, alpha);

	glpic_t *gl = (glpic_t *) pic->data;
	GL_Bind(gl->gltexture);

	GLfloat texts[] = {
		gl->sl, gl->tl,
		gl->sh, gl->tl,
		gl->sh, gl->th,
		gl->sl, gl->th,
	};

	GLfloat verts[] = {
		x,              y,
		x + pic->width, y,
		x + pic->width, y + pic->height,
		x,              y + pic->height,
	};

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	if (alpha < 1.0f)
	{
		glEnable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}
}

void Draw_TransPic(int x, int y, qpic_t *pic)
{
	if (x < 0 ||
	    y < 0 ||
	    (unsigned)(x + pic->width) > vid.width ||
	    (unsigned) (y + pic->height) > vid.height)
		Sys_Error("bad coordinates");

	Draw_Pic(x, y, pic, 1.0f);
}

/* Only used for the player color selection menu */
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation)
{
	int v, u;
	unsigned trans[64 * 64], *dest;
	byte *src;
	int p;

//	GL_Bind(translate_texture);

	dest = trans;
	for (v = 0; v < 64; v++, dest += 64)
	{
		src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
		for (u = 0; u < 64; u++)
		{
			p = src[(u * pic->width) >> 6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] = d_8to24table[translation[p]];
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLfloat texts[] = {
		0, 0,
		1, 0,
		1, 1,
		0, 1,
	};

	GLfloat verts[] = {
		x,              y,
		x + pic->width, y,
		x + pic->width, y + pic->height,
		x,              y + pic->height,
	};

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Draw_ConsoleBackground()
{
	qpic_t *pic = Draw_CachePic ("gfx/conback.lmp");
	pic->width = vid.conwidth;
	pic->height = vid.conheight;

	float alpha = (con_forcedup) ? 1.0 : scr_conalpha.value;

	GL_SetCanvas (CANVAS_CONSOLE); //in case this is called from weird places

	Draw_Pic(0, 0, pic, alpha);
}

/*
 * This repeats a 64*64 tile graphic to fill the screen around a sized down
 * refresh window
 */
void Draw_TileClear(int x, int y, int w, int h)
{
	glpic_t	*gl = (glpic_t *)draw_backtile->data;
	GL_Bind (gl->gltexture);

	GLfloat texts[] = {
		x / 64.0, y / 64.0,
		(x + w) / 64.0, y / 64.0,
		(x + w) / 64.0, (y + h) / 64.0,
		x / 64.0, (y + h) / 64.0,
	};

	GLfloat verts[] = {
		x,     y,
		x + w, y,
		x + w, y + h,
		x,     y + h,
	};

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* Fills a box of pixels with a single color */
void Draw_Fill(int x, int y, int w, int h, int c, float alpha)
{
	byte *pal = (byte *)d_8to24table;
	alpha = CLAMP(0, alpha, 1.0f);

	glDisable(GL_TEXTURE_2D);
	if (alpha < 1.0f)
	{
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
	}
	glColor4f(pal[c * 4    ] / 255.0,
		  pal[c * 4 + 1] / 255.0,
		  pal[c * 4 + 2] / 255.0,
		  alpha);

	GLfloat verts[] = {
		x,     y,
		x + w, y,
		x + w, y + h,
		x,     y + h,
	};

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	if (alpha < 1.0f)
	{
		glEnable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}
	glEnable(GL_TEXTURE_2D);
}

void Draw_FadeScreen(void)
{
	extern cvar_t gl_fadescreen_alpha;

	GL_SetCanvas(CANVAS_DEFAULT);

	Draw_Fill(0, 0, vid.width, vid.height, 0, gl_fadescreen_alpha.value);
	Draw_Fill(0, 0, glwidth, glheight, 0, gl_fadescreen_alpha.value);

	Sbar_Changed();
}

//=============================================================================

static void Q_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar)
{
	GLfloat matrix[] = {
		2.0f/(right-left), 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f/(top-bottom), 0.0f, 0.0f,
		0.0f, 0.0f, -2.0f/(zFar-zNear), 0.0f,
		-(right+left)/(right-left), -(top+bottom)/(top-bottom), -(zFar+zNear)/(zFar-zNear), 1.0f,
	};

	glMultMatrixf(matrix);
}

void GL_SetCanvas(canvastype newcanvas)
{
	extern vrect_t scr_vrect;
	float s;
	int lines;

	if (newcanvas == currentcanvas)
		return;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	switch (newcanvas)
	{
	case CANVAS_DEFAULT:
		Q_glOrthof(0, glwidth, glheight, 0, -99999, 99999);
		glViewport(glx, gly, glwidth, glheight);
		break;
	case CANVAS_CONSOLE:
		lines = vid.conheight - (scr_con_current * vid.conheight / glheight);
		Q_glOrthof(0, vid.conwidth, vid.conheight + lines, lines, -99999, 99999);
		glViewport(glx, gly, glwidth, glheight);
		break;
	case CANVAS_MENU:
		s = min((float )glwidth / 320.0, (float )glheight / 200.0);
		s = CLAMP(1.0, scr_menuscale.value, s);
		// ericw -- doubled width to 640 to accommodate long keybindings
		Q_glOrthof(0, 640, 200, 0, -99999, 99999);
		glViewport(glx + (glwidth - 320 * s) / 2, gly + (glheight - 200 * s) / 2, 640 * s, 200 * s);
		break;
	case CANVAS_SBAR:
		s = CLAMP(1.0, scr_sbarscale.value, (float )glwidth / 320.0);
		if (cl.gametype == GAME_DEATHMATCH)
		{
			Q_glOrthof(0, glwidth / s, 48, 0, -99999, 99999);
			glViewport(glx, gly, glwidth, 48 * s);
		}
		else
		{
			Q_glOrthof(0, 320, 48, 0, -99999, 99999);
			glViewport(glx + (glwidth - 320 * s) / 2, gly, 320 * s, 48 * s);
		}
		break;
//	case CANVAS_WARPIMAGE:
//		glOrtho (0, 128, 0, 128, -99999, 99999);
//		glViewport (glx, gly+glheight-gl_warpimagesize, gl_warpimagesize, gl_warpimagesize);
//		break;
	case CANVAS_CROSSHAIR: //0,0 is center of viewport
		s = CLAMP(1.0, scr_crosshairscale.value, 10.0);
		Q_glOrthof(scr_vrect.width / -2 / s, scr_vrect.width / 2 / s, scr_vrect.height / 2 / s, scr_vrect.height / -2 / s, -99999, 99999);
		glViewport(scr_vrect.x, glheight - scr_vrect.y - scr_vrect.height, scr_vrect.width & ~1, scr_vrect.height & ~1);
		break;
	case CANVAS_BOTTOMLEFT: //used by devstats
		s = (float) glwidth / vid.conwidth; //use console scale
		Q_glOrthof(0, 320, 200, 0, -99999, 99999);
		glViewport(glx, gly, 320 * s, 200 * s);
		break;
	case CANVAS_BOTTOMRIGHT: //used by fps/clock
		s = (float) glwidth / vid.conwidth; //use console scale
		Q_glOrthof(0, 320, 200, 0, -99999, 99999);
		glViewport(glx + glwidth - 320 * s, gly, 320 * s, 200 * s);
		break;
	case CANVAS_TOPRIGHT: //used by disc
		s = 1;
		Q_glOrthof(0, 320, 200, 0, -99999, 99999);
		glViewport(glx + glwidth - 320 * s, gly + glheight - 200 * s, 320 * s, 200 * s);
		break;
	default:
		Sys_Error("GL_SetCanvas: bad canvas type");
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	currentcanvas = newcanvas;
}

void GL_Set2D(void)
{
	currentcanvas = CANVAS_INVALID;
	GL_SetCanvas(CANVAS_DEFAULT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glColor4f(1, 1, 1, 1);
}

static void Load_CharSet(void)
{
	draw_chars = (byte *) W_GetLumpName("conchars");
	for (int i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255; // proper transparent color

	// Expand charset texture with blank lines in between to avoid in-line distortion
	byte *dest = (byte *) Q_malloc(128 * 256);
	memset(dest, 0, 128 * 256);
	byte *src = draw_chars;

	for (int i = 0; i < 16; ++i)
		memcpy(&dest[8 * 128 * 2 * i], &src[8 * 128 * i], 8 * 128); // Copy each line

	// now turn them into textures
	char_texture = TexMgr_LoadImage("charset", 128, 256, SRC_INDEXED, dest, TEX_ALPHA | TEX_NOPICMIP);

	free(dest);
}

/* generate pics from internal data */
static qpic_t *Draw_MakePic(const char *name, int width, int height, byte *data)
{
	int flags = TEX_NEAREST | TEX_ALPHA | TEX_PERSIST | TEX_NOPICMIP | TEX_PAD;
	glpic_t gl;

	qpic_t *pic = (qpic_t *) Hunk_Alloc(sizeof(qpic_t) - 4 + sizeof(glpic_t));
	pic->width = width;
	pic->height = height;

	gl.gltexture = TexMgr_LoadImage(name, width, height, SRC_INDEXED, data, flags);
	gl.sl = 0;
	gl.sh = (float) width / (float) TexMgr_PadConditional(width);
	gl.tl = 0;
	gl.th = (float) height / (float) TexMgr_PadConditional(height);
	memcpy(pic->data, &gl, sizeof(glpic_t));

	return pic;
}

void Draw_Init(void)
{
	int i;

	Cvar_RegisterVariable(&crosshair);
	Cvar_RegisterVariable(&cl_crossx);
	Cvar_RegisterVariable(&cl_crossy);
	Cvar_RegisterVariable(&cl_crosshaircentered);
	Cvar_RegisterVariable(&gl_crosshairalpha);
	Cvar_RegisterVariable(&crosshairsize);
	Cvar_RegisterVariable(&scr_conalpha);

	Cvar_RegisterVariable(&gl_smoothfont);
	Cvar_SetCallback(&gl_smoothfont, OnChange_gl_smoothfont);

	Load_CharSet();

	// Load the crosshair pics
	for (i = 0; i < NUMCROSSHAIRS; i++)
	{
		char name[11];
		sprintf(name, "crosshair%i", i);
		crosshair_pics[i] = Draw_MakePic(name, 8, 8, crosshairdata[i]);
	}

	// load game pics
	draw_backtile = Draw_PicFromWad("backtile");
}
