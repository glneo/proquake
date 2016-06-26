/*
 * Copyright (C) 2004-2004 MH
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

#include "gl_model.h"

int FindFullbrightTexture (byte *pixels, int num_pix);
void DrawFullBrightTextures (msurface_t *first_surf, int num_surfs);
void ConvertPixels (byte *pixels, int num_pixels);
