/*
 * Texture related functions and variables
 *
 * Copyright (C) 2001-2002       A Nourai
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

#ifndef __GL_TEXTURE_H
#define __GL_TEXTURE_H

#define	MAX_GLTEXTURES 1024

typedef struct
{
	unsigned int texnum;
	char identifier[MAX_QPATH];
	int width, height;
//	bool mipmap;
	unsigned short crc;  // Baker 3.80x - part of GL_LoadTexture: cache mismatch fix
	int texmode;	// Baker: 4.26 to all clearing of world textures
} gltexture_t;

// Engine internal vars
extern bool gl_mtexable;
extern int gl_max_size;

extern gltexture_t gltextures[MAX_GLTEXTURES];
extern int numgltextures;

extern int texture_extension_number;

extern texture_t *r_notexture_mip;
extern int d_lightstylevalue[256]; // 8.8 fraction of base light value

extern bool envmap;

extern int current_texture_num;
extern int particletexture;
extern int playertextures;

extern int skytexturenum; // index in cl.loadmodel, not gl texture object

extern int mirrortexturenum; // quake texturenum, not gltexturenum
extern bool mirror;
extern mplane_t *mirror_plane;

extern int texture_mode;
extern int gl_lightmap_format;

// Engine internal functions

void GL_Bind(int texnum);

void GL_SelectTexture(GLenum target);
void GL_DisableMultitexture(void);
void GL_EnableMultitexture(void);

void GL_Upload32(unsigned *data, int width, int height, int mode);
void GL_Upload8(byte *data, int width, int height, int mode);
int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode);
int GL_FindTexture(char *identifier);
int GL_LoadTexture32(char *identifier, int width, int height, byte *data, int mode);

void OnChange_gl_texturemode(struct cvar_s *cvar);

#endif /* __GL_TEXTURE_H */
