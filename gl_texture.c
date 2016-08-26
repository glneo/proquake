/*
 * Texture related functions
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

extern cvar_t gl_picmip;
extern cvar_t gl_texturemode;
extern cvar_t gl_free_world_textures;

int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

int texels;

gltexture_t gltextures[MAX_GLTEXTURES];
int numgltextures;

void GL_MipMap(byte *in, int width, int height)
{
	int i, j;
	byte *out;

	width <<= 2;
	height >>= 1;
	out = in;
	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[width + 0] + in[width + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[width + 1] + in[width + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[width + 2] + in[width + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[width + 3] + in[width + 7]) >> 2;
		}
	}
}

void GL_Upload32(unsigned *data, int width, int height, int mode)
{
	GLint samples;
	static unsigned scaled[1024 * 512];	// [512*256];
	int scaled_width, scaled_height;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
		;
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
		;

	scaled_width >>= (int) gl_picmip.value;
	scaled_height >>= (int) gl_picmip.value;

	if (scaled_width > gl_max_size)
		scaled_width = gl_max_size;
	if (scaled_height > gl_max_size)
		scaled_height = gl_max_size;

	if (scaled_width * scaled_height > sizeof(scaled) / 4)
		Sys_Error("GL_LoadTexture: too big");

	samples = (mode & TEX_ALPHA) ? GL_RGBA : GL_RGB;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!(mode & TEX_MIPMAP))
		{
			glTexImage2D(GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

			goto done;
		}
		memcpy(scaled, data, width * height * 4);
	}
	else
		GL_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);

	glTexImage2D(GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if ((mode & TEX_MIPMAP))
	{
		int miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap((byte *) scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;

			glTexImage2D(GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
	done:

	if ((mode & TEX_MIPMAP))
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

void GL_Upload8(byte *data, int width, int height, int mode)
{
	static unsigned trans[640 * 480];		// FIXME, temporary
	int i, s;
	bool noalpha;
	int p;

	s = width * height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (mode & TEX_ALPHA)
	{
		noalpha = true;
		for (i = 0; i < s; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (noalpha)
			mode = mode - TEX_ALPHA; // Baker: we know this bit flag exists due to IF, so just subtract it
	}
	else
	{
		if (s & 3)
			Sys_Error("GL_Upload8: s&3");
		for (i = 0; i < s; i += 4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i + 1] = d_8to24table[data[i + 1]];
			trans[i + 2] = d_8to24table[data[i + 2]];
			trans[i + 3] = d_8to24table[data[i + 3]];
		}
	}

	GL_Upload32(trans, width, height, mode);
}

int GL_FindTexture(char *identifier)
{
	int i;
	gltexture_t *glt;

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!strcmp(identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
 ================
 GL_ResampleTexture
 ================
 */
void GL_ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	int i, j;
	unsigned *inrow;
	unsigned frac, fracstep;

	fracstep = inwidth * 0x10000 / outwidth;
	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j += 4)
		{
			out[j] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 1] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 2] = inrow[frac >> 16];
			frac += fracstep;
			out[j + 3] = inrow[frac >> 16];
			frac += fracstep;
		}
	}
}

void GL_FreeTextures(void)
{
	int i, j;

	Con_DPrintf("GL_FreeTextures: Entry.\n");

	if (gl_free_world_textures.value == 0)
	{
		Con_DPrintf("GL_FreeTextures: Not Clearing old Map Textures.\n");
		return;
	}

	Con_DPrintf("GL_FreeTextures: Freeing textures (numgltextures = %i) \n", numgltextures);

	for (i = j = 0; i < numgltextures; ++i, ++j)
	{
		if (gltextures[i].texmode & TEX_WORLD) //Only clear out world textures... for now.
		{
			Con_DPrintf("GL_FreeTextures: Clearing texture %s\n", gltextures[i].identifier);
			glDeleteTextures(1, &gltextures[i].texnum);
			--j;
		}
		else if (j < i)
		{
//			Con_DPrintf("GL_FreeTextures: NOT Clearing texture %s\n", gltextures[i].identifier);
			gltextures[j] = gltextures[i];
		}
	}

	numgltextures = j;

	Con_DPrintf("GL_FreeTextures: Completed (numgltextures = %i) \n", numgltextures);

}

int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode)
{
	int i;
	gltexture_t *glt;
	unsigned short crc; // Baker 3.80x - LoadTexture fix LordHavoc provided by Reckless

	crc = CRC_Block(data, width * height); // Baker 3.80x - LoadTexture fix LordHavoc provided by Reckless

	// see if the texture is already present
	if (identifier[0])
	{
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
		{
			if (!strcmp(identifier, glt->identifier))
			{
				// Baker 3.60 - LoadTexture fix LordHavoc provided by Reckless
				if (width != glt->width || height != glt->height || crc != glt->crc)
					goto setup;
				return gltextures[i].texnum;
			}
		}
		// Jack Palevich -- surely we want to remember this new texture.
		// Doing this costs 1% fps per timedemo, probably because of the
		// linear search through the texture cache, but it saves 10 MB
		// of VM growth per level load. It also makes the GL_TEXTUREMODE
		// console command work correctly.
		// numgltextures++;  <---- Baker 3.70D3D - no implementing this yet, if at all

	}
//   else { get the hell rid of this :P
	glt = &gltextures[numgltextures++];
	//}

	strcpy(glt->identifier, identifier);
	glt->texnum = texture_extension_number++;

	setup: glt->width = width;
	glt->height = height;
	glt->texmode = mode;
	glt->crc = crc;

	// Baker: part 1 of gl dedicated server fix by Nathan Cline
	if (cls.state != ca_dedicated)
	{
		GL_Bind(glt->texnum);
		GL_Upload8(data, width, height, mode);
	}

	return glt->texnum;
}

/****************************************/

int current_texture_num = -1; // to avoid unnecessary texture sets
void GL_Bind(int texnum)
{
	if (current_texture_num == texnum)
		return;

	current_texture_num = texnum;
	glBindTexture(GL_TEXTURE_2D, texnum);
}

static GLenum currenttarget = GL_TEXTURE0;
bool mtexenabled = false;

void GL_SelectTexture(GLenum target)
{
	if (!gl_mtexable)
		return;

	if (target == currenttarget)
		return;

	glActiveTexture(target);

	cnttextures[currenttarget - GL_TEXTURE0] = current_texture_num;
	current_texture_num = cnttextures[target - GL_TEXTURE0];
	currenttarget = target;
}

void GL_DisableMultitexture(void)
{
	if (mtexenabled)
	{
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture(void)
{
	if (gl_mtexable)
	{
		GL_SelectTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

struct
{
	char *name;
	int minimize, maximize;
} modes[] = {
	{ "GL_NEAREST", GL_NEAREST, GL_NEAREST },
	{ "GL_LINEAR", GL_LINEAR, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR },
	{ "GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST },
	{ "GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
};

void OnChange_gl_texturemode(void)
{
	int i;
	gltexture_t *glt;
	static bool recursiveblock = false;

	if (recursiveblock)
		return;		// Get out

	for (i = 0; i < 6; i++)
	{
		char *str = gl_texturemode.string;
		if (!strcasecmp(modes[i].name, str))
			break;

		if (isdigit(*str) && atoi(str) - 1 == i)
		{
			// We have a number, set the cvar as the mode name
			recursiveblock = true; // Let's prevent this from occuring twice
			Cvar_Set("gl_texturemode", modes[i].name);
			recursiveblock = false;
			break;
		}
	}

	if (i == 6)
	{
		Con_Printf("bad filter name, available are:\n");
		for (i = 0; i < 6; i++)
			Con_Printf("%s (%d)\n", modes[i].name, i + 1);

		Cvar_SetValue("gl_texturemode", 1);
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->texmode & TEX_MIPMAP)
		{
			Con_DPrintf("Doing texture %s\n", glt->identifier);
			GL_Bind(glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

void R_InitTextures(void)
{
	int x, y, m;
	byte *dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName(sizeof(texture_t) + (16 * 16) + (8 * 8) + (4 * 4) + (2 * 2), "notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++)
	{
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++)
			{
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}

	R_Init_FlashBlend_Bubble();
}

static byte dottexture[8][8] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture(void)
{
	int x, y;
	byte data[8][8][4];

	//
	// particle texture
	//
	particletexture = texture_extension_number++;
	GL_Bind(particletexture);

	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
