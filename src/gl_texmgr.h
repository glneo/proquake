/*
 * OpenGL texture manager
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2007-2008 Kristian Duske
 * Copyright (C) 2010-2014 QuakeSpasm developers
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

#ifndef __GL_TEXMGR_H
#define __GL_TEXMGR_H

#define TEX_NOFLAGS     0 // 20 total
#define TEX_MIPMAP      BIT(0) // generate mipmaps	7
// TEX_NEAREST and TEX_LINEAR aren't supposed to be ORed with TEX_MIPMAP
#define TEX_LINEAR      BIT(1) // force linear		2
#define TEX_NEAREST     BIT(2) // force nearest		3
#define TEX_ALPHA       BIT(3) // allow alpha		9
#define TEX_PAD         BIT(4) // allow padding		3
//#define TEX_PERSIST     BIT(5) // never free		2
#define TEX_OVERWRITE   BIT(6) // overwrite existing same-name texture	1
#define TEX_NOPICMIP    BIT(7) // always load full-sized		8
#define TEX_FULLBRIGHT  BIT(8) // use fullbright mask palette
#define TEX_NOBRIGHT    BIT(9) // use nobright mask palette
#define TEX_CONCHARS    BIT(10) // use conchars palette
#define TEX_WARPIMAGE   BIT(11) // resize this texture when warpimagesize changes
#define TEX_WORLD       BIT(12) // world textures

enum srcformat
{
	SRC_INDEXED,
	SRC_LIGHTMAP,
	SRC_RGBA
};

typedef unsigned int GLenum;

typedef struct gltexture_s gltexture_t;

extern gltexture_t *notexture;
extern gltexture_t *nulltexture;

extern unsigned int d_8to24table[256];
extern unsigned int d_8to24table_fbright[256];

// TEXTURE BINDING
void GL_SelectTextureUnit(GLenum target);
void GL_Bind(gltexture_t *texture);
void GL_BindToUnit(GLenum target, gltexture_t *texture);

// TEXTURE MANAGER
void TexMgr_FreeTexture(gltexture_t *kill);
void TexMgr_Init(void);

// IMAGE LOADING
gltexture_t *TexMgr_LoadImage(const char *name, int width, int height, enum srcformat format, const byte *data, unsigned flags);
void TexMgr_ReloadImage(gltexture_t *glt, int shirt, int pants);
void TexMgr_ReloadImages(void);

#endif	/* __GL_TEXMGR_H */
