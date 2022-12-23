/*
 * Character and picture rendering
 *
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

#include "draw.fs.h"
#include "draw.vs.h"

static gltexture_t *char_texture;
static gltexture_t *solidtexture;

// shader program
static GLuint draw_program;

// uniforms used in vertex shader
static GLuint draw_projectionLoc;

// uniforms used in fragment shader
static GLuint draw_texLoc;
static GLuint draw_colorLoc;

// attribs
static GLuint draw_vertexAttrIndex;
static GLuint draw_texCoordsAttrIndex;

// VBOs
GLuint draw_vertex_VBO;
GLuint draw_texCoords_VBO;

qpic_t crosshairpic;

canvastype currentcanvas = CANVAS_NONE; //johnfitz -- for GL_SetCanvas

typedef struct draw_vertex_s {
       vec2_t position;
       vec2_t textureCord;
} draw_vertex_t;

std::vector<draw_vertex_t> draw_buffer;

/* Allocate all the little status bar objects into a single texture */

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
	dqpic_t *p = (dqpic_t *)W_GetLumpName(name);
	qpic_t *gl = (qpic_t *)Q_malloc(sizeof(qpic_t));

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int x = 0;
		int y = 0;
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
		gl->sh = 1.0f;
		gl->tl = 0;
		gl->th = 1.0f;
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
	float offset = size;

	alpha = CLAMP(0.0f, alpha, 1.0f);

	//                      position                          texture
	draw_buffer.push_back( {{(GLfloat)x,     (GLfloat)y,},    {fcol,          frow,}         });
	draw_buffer.push_back( {{(GLfloat)x + 8, (GLfloat)y,},    {fcol + offset, frow,}         });
	draw_buffer.push_back( {{(GLfloat)x + 8, (GLfloat)y + 8}, {fcol + offset, frow + offset,}});
	draw_buffer.push_back( {{(GLfloat)x,     (GLfloat)y,},    {fcol,          frow,}         });
	draw_buffer.push_back( {{(GLfloat)x + 8, (GLfloat)y + 8}, {fcol + offset, frow + offset,}});
	draw_buffer.push_back( {{(GLfloat)x,     (GLfloat)y + 8}, {fcol,          frow + offset,}});
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

	GL_BindToUnit(GL_TEXTURE0, char_texture);

	Character(x, y, num, alpha);
}

void Draw_String(int x, int y, const char *str, float alpha)
{
	GL_BindToUnit(GL_TEXTURE0, char_texture);

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

	// setup
	GL_BindToUnit(GL_TEXTURE0, pic->gltexture);

	// set uniforms
	alpha = CLAMP(0.0f, alpha, 1.0f);
	glUniform4f(draw_colorLoc, 1.0f, 1.0f, 1.0f, alpha);

	// set attributes
	glBindBuffer(GL_ARRAY_BUFFER, draw_vertex_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &verts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(draw_vertexAttrIndex);
	glVertexAttribPointer(draw_vertexAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, draw_texCoords_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &texts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(draw_texCoordsAttrIndex);
	glVertexAttribPointer(draw_texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// cleanup
	glDisableVertexAttribArray(draw_texCoordsAttrIndex);
	glDisableVertexAttribArray(draw_vertexAttrIndex);
	glUniform4f(draw_colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
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

void Draw_TransPicTranslate(int x, int y, qpic_t *pic, float alpha, int top, int bottom)
{
	static int oldtop = -1;
	static int oldbottom = -1;

	if (top != oldtop || bottom != oldbottom)
	{
		oldtop = top;
		oldbottom = bottom;
		TexMgr_ReloadImage (pic->gltexture, top, bottom);
	}
	Draw_Pic(x, y, pic, alpha);
}

/* Tile graphic to fill the screen */
void Draw_PicTile(int x, int y, int w, int h, qpic_t *pic, float alpha)
{
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

	alpha = CLAMP(0.0f, alpha, 1.0f);

	// setup
	GL_BindToUnit(GL_TEXTURE0, pic->gltexture);

	// set uniforms
	glUniform4f(draw_colorLoc, 1.0f, 1.0f, 1.0f, alpha);

	// set attributes
	glBindBuffer(GL_ARRAY_BUFFER, draw_vertex_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &verts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(draw_vertexAttrIndex);
	glVertexAttribPointer(draw_vertexAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, draw_texCoords_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &texts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(draw_texCoordsAttrIndex);
	glVertexAttribPointer(draw_texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// cleanup
	glDisableVertexAttribArray(draw_texCoordsAttrIndex);
	glDisableVertexAttribArray(draw_vertexAttrIndex);
	glUniform4f(draw_colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Fills a box of pixels with a single color */
static void Draw_Solid(int x, int y, int w, int h, float r, float g, float b, float alpha)
{
	GLfloat verts[] = {
		(GLfloat)x,     (GLfloat)y,
		(GLfloat)x + w, (GLfloat)y,
		(GLfloat)x + w, (GLfloat)y + h,
		(GLfloat)x,     (GLfloat)y + h,
	};

	GLfloat texts[] = {
		(GLfloat)0.0f, (GLfloat)0.0f,
		(GLfloat)1.0f, (GLfloat)0.0f,
		(GLfloat)1.0f, (GLfloat)1.0f,
		(GLfloat)0.0f, (GLfloat)1.0f,
	};

	// set uniforms
	GL_BindToUnit(GL_TEXTURE0, solidtexture);
	glUniform4f(draw_colorLoc, r, g, b, alpha);

	// set attributes
	glBindBuffer(GL_ARRAY_BUFFER, draw_vertex_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &verts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(draw_vertexAttrIndex);
	glVertexAttribPointer(draw_vertexAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, draw_texCoords_VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 * 4, &texts[0], GL_STREAM_DRAW);
	glEnableVertexAttribArray(draw_texCoordsAttrIndex);
	glVertexAttribPointer(draw_texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// draw
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// cleanup
	glDisableVertexAttribArray(draw_texCoordsAttrIndex);
	glDisableVertexAttribArray(draw_vertexAttrIndex);
	glUniform4f(draw_colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Fills a box of pixels with a single color */
void Draw_Fill(int x, int y, int w, int h, int c, float alpha)
{
	float red   = (((byte *)&d_8to24table[c])[0]) / 255.0f;
	float green = (((byte *)&d_8to24table[c])[1]) / 255.0f;
	float blue  = (((byte *)&d_8to24table[c])[2]) / 255.0f;

	Draw_Solid(x, y, w, h, red, green, blue, alpha);
}

void Draw_PolyBlend(void)
{
	if (!v_blend[3]) // No blends ... get outta here
		return;

	if (!gl_polyblend.value)
		return;

	Draw_SetCanvas(CANVAS_DEFAULT);

	Draw_Solid(0, 0, vid.width, vid.height, v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
}

static void Draw_FlushState(void)
{
	if (draw_buffer.empty())
		return;

	// setup
        GL_Bind(char_texture);

        // set attributes
        glBindBuffer(GL_ARRAY_BUFFER, draw_vertex_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(draw_vertex_t) * draw_buffer.size(), draw_buffer.data(), GL_STREAM_DRAW);
        glEnableVertexAttribArray(draw_vertexAttrIndex);
        glVertexAttribPointer(draw_vertexAttrIndex, 2, GL_FLOAT, GL_FALSE, sizeof(draw_vertex_t), BUFFER_OFFSET(offsetof(draw_vertex_t, position)));
        glEnableVertexAttribArray(draw_texCoordsAttrIndex);
        glVertexAttribPointer(draw_texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, sizeof(draw_vertex_t), BUFFER_OFFSET(offsetof(draw_vertex_t, textureCord)));

        // draw
        glDrawArrays(GL_TRIANGLES, 0, draw_buffer.size());
        draw_buffer.clear();

	// cleanup
	glDisableVertexAttribArray(draw_texCoordsAttrIndex);
	glDisableVertexAttribArray(draw_vertexAttrIndex);
}

void Draw_SetCanvas(canvastype newcanvas)
{
	float s;
	int lines, x, y, w, h;

	if (newcanvas == currentcanvas)
		return;

	Draw_FlushState();

	projectionMatrix.identity();

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
		s = min((float )vid.width / 320.0f, (float )vid.height / 200.0f);
		s = CLAMP(1.0f, scr_menuscale.value, s);
		// ericw -- doubled width to 640 to accommodate long keybindings
		projectionMatrix.ortho(0, 640, 200, 0, -1.0f, 1.0f);
		glViewport(vid.x + (vid.width - 320 * s) / 2, vid.y + (vid.height - 200 * s) / 2, 640 * s, 200 * s);
		break;
	case CANVAS_SBAR:
		s = CLAMP(1.0f, scr_sbarscale.value, (float )vid.width / 320.0f);
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
	case CANVAS_CROSSHAIR: //0,0 is center of viewport
		s = CLAMP(1.0f, scr_crosshairscale.value, 10.0f);
		x = vid.x + r_refdef.vrect.x;
		y = vid.y + r_refdef.vrect.y;
		w = r_refdef.vrect.width;
		h = r_refdef.vrect.height;
		projectionMatrix.ortho(-(w/(2*s)), (w/(2*s)), (h/(2*s)), -(h/(2*s)), -1.0f, 1.0f);
		glViewport(x, y, w, h);
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

	glUniformMatrix4fv(draw_projectionLoc, 1, GL_FALSE, projectionMatrix.get());

	currentcanvas = newcanvas;
}

void GL_Begin2D(void)
{
	glUseProgram(draw_program);

	// set uniforms
        glUniform1i(draw_texLoc, 0);
        glUniform4f(draw_colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
	currentcanvas = CANVAS_INVALID;
	Draw_SetCanvas(CANVAS_DEFAULT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
}

void GL_End2D(void)
{
	Draw_FlushState();
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

/* generate pics from internal data */
qpic_t *Draw_MakePic(const char *name, int width, int height, byte *data)
{
	int flags = TEX_NEAREST | TEX_ALPHA | TEX_NOPICMIP | TEX_PAD;

	qpic_t *pic = (qpic_t *)Q_malloc(sizeof(qpic_t));

	pic->gltexture = TexMgr_LoadImage(name, width, height, SRC_INDEXED, data, flags);
	pic->width = width;
	pic->height = height;
	pic->sl = 0;
	pic->sh = 1.0f;
	pic->tl = 0;
	pic->th = 1.0f;

	return pic;
}

static void GL_LoadCharSet(void)
{
	byte *draw_chars = (byte *) W_GetLumpName("conchars");
	for (int i = 0; i < 128 * 128; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255; // proper transparent color

	// now turn them into textures
	char_texture = TexMgr_LoadImage("charset", 128, 128, SRC_INDEXED, draw_chars, TEX_NEAREST | TEX_ALPHA | TEX_NOPICMIP);
}

static void GL_CreateDrawShaders(void)
{
	// generate program
	draw_program = LoadShader((const char *)draw_vs, draw_vs_len,
	                          (const char *)draw_fs, draw_fs_len);

	// get attribute locations
	draw_vertexAttrIndex = GL_GetAttribLocation(draw_program, "Vertex");
	draw_texCoordsAttrIndex = GL_GetAttribLocation(draw_program, "TexCoords");

	// get uniform locations
	draw_texLoc = GL_GetUniformLocation(draw_program, "Tex");
	draw_projectionLoc = GL_GetUniformLocation(draw_program, "Projection");
	draw_colorLoc = GL_GetUniformLocation(draw_program, "Color");
}

void Draw_Init(void)
{
	GL_LoadCharSet();

	// Hack to get around switching to shader that does not sample textures
	static byte data[4] = {255, 255, 255, 255};
	solidtexture = TexMgr_LoadImage("solidtexture", 1, 1, SRC_RGBA, (byte *)data, TEX_ALPHA | TEX_NEAREST);

	GL_CreateDrawShaders();

	glGenBuffers(1, &draw_vertex_VBO);
	glGenBuffers(1, &draw_texCoords_VBO);
}
