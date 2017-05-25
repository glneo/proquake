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

#include "quakedef.h"
#include "glquake.h"

static cvar_t gl_texturemode = { "gl_texturemode", "", CVAR_ARCHIVE };
static cvar_t gl_texture_anisotropy = { "gl_texture_anisotropy", "1", CVAR_ARCHIVE };
static cvar_t gl_max_size = { "gl_max_size", "0", CVAR_NONE };
static cvar_t gl_picmip = { "gl_picmip", "0", CVAR_NONE };

static GLint gl_hardware_maxsize;

#define	MAX_GLTEXTURES	2048
static int numgltextures;
static gltexture_t *active_gltextures, *free_gltextures;
gltexture_t *notexture, *nulltexture;

unsigned int d_8to24table[256];
unsigned int d_8to24table_fbright[256];
unsigned int d_8to24table_fbright_fence[256];
unsigned int d_8to24table_nobright[256];
unsigned int d_8to24table_nobright_fence[256];
unsigned int d_8to24table_conchars[256];
unsigned int d_8to24table_shirt[256];
unsigned int d_8to24table_pants[256];

// current texture in each texture unit cache
static GLuint currenttexture[2] = { GL_UNUSED_TEXTURE, GL_UNUSED_TEXTURE };
static GLenum currenttarget = GL_TEXTURE0;
bool mtexenabled = false;

void GL_Bind(gltexture_t *texture)
{
	if (!texture)
		texture = nulltexture;

	if (texture->texnum != currenttexture[currenttarget - GL_TEXTURE0])
	{
		currenttexture[currenttarget - GL_TEXTURE0] = texture->texnum;
		glBindTexture(GL_TEXTURE_2D, texture->texnum);
		texture->visframe = r_framecount;
	}
}

static void GL_SelectTextureUnit(GLenum target)
{
	if (target == currenttarget)
		return;

	glActiveTexture(target);

	currenttarget = target;
}

/* selects texture unit 1 */
void GL_EnableMultitexture(void)
{
	if (!mtexenabled)
	{
		GL_SelectTextureUnit(GL_TEXTURE1);
		glClientActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

/* selects texture unit 0 */
void GL_DisableMultitexture(void)
{
	if (mtexenabled)
	{
		glDisable(GL_TEXTURE_2D);
		glClientActiveTexture(GL_TEXTURE0);
		GL_SelectTextureUnit(GL_TEXTURE0);
		mtexenabled = false;
	}
}

/*
 * Wrapper around glDeleteTextures that also clears the given texture number
 * from our per-TMU cached texture binding table.
 */
static void GL_DeleteTexture(gltexture_t *texture)
{
	glDeleteTextures(1, &texture->texnum);

	if (texture->texnum == currenttexture[0])
		currenttexture[0] = GL_UNUSED_TEXTURE;
	if (texture->texnum == currenttexture[1])
		currenttexture[1] = GL_UNUSED_TEXTURE;

	texture->texnum = 0;
}

/*
 ================================================================================

 COMMANDS

 ================================================================================
 */

typedef struct
{
	int magfilter;
	int minfilter;
	const char *name;
} glmode_t;
static glmode_t glmodes[] = { { GL_NEAREST, GL_NEAREST, "GL_NEAREST" }, { GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, "GL_NEAREST_MIPMAP_NEAREST" }, { GL_NEAREST,
		GL_NEAREST_MIPMAP_LINEAR, "GL_NEAREST_MIPMAP_LINEAR" }, { GL_LINEAR, GL_LINEAR, "GL_LINEAR" }, { GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST,
		"GL_LINEAR_MIPMAP_NEAREST" }, { GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, "GL_LINEAR_MIPMAP_LINEAR" }, };
#define NUM_GLMODES (int)(sizeof(glmodes)/sizeof(glmodes[0]))
static int glmode_idx = NUM_GLMODES - 1; /* trilinear */

/* report available texturemodes */
static void TexMgr_DescribeTextureModes_f(void)
{
	int i;

	for (i = 0; i < NUM_GLMODES ; i++)
		Con_SafePrintf("   %2i: %s\n", i + 1, glmodes[i].name);

	Con_Printf("%i modes\n", i);
}

static void TexMgr_SetFilterModes(gltexture_t *glt)
{
	GL_Bind(glt);

	if (glt->flags & TEX_NEAREST)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else if (glt->flags & TEX_LINEAR)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else if (glt->flags & TEX_MIPMAP)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmodes[glmode_idx].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glmodes[glmode_idx].minfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy.value);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmodes[glmode_idx].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glmodes[glmode_idx].magfilter);
	}
}

/* called when gl_texturemode changes */
static void TexMgr_TextureMode_f(cvar_t *var)
{
	gltexture_t *glt;
	int i;

	for (i = 0; i < NUM_GLMODES ; i++)
	{
		if (!strcmp(glmodes[i].name, gl_texturemode.string))
		{
			if (glmode_idx != i)
			{
				glmode_idx = i;
				for (glt = active_gltextures; glt; glt = glt->next)
					TexMgr_SetFilterModes(glt);
				Sbar_Changed(); //sbar graphics need to be redrawn with new filter mode
				//FIXME: warpimages need to be redrawn, too.
			}
			return;
		}
	}

	for (i = 0; i < NUM_GLMODES ; i++)
	{
		if (!strcasecmp(glmodes[i].name, gl_texturemode.string))
		{
			Cvar_SetQuick(&gl_texturemode, glmodes[i].name);
			return;
		}
	}

	i = atoi(gl_texturemode.string);
	if (i >= 1 && i <= NUM_GLMODES)
	{
		Cvar_SetQuick(&gl_texturemode, glmodes[i - 1].name);
		return;
	}

	Con_Printf("\"%s\" is not a valid texturemode\n", gl_texturemode.string);
	Cvar_SetQuick(&gl_texturemode, glmodes[glmode_idx].name);
}

/* called when gl_texture_anisotropy changes */
static void TexMgr_Anisotropy_f(cvar_t *var)
{
	if (gl_texture_anisotropy.value < 1)
	{
		Cvar_SetQuick(&gl_texture_anisotropy, "1");
	}
	else if (gl_texture_anisotropy.value > gl_max_anisotropy)
	{
		Cvar_SetValueQuick(&gl_texture_anisotropy, gl_max_anisotropy);
	}
	else
	{
		gltexture_t *glt;
		for (glt = active_gltextures; glt; glt = glt->next)
		{
			/*  TexMgr_SetFilterModes (glt);*/
			if (glt->flags & TEX_MIPMAP)
			{
				GL_Bind(glt);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmodes[glmode_idx].magfilter);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glmodes[glmode_idx].minfilter);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy.value);
			}
		}
	}
}

/* report loaded textures */
static void TexMgr_Imagelist_f(void)
{
	float mb;
	float texels = 0;
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		Con_SafePrintf("   %4i x%4i %s\n", glt->width, glt->height, glt->name);
		if (glt->flags & TEX_MIPMAP)
			texels += glt->width * glt->height * 4.0f / 3.0f;
		else
			texels += (glt->width * glt->height);
	}

	mb = texels * (Cvar_VariableValue("vid_bpp") / 8.0f) / 0x100000;
	Con_Printf("%i textures %i pixels %1.1f megabytes\n", numgltextures, (int) texels, mb);
}

typedef struct targaheader_s {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} targaheader_t;

#define TARGAHEADERSIZE 18 //size on disk

/* writes RGB or RGBA data to a TGA file */
bool Image_WriteTGA (const char *name, byte *data, int width, int height, int bpp, bool upsidedown)
{
	int handle, i, size, temp, bytes;
	char pathname[MAX_OSPATH];
	byte header[TARGAHEADERSIZE];

	Sys_mkdir (com_gamedir); //if we've switched to a nonexistant gamedir, create it now so we don't crash
	snprintf (pathname, sizeof(pathname), "%s/%s", com_gamedir, name);
	handle = Sys_FileOpenWrite (pathname);
	if (handle == -1)
		return false;

	memset (&header, 0, TARGAHEADERSIZE);
	header[2] = 2; // uncompressed type
	header[12] = width&255;
	header[13] = width>>8;
	header[14] = height&255;
	header[15] = height>>8;
	header[16] = bpp; // pixel size
	if (upsidedown)
		header[17] = 0x20; //upside-down attribute

	// swap red and blue bytes
	bytes = bpp/8;
	size = width*height*bytes;
	for (i=0; i<size; i+=bytes)
	{
		temp = data[i];
		data[i] = data[i+2];
		data[i+2] = temp;
	}

	Sys_FileWrite (handle, &header, TARGAHEADERSIZE);
	Sys_FileWrite (handle, data, size);
	Sys_FileClose (handle);

	return true;
}

/* dump all current textures to TGA files */
static void TexMgr_Imagedump_f (void)
{
	char tganame[MAX_OSPATH], tempname[MAX_OSPATH], dirname[MAX_OSPATH];
	gltexture_t *glt;
	byte *buffer;
	char *c;

	//create directory
	snprintf(dirname, sizeof(dirname), "%s/imagedump", com_gamedir);
	Sys_mkdir (dirname);

	//loop through textures
	for (glt = active_gltextures; glt; glt = glt->next)
	{
		strlcpy (tempname, glt->name, sizeof(tempname));
		while ( (c = strchr(tempname, ':')) ) *c = '_';
		while ( (c = strchr(tempname, '/')) ) *c = '_';
		while ( (c = strchr(tempname, '*')) ) *c = '_';
		snprintf(tganame, sizeof(tganame), "imagedump/%s.tga", tempname);

		GL_Bind (glt);
		if (glt->flags & TEX_ALPHA)
		{
			buffer = (byte *) malloc(glt->width*glt->height*4);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA(tganame, buffer, glt->width, glt->height, 32, true);
		}
		else
		{
			buffer = (byte *) malloc(glt->width*glt->height*3);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA(tganame, buffer, glt->width, glt->height, 24, true);
		}
		free (buffer);
	}

	Con_Printf ("dumped %i textures to %s\n", numgltextures, dirname);
}

/* report texture memory usage for this frame */
float TexMgr_FrameUsage(void)
{
	float mb;
	float texels = 0;
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->visframe == r_framecount)
		{
			if (glt->flags & TEX_MIPMAP)
				texels += glt->width * glt->height * 4.0f / 3.0f;
			else
				texels += (glt->width * glt->height);
		}
	}

	mb = texels * (Cvar_VariableValue("vid_bpp") / 8.0f) / 0x100000;
	return mb;
}

gltexture_t *TexMgr_FindTexture(const char *name)
{
	gltexture_t *glt;

	if (name)
	{
		for (glt = active_gltextures; glt; glt = glt->next)
		{
			if (!strcmp(glt->name, name))
				return glt;
		}
	}

	return NULL;
}

gltexture_t *TexMgr_NewTexture(void)
{
	gltexture_t *glt;

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error("numgltextures == MAX_GLTEXTURES\n");

	glt = free_gltextures;
	free_gltextures = glt->next;
	glt->next = active_gltextures;
	active_gltextures = glt;

	glGenTextures(1, &glt->texnum);
	numgltextures++;
	return glt;
}

// workaround for preventing TexMgr_FreeTexture during TexMgr_ReloadImages
static bool in_reload_images;

void TexMgr_FreeTexture(gltexture_t *kill)
{
	gltexture_t *glt;

	if (in_reload_images)
		return;

	if (kill == NULL)
	{
		Con_Printf("TexMgr_FreeTexture: NULL texture\n");
		return;
	}

	if (active_gltextures == kill)
	{
		active_gltextures = kill->next;
		kill->next = free_gltextures;
		free_gltextures = kill;

		GL_DeleteTexture(kill);
		numgltextures--;
		return;
	}

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->next == kill)
		{
			glt->next = kill->next;
			kill->next = free_gltextures;
			free_gltextures = kill;

			GL_DeleteTexture(kill);
			numgltextures--;
			return;
		}
	}

	Con_Printf("TexMgr_FreeTexture: not found\n");
}

/* compares each bit in "flags" to the one in glt->flags only if that bit is active in "mask" */
void TexMgr_FreeTextures(unsigned int flags, unsigned int mask)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if ((glt->flags & mask) == (flags & mask))
			TexMgr_FreeTexture(glt);
	}
}

void TexMgr_DeleteTextureObjects(void)
{
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		GL_DeleteTexture(glt);
	}
}

void TexMgr_LoadPalette(void)
{
	byte *pal, *src, *dst;
	int i;
	FILE *f;

	COM_FOpenFile("gfx/palette.lmp", &f);
	if (!f)
		Sys_Error("Couldn't load gfx/palette.lmp");

	pal = (byte *)malloc(768);
	fread(pal, 1, 768, f);
	fclose(f);

	//standard palette, 255 is transparent
	dst = (byte *) d_8to24table;
	src = pal;
	for (i = 0; i < 256; i++)
	{
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = 255;
	}
	((byte *) &d_8to24table[255])[3] = 0;

	//fullbright palette, 0-223 are black (for additive blending)
	src = pal + 224 * 3;
	dst = (byte *) &d_8to24table_fbright[224];
	for (i = 224; i < 256; i++)
	{
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = 255;
	}
	for (i = 0; i < 224; i++)
	{
		dst = (byte *) &d_8to24table_fbright[i];
		dst[3] = 255;
		dst[2] = dst[1] = dst[0] = 0;
	}

	//nobright palette, 224-255 are black (for additive blending)
	dst = (byte *) d_8to24table_nobright;
	src = pal;
	for (i = 0; i < 256; i++)
	{
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = 255;
	}
	for (i = 224; i < 256; i++)
	{
		dst = (byte *) &d_8to24table_nobright[i];
		dst[3] = 255;
		dst[2] = dst[1] = dst[0] = 0;
	}

	//fullbright palette, for fence textures
	memcpy(d_8to24table_fbright_fence, d_8to24table_fbright, 256 * 4);
	d_8to24table_fbright_fence[255] = 0; // Alpha of zero.

	//nobright palette, for fence textures
	memcpy(d_8to24table_nobright_fence, d_8to24table_nobright, 256 * 4);
	d_8to24table_nobright_fence[255] = 0; // Alpha of zero.

	//conchars palette, 0 and 255 are transparent
	memcpy(d_8to24table_conchars, d_8to24table, 256 * 4);
	((byte *) &d_8to24table_conchars[0])[3] = 0;

	free(pal);
}

void TexMgr_NewGame(void)
{
	TexMgr_FreeTextures(0, TEX_PERSIST); //deletes all textures where TEX_PERSIST is unset
	TexMgr_LoadPalette();
}

/* return smallest power of two greater than or equal to s */
int TexMgr_Pad(int s)
{
	int i;
	for (i = 1; i < s; i <<= 1)
		;
	return i;
}

/* return a size with hardware and user prefs in mind*/
int TexMgr_SafeTextureSize(int s)
{
	if (!gl_texture_NPOT)
		s = TexMgr_Pad(s);
	if ((int) gl_max_size.value > 0)
		s = min(TexMgr_Pad((int ) gl_max_size.value), s);
	s = min(gl_hardware_maxsize, s);
	return s;
}

/* only pad if a texture of that size would be padded */
int TexMgr_PadConditional(int s)
{
	if (s < TexMgr_SafeTextureSize(s))
		return TexMgr_Pad(s);
	else
		return s;
}

static unsigned *TexMgr_MipMapW(unsigned *data, int width, int height)
{
	int i, size;
	byte *out, *in;

	out = in = (byte *) data;
	size = (width * height) >> 1;

	for (i = 0; i < size; i++, out += 4, in += 8)
	{
		out[0] = (in[0] + in[4]) >> 1;
		out[1] = (in[1] + in[5]) >> 1;
		out[2] = (in[2] + in[6]) >> 1;
		out[3] = (in[3] + in[7]) >> 1;
	}

	return data;
}

static unsigned *TexMgr_MipMapH(unsigned *data, int width, int height)
{
	int i, j;
	byte *out, *in;

	out = in = (byte *) data;
	height >>= 1;
	width <<= 2;

	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 4, out += 4, in += 4)
		{
			out[0] = (in[0] + in[width + 0]) >> 1;
			out[1] = (in[1] + in[width + 1]) >> 1;
			out[2] = (in[2] + in[width + 2]) >> 1;
			out[3] = (in[3] + in[width + 3]) >> 1;
		}
	}

	return data;
}

/* bilinear resample */
static unsigned *TexMgr_ResampleTexture(unsigned *in, int inwidth, int inheight, bool alpha)
{
	byte *nwpx, *nepx, *swpx, *sepx, *dest;
	unsigned xfrac, yfrac, x, y, modx, mody, imodx, imody, injump, outjump;
	unsigned *out;
	int i, j, outwidth, outheight;

	if (inwidth == TexMgr_Pad(inwidth) && inheight == TexMgr_Pad(inheight))
		return in;

	outwidth = TexMgr_Pad(inwidth);
	outheight = TexMgr_Pad(inheight);
	out = (unsigned *) Hunk_Alloc(outwidth * outheight * 4);

	xfrac = ((inwidth - 1) << 16) / (outwidth - 1);
	yfrac = ((inheight - 1) << 16) / (outheight - 1);
	y = outjump = 0;

	for (i = 0; i < outheight; i++)
	{
		mody = (y >> 8) & 0xFF;
		imody = 256 - mody;
		injump = (y >> 16) * inwidth;
		x = 0;

		for (j = 0; j < outwidth; j++)
		{
			modx = (x >> 8) & 0xFF;
			imodx = 256 - modx;

			nwpx = (byte *) (in + (x >> 16) + injump);
			nepx = nwpx + 4;
			swpx = nwpx + inwidth * 4;
			sepx = swpx + 4;

			dest = (byte *) (out + outjump + j);

			dest[0] = (nwpx[0] * imodx * imody + nepx[0] * modx * imody + swpx[0] * imodx * mody + sepx[0] * modx * mody) >> 16;
			dest[1] = (nwpx[1] * imodx * imody + nepx[1] * modx * imody + swpx[1] * imodx * mody + sepx[1] * modx * mody) >> 16;
			dest[2] = (nwpx[2] * imodx * imody + nepx[2] * modx * imody + swpx[2] * imodx * mody + sepx[2] * modx * mody) >> 16;
			if (alpha)
				dest[3] = (nwpx[3] * imodx * imody + nepx[3] * modx * imody + swpx[3] * imodx * mody + sepx[3] * modx * mody) >> 16;
			else
				dest[3] = 255;

			x += xfrac;
		}
		outjump += outwidth;
		y += yfrac;
	}

	return out;
}

/*
 * eliminate pink edges on sprites, etc.
 * operates in place on 32bit data
 */
static void TexMgr_AlphaEdgeFix(byte *data, int width, int height)
{
	int i, j, n = 0, b, c[3] = { 0, 0, 0 }, lastrow, thisrow, nextrow, lastpix, thispix, nextpix;
	byte *dest = data;

	for (i = 0; i < height; i++)
	{
		lastrow = width * 4 * ((i == 0) ? height - 1 : i - 1);
		thisrow = width * 4 * i;
		nextrow = width * 4 * ((i == height - 1) ? 0 : i + 1);

		for (j = 0; j < width; j++, dest += 4)
		{
			if (dest[3]) //not transparent
				continue;

			lastpix = 4 * ((j == 0) ? width - 1 : j - 1);
			thispix = 4 * j;
			nextpix = 4 * ((j == width - 1) ? 0 : j + 1);

			b = lastrow + lastpix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = thisrow + lastpix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = nextrow + lastpix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = lastrow + thispix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = nextrow + thispix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = lastrow + nextpix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = thisrow + nextpix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}
			b = nextrow + nextpix;
			if (data[b + 3])
			{
				c[0] += data[b];
				c[1] += data[b + 1];
				c[2] += data[b + 2];
				n++;
			}

			//average all non-transparent neighbors
			if (n)
			{
				dest[0] = (byte) (c[0] / n);
				dest[1] = (byte) (c[1] / n);
				dest[2] = (byte) (c[2] / n);

				n = c[0] = c[1] = c[2] = 0;
			}
		}
	}
}

/*
 * special case of AlphaEdgeFix for textures that only need it because they were padded
 * operates in place on 32bit data, and expects unpadded height and width values
 */
static void TexMgr_PadEdgeFixW(byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;

	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);

	//copy last full column to first empty column, leaving alpha byte at zero
	src = data + (width - 1) * 4;
	for (i = 0; i < padh; i++)
	{
		src[4] = src[0];
		src[5] = src[1];
		src[6] = src[2];
		src += padw * 4;
	}

	//copy first full column to last empty column, leaving alpha byte at zero
	src = data;
	dst = data + (padw - 1) * 4;
	for (i = 0; i < padh; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += padw * 4;
		dst += padw * 4;
	}
}

/*
 * special case of AlphaEdgeFix for textures that only need it because they were padded
 * operates in place on 32bit data, and expects unpadded height and width values
 */
static void TexMgr_PadEdgeFixH(byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;

	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);

	//copy last full row to first empty row, leaving alpha byte at zero
	dst = data + height * padw * 4;
	src = dst - padw * 4;
	for (i = 0; i < padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}

	//copy first full row to last empty row, leaving alpha byte at zero
	dst = data + (padh - 1) * padw * 4;
	src = data;
	for (i = 0; i < padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}
}

static unsigned *TexMgr_8to32(byte *in, int pixels, unsigned int *usepal)
{
	int i;
	unsigned *out, *data;

	out = data = (unsigned *) Hunk_Alloc(pixels * 4);

	for (i = 0; i < pixels; i++)
		*out++ = usepal[*in++];

	return data;
}

/* return image with width padded up to power-of-two dimentions */
static byte *TexMgr_PadImageW(byte *in, int width, int height, byte padbyte)
{
	int i, j, outwidth;
	byte *out, *data;

	if (width == TexMgr_Pad(width))
		return in;

	outwidth = TexMgr_Pad(width);

	out = data = (byte *) Hunk_Alloc(outwidth * height);

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
			*out++ = *in++;
		for (; j < outwidth; j++)
			*out++ = padbyte;
	}

	return data;
}

/* return image with height padded up to power-of-two dimentions */
static byte *TexMgr_PadImageH(byte *in, int width, int height, byte padbyte)
{
	int i, srcpix, dstpix;
	byte *data, *out;

	if (height == TexMgr_Pad(height))
		return in;

	srcpix = width * height;
	dstpix = width * TexMgr_Pad(height);

	out = data = (byte *) Hunk_Alloc(dstpix);

	for (i = 0; i < srcpix; i++)
		*out++ = *in++;
	for (; i < dstpix; i++)
		*out++ = padbyte;

	return data;
}

/* handles 32bit source data */
static void TexMgr_LoadImage32(gltexture_t *glt, unsigned *data)
{
	int miplevel, mipwidth, mipheight, picmip;

	if (!gl_texture_NPOT)
	{
		// resample up
		data = TexMgr_ResampleTexture(data, glt->width, glt->height, glt->flags & TEX_ALPHA);
		glt->width = TexMgr_Pad(glt->width);
		glt->height = TexMgr_Pad(glt->height);
	}

	// mipmap down
	picmip = (glt->flags & TEX_NOPICMIP) ? 0 : max((int ) gl_picmip.value, 0);
	mipwidth = TexMgr_SafeTextureSize(glt->width >> picmip);
	mipheight = TexMgr_SafeTextureSize(glt->height >> picmip);
	while ((int) glt->width > mipwidth)
	{
		TexMgr_MipMapW(data, glt->width, glt->height);
		glt->width >>= 1;
		if (glt->flags & TEX_ALPHA)
			TexMgr_AlphaEdgeFix((byte *) data, glt->width, glt->height);
	}
	while ((int) glt->height > mipheight)
	{
		TexMgr_MipMapH(data, glt->width, glt->height);
		glt->height >>= 1;
		if (glt->flags & TEX_ALPHA)
			TexMgr_AlphaEdgeFix((byte *) data, glt->width, glt->height);
	}

	// upload
	GL_Bind(glt);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// upload mipmaps
	if (glt->flags & TEX_MIPMAP)
	{
		mipwidth = glt->width;
		mipheight = glt->height;

		for (miplevel = 1; mipwidth > 1 || mipheight > 1; miplevel++)
		{
			if (mipwidth > 1)
			{
				TexMgr_MipMapW(data, mipwidth, mipheight);
				mipwidth >>= 1;
			}
			if (mipheight > 1)
			{
				TexMgr_MipMapH(data, mipwidth, mipheight);
				mipheight >>= 1;
			}
			glTexImage2D(GL_TEXTURE_2D, miplevel, GL_RGBA, mipwidth, mipheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
	}

	// set filter modes
	TexMgr_SetFilterModes(glt);
}

/* handles 8bit source data, then passes it to LoadImage32 */
static void TexMgr_LoadImage8(gltexture_t *glt, byte *data)
{
	extern cvar_t gl_fullbright;
	bool padw = false, padh = false;
	byte padbyte;
	unsigned int *usepal;
	int i;

	// HACK HACK HACK -- taken from tomazquake
	if (strstr(glt->name, "shot1sid") && glt->width == 32 && glt->height == 32 && CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy(data, data + 32 * 31, 32);
	}

	// detect false alpha cases
	if ((glt->flags & TEX_ALPHA) && !(glt->flags & TEX_CONCHARS))
	{
		for (i = 0; i < (int) (glt->width * glt->height); i++)
			if (data[i] == 255) //transparent index
				break;
		if (i == (int) (glt->width * glt->height))
			glt->flags -= TEX_ALPHA;
	}

	// choose palette and padbyte
	if (glt->flags & TEX_FULLBRIGHT)
	{
		if (glt->flags & TEX_ALPHA)
			usepal = d_8to24table_fbright_fence;
		else
			usepal = d_8to24table_fbright;
		padbyte = 0;
	}
	else if ((glt->flags & TEX_NOBRIGHT) && gl_fullbright.value)
	{
		if (glt->flags & TEX_ALPHA)
			usepal = d_8to24table_nobright_fence;
		else
			usepal = d_8to24table_nobright;
		padbyte = 0;
	}
	else if (glt->flags & TEX_CONCHARS)
	{
		usepal = d_8to24table_conchars;
		padbyte = 0;
	}
	else
	{
		usepal = d_8to24table;
		padbyte = 255;
	}

	// pad each dimension, but only if it's not going to be downsampled later
	if (glt->flags & TEX_PAD)
	{
		if ((int) glt->width < TexMgr_SafeTextureSize(glt->width))
		{
			data = TexMgr_PadImageW(data, glt->width, glt->height, padbyte);
			glt->width = TexMgr_Pad(glt->width);
			padw = true;
		}
		if ((int) glt->height < TexMgr_SafeTextureSize(glt->height))
		{
			data = TexMgr_PadImageH(data, glt->width, glt->height, padbyte);
			glt->height = TexMgr_Pad(glt->height);
			padh = true;
		}
	}

	// convert to 32bit
	data = (byte *) TexMgr_8to32(data, glt->width * glt->height, usepal);

	// fix edges
	if (glt->flags & TEX_ALPHA)
		TexMgr_AlphaEdgeFix(data, glt->width, glt->height);
	else
	{
		if (padw)
			TexMgr_PadEdgeFixW(data, glt->source_width, glt->source_height);
		if (padh)
			TexMgr_PadEdgeFixH(data, glt->source_width, glt->source_height);
	}

	// upload it
	TexMgr_LoadImage32(glt, (unsigned *)data);
}

/* handles lightmap data */
static void TexMgr_LoadLightmap(gltexture_t *glt, byte *data)
{
	// upload it
	GL_Bind(glt);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, glt->width, glt->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

	// set filter modes
	TexMgr_SetFilterModes(glt);
}

/* the one entry point for loading all textures */
gltexture_t *TexMgr_LoadImage(const char *name, int width, int height, enum srcformat format, byte *data, unsigned flags)
{
	unsigned short crc;
	gltexture_t *glt;
	int mark;

	// cache check
	switch (format)
	{
	case SRC_INDEXED:
		crc = CRC_Block(data, width * height);
		break;
	case SRC_LIGHTMAP:
		crc = CRC_Block(data, width * height * 1);
		break;
	case SRC_RGBA:
		crc = CRC_Block(data, width * height * 4);
		break;
	default: /* not reachable but avoids compiler warnings */
		crc = 0;
	}
	if ((flags & TEX_OVERWRITE) && (glt = TexMgr_FindTexture(name)))
	{
		if (glt->source_crc == crc)
			return glt;
	}
	else
		glt = TexMgr_NewTexture();

	// copy data
	strlcpy(glt->name, name, sizeof(glt->name));
	glt->width = width;
	glt->height = height;
	glt->flags = flags;
	glt->shirt = -1;
	glt->pants = -1;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;

	//upload it
	mark = Hunk_LowMark();

	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_LoadImage8(glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_LoadLightmap(glt, data);
		break;
	case SRC_RGBA:
		TexMgr_LoadImage32(glt, (unsigned *)data);
		break;
	}

	Hunk_FreeToLowMark(mark);

	return glt;
}

/*
 ================================================================================

 COLORMAPPING AND TEXTURE RELOADING

 ================================================================================
 */

/* reloads a texture, and colormaps it if needed */
void TexMgr_ReloadImage(gltexture_t *glt, int shirt, int pants)
{
	byte translation[256];
	byte *src, *dst, *data = NULL, *translated;
	int size, i;
//
// get source data
//
//	data = (byte *) glt->source_data; //image in memory

	if (!data)
	{
		Con_Printf("TexMgr_ReloadImage: invalid source for %s\n", glt->name);
		return;
	}

	glt->width = glt->source_width;
	glt->height = glt->source_height;
//
// apply shirt and pants colors
//
// if shirt and pants are -1,-1, use existing shirt and pants colors
// if existing shirt and pants colors are -1,-1, don't bother colormapping
	if (shirt > -1 && pants > -1)
	{
		if (glt->source_format == SRC_INDEXED)
		{
			glt->shirt = shirt;
			glt->pants = pants;
		}
		else
			Con_Printf("TexMgr_ReloadImage: can't colormap a non SRC_INDEXED texture: %s\n", glt->name);
	}
	if (glt->shirt > -1 && glt->pants > -1)
	{
		//create new translation table
		for (i = 0; i < 256; i++)
			translation[i] = i;

		shirt = glt->shirt * 16;
		if (shirt < 128)
		{
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE + i] = shirt + i;
		}
		else
		{
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE + i] = shirt + 15 - i;
		}

		pants = glt->pants * 16;
		if (pants < 128)
		{
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE + i] = pants + i;
		}
		else
		{
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE + i] = pants + 15 - i;
		}

		//translate texture
		size = glt->width * glt->height;
		dst = translated = (byte *) Hunk_Alloc(size);
		src = data;

		for (i = 0; i < size; i++)
			*dst++ = translation[*src++];

		data = translated;
	}
//
// upload it
//
	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_LoadImage8(glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_LoadLightmap(glt, data);
		break;
	case SRC_RGBA:
		TexMgr_LoadImage32(glt, (unsigned *)data);
		break;
	}
}

/* must be called before any texture loading */
void TexMgr_Init(void)
{
	int i;
	static byte notexture_data[16] = { //black and pink checker
		104,   0,
		  0, 104,
	};
	static byte nulltexture_data[16] = { //black and blue checker
		 42,   0,
		  0,  42,
	};
	extern texture_t *r_notexture_mip, *r_notexture_mip2;

	// init texture list
	free_gltextures = (gltexture_t *) Hunk_AllocName(MAX_GLTEXTURES * sizeof(gltexture_t), "gltextures");
	active_gltextures = NULL;
	for (i = 0; i < MAX_GLTEXTURES - 1; i++)
		free_gltextures[i].next = &free_gltextures[i + 1];
	free_gltextures[i].next = NULL;
	numgltextures = 0;

	// palette
	TexMgr_LoadPalette();

	Cvar_RegisterVariable(&gl_max_size);
	Cvar_RegisterVariable(&gl_picmip);
	Cvar_RegisterVariable(&gl_texture_anisotropy);
	Cvar_SetCallback(&gl_texture_anisotropy, &TexMgr_Anisotropy_f);
	gl_texturemode.string = (char *) glmodes[glmode_idx].name;
	Cvar_RegisterVariable(&gl_texturemode);
	Cvar_SetCallback(&gl_texturemode, &TexMgr_TextureMode_f);
	Cmd_AddCommand("gl_describetexturemodes", &TexMgr_DescribeTextureModes_f);
	Cmd_AddCommand("imagelist", &TexMgr_Imagelist_f);
	Cmd_AddCommand("imagedump", &TexMgr_Imagedump_f);

	// poll max size from hardware
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_hardware_maxsize);

	// load notexture images
	notexture = TexMgr_LoadImage("notexture", 2, 2, SRC_INDEXED, notexture_data, TEX_NEAREST | TEX_PERSIST | TEX_NOPICMIP);
	nulltexture = TexMgr_LoadImage("nulltexture", 2, 2, SRC_INDEXED, nulltexture_data, TEX_NEAREST | TEX_PERSIST | TEX_NOPICMIP);

	//have to assign these here becuase Mod_Init is called before TexMgr_Init
	r_notexture_mip->gltexture = r_notexture_mip2->gltexture = notexture;
}

/* reloads all texture images. called only by vid_restart */
void TexMgr_ReloadImages(void)
{
	gltexture_t *glt;

// ericw -- tricky bug: if the hunk is almost full, an allocation in TexMgr_ReloadImage
// triggers cache items to be freed, which calls back into TexMgr to free the
// texture. If this frees 'glt' in the loop below, the active_gltextures
// list gets corrupted.
// A test case is jam3_tronyn.bsp with -heapsize 65536, and do several mode
// switches/fullscreen toggles
// 2015-09-04 -- Cache_Flush workaround was causing issues (http://sourceforge.net/p/quakespasm/bugs/10/)
// switching to a boolean flag.
	in_reload_images = true;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		glGenTextures(1, &glt->texnum);
		TexMgr_ReloadImage(glt, -1, -1);
	}

	in_reload_images = false;
}

/* reloads all texture that were loaded with the nobright palette */
void TexMgr_ReloadNobrightImages(void)
{
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
		if (glt->flags & TEX_NOBRIGHT)
			TexMgr_ReloadImage(glt, -1, -1);
}
