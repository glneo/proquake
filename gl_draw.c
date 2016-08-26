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

//cvar_t		gl_nobind = {"gl_nobind", "0"};

int gl_max_size = 1024;

cvar_t gl_picmip = { "gl_picmip", "0", true };
cvar_t gl_crosshairalpha = { "crosshairalpha", "1", true };
cvar_t gl_texturemode = { "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", false }; // Let's not save to config

cvar_t gl_free_world_textures = { "gl_free_world_textures", "1", true }; //R00k

cvar_t crosshaircolor = { "crosshaircolor", "15", true };
cvar_t crosshairsize = { "crosshairsize", "1", true };

byte *draw_chars;				// 8*8 graphic characters
qpic_t *draw_disc;
qpic_t *draw_backtile;

int translate_texture;
int char_texture;

extern cvar_t crosshair, cl_crosshaircentered, cl_crossx, cl_crossy;

qpic_t crosshairpic;

typedef struct
{
	int texnum;
	float sl, tl, sh, th;
} glpic_t;

byte conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t *conback = (qpic_t *) &conback_buffer;

static int GL_LoadPicTexture(qpic_t *pic)
{
	return GL_LoadTexture("", pic->width, pic->height, pic->data, TEX_ALPHA);
}

#define NUMCROSSHAIRS	5
int crosshairtextures[NUMCROSSHAIRS];

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
#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte scrap_texels[MAX_SCRAPS][BLOCK_WIDTH * BLOCK_HEIGHT * 4];
bool scrap_dirty;
int scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;
	int texnum;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++)
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

	Sys_Error("Scrap_AllocBlock: full");
	return (0);
}

int scrap_uploads;

void Scrap_Upload(void)
{
	int texnum;

	scrap_uploads++;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		GL_Bind(scrap_texnum + texnum);
		GL_Upload8(scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, TEX_ALPHA);
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
cachepic_t menu_cachepics[MAX_CACHED_PICS];
int menu_numcachepics;

byte menuplyr_pixels[4096];

int pic_texels;
int pic_count;

qpic_t *Draw_PicFromWad(char *name)
{
	qpic_t *p;
	glpic_t *gl;

	p = W_GetLumpName(name);
	gl = (glpic_t *) p->data;

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
			for (j = 0; j < p->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x + 0.01) / (float) BLOCK_WIDTH;
		gl->sh = (x + p->width - 0.01) / (float) BLOCK_WIDTH;
		gl->tl = (y + 0.01) / (float) BLOCK_WIDTH;
		gl->th = (y + p->height - 0.01) / (float) BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width * p->height;
	}
	else
	{
		gl->texnum = GL_LoadPicTexture(p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}

qpic_t *Draw_CachePic(char *path)
{
	cachepic_t *pic;
	int i;
	qpic_t *dat;
	glpic_t *gl;

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
		Sys_Error("Draw_CachePic: failed to load %s", path);
	SwapPic(dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp(path, "gfx/menuplyr.lmp"))
		memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *) pic->pic.data;
	gl->texnum = GL_LoadPicTexture(dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

void Draw_CharToConback(int num, byte *dest)
{
	int row, col;
	byte *source;
	int drawline;
	int x;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row << 10) + (col << 3);

	drawline = 8;

	while (drawline--)
	{
		for (x = 0; x < 8; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

/*
 ===============
 Draw_LoadPics -- johnfitz
 ===============
 */
void Draw_LoadPics(void)
{
	draw_disc = Draw_PicFromWad("disc");
	draw_backtile = Draw_PicFromWad("backtile");
}

static bool smoothfont = 1;
bool smoothfont_init = false;

static void SetSmoothFont(void)
{
	smoothfont_init = true; // This is now available
	GL_Bind(char_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smoothfont ? GL_LINEAR : GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smoothfont ? GL_LINEAR : GL_NEAREST);
}

void SmoothFontSet(bool smoothfont_choice)
{
	smoothfont = smoothfont_choice;
	if (smoothfont_init)
		SetSmoothFont();
}

void Draw_SmoothFont_f(void)
{
	if (Cmd_Argc() == 1)
	{
		Con_Printf("gl_smoothfont is %d\n", smoothfont);
		return;
	}

	smoothfont = atoi(Cmd_Argv(1));
	SetSmoothFont();
}

static void Load_CharSet(void)
{
	int i;
	byte *dest, *src;

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName("conchars");
	for (i = 0; i < 256 * 64; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// Expand charset texture with blank lines in between to avoid in-line distortion
	dest = Q_malloc(128 * 256);
	memset(dest, 0, 128 * 256);
	src = draw_chars;

	for (i = 0; i < 16; ++i)
		memcpy(&dest[8 * 128 * 2 * i], &src[8 * 128 * i], 8 * 128); // Copy each line

	// now turn them into textures
	char_texture = GL_LoadTexture("charset", 128, 256, dest, TEX_ALPHA);

	free(dest);

	SetSmoothFont();
}

void Draw_InitConback_Old(void)
{
	qpic_t *cb;
	int start;
	byte *dest;
	char ver[40];
	glpic_t *gl;

	byte *ncdata;

	start = Hunk_LowMark();

	cb = (qpic_t *) COM_LoadTempFile("gfx/conback.lmp");
	if (!cb)
		Sys_Error("Couldn't load gfx/conback.lmp");
	SwapPic(cb);

	// hack the version number directly into the pic

	snprintf(ver, sizeof(ver), "(ProQuake) %4.2f", (float) PROQUAKE_SERIES_VERSION); // JPG - obvious change

	dest = cb->data + 320 * 186 + 320 - 11 - 8 * strlen(ver);
	for (int x = 0; x < strlen(ver); x++)
		Draw_CharToConback(ver[x], dest + (x << 3));

	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *) conback->data;
	gl->texnum = GL_LoadTexture("conback", conback->width, conback->height, ncdata, TEX_NOFLAGS);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);
}

void Draw_Init(void)
{
	int i;

//	Cvar_RegisterVariable (&gl_nobind, NULL);

	Cvar_RegisterVariable(&gl_picmip, NULL);
	Cvar_RegisterVariable(&gl_crosshairalpha, NULL);
	Cvar_RegisterVariable(&crosshaircolor, NULL);

	Cvar_RegisterVariable(&crosshairsize, NULL);

	Cvar_RegisterVariable(&gl_free_world_textures, NULL); //R00k

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_size);

	Cvar_RegisterVariable(&gl_texturemode, &OnChange_gl_texturemode);
	Cmd_AddCommand("gl_smoothfont", Draw_SmoothFont_f);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture

	Load_CharSet();

	Draw_InitConback_Old();

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;

	// Load the crosshair pics
	for (i = 0; i < NUMCROSSHAIRS; i++)
	{
		crosshairtextures[i] = GL_LoadTexture("", 8, 8, crosshairdata[i], TEX_ALPHA);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// load game pics
	Draw_LoadPics();

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
	float frow, fcol, size, offset;

	num &= 255;

	int row = num >> 4;
	int col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;
//	offset = 0.002; // slight offset to avoid in-between lines distortion
	offset = 0.03125; // offset to match expanded charset texture

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

void Draw_String(int x, int y, char *str)
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

byte *StringToRGB(char *s)
{
	byte *col;
	static byte rgb[4];

	Cmd_TokenizeString(s);
	if (Cmd_Argc() == 3)
	{
		rgb[0] = (byte) atoi(Cmd_Argv(0));
		rgb[1] = (byte) atoi(Cmd_Argv(1));
		rgb[2] = (byte) atoi(Cmd_Argv(2));
	}
	else
	{
		col = (byte *) &d_8to24table[(byte) atoi(s)];
		rgb[0] = col[0];
		rgb[1] = col[1];
		rgb[2] = col[2];
	}
	rgb[3] = 255;

	return rgb;
}

void Draw_Crosshair(void)
{
	float x, y, ofs1, ofs2, sh, th, sl, tl;
	byte *col;
	extern vrect_t scr_vrect;

	if (crosshair.value >= 2 && (crosshair.value <= NUMCROSSHAIRS + 1))
	{
		x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value;
		y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;

		if (!gl_crosshairalpha.value)
			return;

		glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		col = StringToRGB(crosshaircolor.string);

		if (gl_crosshairalpha.value)
		{
			glDisable(GL_ALPHA_TEST);
			glEnable(GL_BLEND);
			col[3] = CLAMP (0, gl_crosshairalpha.value, 1) * 255;
			glColor4ub(col[0], col[1], col[2], col[3]);
		}
		else
		{
			glColor4ub(col[0], col[1], col[2], 255);
		}

		GL_Bind(crosshairtextures[(int) crosshair.value - 2]);
		ofs1 = 3.5;
		ofs2 = 4.5;
		tl = sl = 0;
		sh = th = 1;

		ofs1 *= (vid.width / 320) * CLAMP(0, crosshairsize.value, 20);
		ofs2 *= (vid.width / 320) * CLAMP(0, crosshairsize.value, 20);

		GLfloat texts[] = {
			sl, tl,
			sh, tl,
			sh, th,
			sl, th,
		};

		GLfloat verts[] = {
			x - ofs1, y - ofs1,
			x + ofs2, y - ofs1,
			x + ofs2, y + ofs2,
			x - ofs1, y + ofs2,
		};

		glTexCoordPointer(2, GL_FLOAT, 0, texts);
		glVertexPointer(2, GL_FLOAT, 0, verts);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		if (gl_crosshairalpha.value)
		{
			glDisable(GL_BLEND);
			glEnable(GL_ALPHA_TEST);
		}

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	else if (crosshair.value)
	{
		if (!cl_crosshaircentered.value)
		{
			// Standard off-center Quake crosshair
			Draw_Character(scr_vrect.x + (scr_vrect.width / 2) + cl_crossx.value, scr_vrect.y + (scr_vrect.height / 2) + cl_crossy.value, '+');
		}
		else
		{
			// Baker 3.60 - Centered crosshair (FuhQuake)
			Draw_Character(scr_vrect.x + ((scr_vrect.width / 2) - 4) + cl_crossx.value, scr_vrect.y + ((scr_vrect.height / 2) - 4) + cl_crossy.value, '+');
		}
	}

}

void Draw_AlphaPic(int x, int y, qpic_t *pic, float alpha)
{
	glpic_t *gl;

	if (scrap_dirty)
		Scrap_Upload();

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glCullFace(GL_FRONT);

	gl = (glpic_t *) pic->data;
	GL_Bind(gl->texnum);

	glColor4f(1.0f, 1.0f, 1.0f, alpha);

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

	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
}

void Draw_Pic(int x, int y, qpic_t *pic)
{
	glpic_t *gl;

	if (scrap_dirty)
		Scrap_Upload();

	gl = (glpic_t *) pic->data;
	GL_Bind(gl->texnum);

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
}

void Draw_TransPic(int x, int y, qpic_t *pic)
{
	if (x < 0 || (unsigned) (x + pic->width) > vid.width || y < 0 || (unsigned) (y + pic->height) > vid.height)
		Sys_Error("Draw_TransPic: bad coordinates");

	Draw_Pic(x, y, pic);
}

void Draw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
{
	glpic_t *gl;
	float newsl, newtl, newsh, newth;
	float oldglwidth, oldglheight;

	if (scrap_dirty)
		Scrap_Upload();
	gl = (glpic_t *) pic->data;

	oldglwidth = gl->sh - gl->sl;
	oldglheight = gl->th - gl->tl;

	newsl = gl->sl + (srcx * oldglwidth) / pic->width;
	newsh = newsl + (width * oldglwidth) / pic->width;

	newtl = gl->tl + (srcy * oldglheight) / pic->height;
	newth = newtl + (height * oldglheight) / pic->height;

	GL_Bind(gl->texnum);

	GLfloat texts[] = {
		newsl, newtl,
		newsh, newtl,
		newsh, newth,
		newsl, newth,
	};

	GLfloat verts[] = {
		x,         y,
		x + width, y,
		x + width, y + height,
		x,         y + height,
	};

	glTexCoordPointer(2, GL_FLOAT, 0, texts);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/*
 =============
 Draw_TransPicTranslate

 Only used for the player color selection menu
 =============
 */
void Draw_TransPicTranslate(int x, int y, qpic_t *pic, byte *translation)
{
	int v, u, c;
	unsigned trans[64 * 64], *dest;
	byte *src;
	int p;

	GL_Bind(translate_texture);

	c = pic->width * pic->height;

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

void Draw_ConsoleBackground(int lines)
{
	int y = (vid.height * 3) >> 2;

	if (lines > y)
		Draw_Pic(0, lines - vid.height, conback);
	else
		Draw_AlphaPic(0, lines - vid.height, conback, (float) (1.2 * lines) / y);
}

/*
 =============
 Draw_TileClear

 This repeats a 64*64 tile graphic to fill the screen around a sized down
 refresh window.
 =============
 */
void Draw_TileClear(int x, int y, int w, int h)
{
	GL_Bind(*(int *) draw_backtile->data);

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

/*
 =============
 Draw_AlphaFill

 Fills a box of pixels with a single color
 =============
 */
void Draw_AlphaFill(int x, int y, int w, int h, int c, float alpha)
{
	alpha = CLAMP(0, alpha, 1.0f);

	if (!alpha)
		return;

	glDisable(GL_TEXTURE_2D);
	if (alpha < 1.0f)
	{
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
	}
	glColor4f(host_basepal[c * 3] / 255.0, host_basepal[c * 3 + 1] / 255.0, host_basepal[c * 3 + 2] / 255.0, alpha);

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

	glEnable(GL_TEXTURE_2D);
	if (alpha < 1)
	{
		glEnable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
	}
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/*
 =============
 Draw_Fill

 Fills a box of pixels with a single color
 =============
 */
void Draw_Fill(int x, int y, int w, int h, int c)
{
	glDisable(GL_TEXTURE_2D);
	glColor4f(host_basepal[c * 3] / 255.0, host_basepal[c * 3 + 1] / 255.0, host_basepal[c * 3 + 2] / 255.0, 1.0f);

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
	glEnable(GL_TEXTURE_2D);
}
//=============================================================================

void Draw_FadeScreen(void)
{
	extern cvar_t gl_fadescreen_alpha;

	glEnable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glColor4f(0, 0, 0, gl_fadescreen_alpha.value);

	GLfloat verts[] = {
		0, 0,
		vid.width, 0,
		vid.width, vid.height,
		0, vid.height,
	};

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	Sbar_Changed();
}

//=============================================================================

/*
 ================
 Draw_BeginDisc

 Draws the little blue disc in the corner of the screen.
 Call before beginning any disc IO.
 ================
 */
void Draw_BeginDisc(void)
{
	if (!draw_disc)
		return;

	//if (mod_conhide==true && (key_dest != key_console && key_dest != key_message)) {
	if (key_dest != key_console && key_dest != key_message)
	{
		// No draw this either
		return;
	}

//	glDrawBuffer(GL_FRONT);
	Draw_Pic(vid.width - 24, 0, draw_disc);
//	glDrawBuffer(GL_BACK);
}

/*
 ================
 Draw_EndDisc

 Erases the disc icon.
 Call after completing any disc IO
 ================
 */
void Draw_EndDisc(void)
{
}


void Q_glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar)
{
	GLfloat matrix[] = {
		2.0f/(right-left), 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f/(top-bottom), 0.0f, 0.0f,
		0.0f, 0.0f, -2.0f/(zFar-zNear), 0.0f,
		-(right+left)/(right-left), -(top+bottom)/(top-bottom), -(zFar+zNear)/(zFar-zNear), 1.0f,
	};

	glMultMatrixf(matrix);
}

/*
 ================
 GL_Set2D

 Setup as if the screen was 320*200
 ================
 */
void GL_Set2D(void)
{
	glViewport(glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	Q_glOrthof(0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
}
