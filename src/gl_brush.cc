/*
 * Brush model rendering
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
#include "model.h"

#include "brush.fs.h"
#include "brush.vs.h"

extern gltexture_t *solidskytexture;
extern gltexture_t *alphaskytexture;

// shader program
GLuint brush_program;

// uniforms used in vertex shader
GLuint brush_ProjectionUniform;
GLuint brush_ModelViewUniform;
GLuint brush_TimeUniform;
GLuint brush_SpeedScaleUniform;
GLuint brush_VieworgUniform;

// uniforms used in fragment shader
GLuint brush_TexUniform;
GLuint brush_UseLMTexUniform;
GLuint brush_UseOverbrightUniform;
GLuint brush_AlphaUniform;

// attribs
GLuint brush_VertexAttrib;
GLuint brush_TexCoordsAttrib;
GLuint brush_LMTexCoordsAttrib;

// VBOs
GLuint brush_elements_VBO;

static void EmitSkySurface(msurface_t *fa)
{
	float speedscale = cl.time * 8; // for top sky
//	speedscale -= (int)speedscale & ~127;

	float speedscale2 = cl.time * 16; // and bottom sky
//	speedscale2 -= (int)speedscale2 & ~127;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fa->indicesVBO);

	GL_BindToUnit(GL_TEXTURE0, solidskytexture);
	glUniform1f(brush_SpeedScaleUniform, speedscale);
	glDrawElements(GL_TRIANGLES, fa->indices.size(), GL_UNSIGNED_SHORT, 0);

	GL_BindToUnit(GL_TEXTURE0, alphaskytexture);
	glUniform1f(brush_SpeedScaleUniform, speedscale2);
	glDrawElements(GL_TRIANGLES, fa->indices.size(), GL_UNSIGNED_SHORT, 0);

	glUniform1f(brush_SpeedScaleUniform, 0.0f);
}

static void GL_DrawSkySurfaces(brush_model_t *brushmodel, texchain_t chain)
{
	for (size_t i = 0; i < brushmodel->numtextures; i++)
	{
		texture_t *t = brushmodel->textures[i];
		if (!t)
			continue; // No texture

		if (!t->isskytexture)
			continue; // Not sky texture

		msurface_t *s = t->texturechains[chain];
		if (!s)
			continue; // No surfaces

		for (; s; s = s->texturechain)
			EmitSkySurface(s);

		t->texturechains[chain] = NULL;

		break; // Only one sky texture
	}
}

static void GL_DrawWaterSurfaces(brush_model_t *brushmodel, texchain_t chain)
{
	glUniform1f(brush_AlphaUniform, r_wateralpha.value);
	glUniform1f(brush_TimeUniform, cl.time);

	for (size_t i = 0; i < brushmodel->numtextures; i++)
	{
		texture_t *t = brushmodel->textures[i];
		if (!t)
			continue; // No texture

		msurface_t *s = t->texturechains[chain];
		if (!s)
			continue; // No surfaces

		if (t->isskytexture)
			continue; // Handled specially

		if (!(s->flags & SURF_DRAWTURB))
			continue;

		GL_BindToUnit(GL_TEXTURE0, t->gltexture);

		for (; s; s = s->texturechain)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->indicesVBO);
			glDrawElements(GL_TRIANGLES, s->indices.size(), GL_UNSIGNED_SHORT, 0);
		}

		t->texturechains[chain] = NULL;
	}

	glUniform1f(brush_AlphaUniform, 1.0f);
}

extern size_t lightmap_count;
typedef struct glRect_s
{
	unsigned short l, t, w, h;
} glRect_t;
typedef struct lightmap_s
{
	gltexture_t *texture;
	std::vector<unsigned short> indices;

	bool modified;
	glRect_t rectchange;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte *data;	//[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
} lightmap_t;
extern lightmap_t *lightmaps;

void GL_DrawSurfaces(brush_model_t *brushmodel, texchain_t chain)
{
	// setup
	glUseProgram(brush_program);

	// set vertex uniforms
	glUniformMatrix4fv(brush_ProjectionUniform, 1, GL_FALSE, projectionMatrix.get());
	glUniformMatrix4fv(brush_ModelViewUniform, 1, GL_FALSE, modelViewMatrix.get());
	glUniform1f(brush_TimeUniform, 0.0f);
	glUniform1f(brush_SpeedScaleUniform, 0.0f);
	glUniform3fv(brush_VieworgUniform, 1, r_refdef.vieworg);

	// set fragment uniforms
	glUniform1i(brush_TexUniform, 0);
	glUniform1i(brush_UseLMTexUniform, GL_FALSE);
	glUniform1i(brush_UseOverbrightUniform, !!gl_overbright.value);
	glUniform1f(brush_AlphaUniform, 1.0f);

	glBindBuffer(GL_ARRAY_BUFFER, brushmodel->vertsVBO);
	glVertexAttribPointer(brush_VertexAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, brushmodel->texVBO);
	glVertexAttribPointer(brush_TexCoordsAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, brushmodel->light_texVBO);
	glVertexAttribPointer(brush_LMTexCoordsAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

	for (size_t i = 0; i < brushmodel->numtextures; i++)
	{
		texture_t *t = brushmodel->textures[i];
		if (!t)
			continue; // No texture

		msurface_t *s = t->texturechains[chain];
		if (!s)
			continue; // No surfaces

		if (t->isskytexture)
			continue; // Handled specially

		if (s->flags & SURF_DRAWTURB)
			continue; // Water surfaces can be transparent draw last

		std::vector<unsigned short> indices;

		for (; s; s = s->texturechain)
		{
			c_brush_polys++;
			indices.insert(std::end(indices), std::begin(s->indices), std::end(s->indices));
		}

		// draw
		GL_BindToUnit(GL_TEXTURE0, t->gltexture);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, brush_elements_VBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(), GL_STREAM_DRAW);
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, 0);

		t->texturechains[chain] = NULL;
	}

	// Lightmap pass
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR); // COMBINE_RGB MODULATE
	glUniform1i(brush_UseLMTexUniform, GL_TRUE);
	for (size_t i = 0; i < lightmap_count; i++)
	{
		lightmap_t *lightmap = &lightmaps[i];
		if (lightmap->indices.empty())
			continue;
		GL_BindToUnit(GL_TEXTURE0, lightmap->texture);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, brush_elements_VBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(lightmap->indices[0]) * lightmap->indices.size(), lightmap->indices.data(), GL_STREAM_DRAW);
		glDrawElements(GL_TRIANGLES, lightmap->indices.size(), GL_UNSIGNED_SHORT, 0);
	}
	glUniform1i(brush_UseLMTexUniform, GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Full Bright pass
	for (size_t i = 0; i < brushmodel->numtextures; i++)
	{
		texture_t *t = brushmodel->textures[i];
		if (!t)
			continue; // No texture

		msurface_t *s = t->fullbrightchains[chain];
		if (!s)
			continue; // No surfaces

		if (t->isskytexture)
			continue; // Handled specially

		if (s->flags & SURF_DRAWTURB)
			continue; // Water surfaces can be transparent draw last

		std::vector<unsigned short> indices;

		for (; s; s = s->fullbrightchain)
		{
//			c_brush_polys++;
			indices.insert(std::end(indices), std::begin(s->indices), std::end(s->indices));
		}

		// draw
		GL_BindToUnit(GL_TEXTURE0, t->fullbright);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, brush_elements_VBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * indices.size(), indices.data(), GL_STREAM_DRAW);
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, 0);

		t->fullbrightchains[chain] = NULL;
	}

	GL_DrawWaterSurfaces(brushmodel, chain);

	glDisable(GL_BLEND);

	GL_DrawSkySurfaces(brushmodel, chain);
}

void R_ChainSurface (msurface_t *surf, texchain_t chain);

void GL_DrawBrushModel(entity_t *ent)
{
	brush_model_t *brushmodel = ent->model->brushmodel;

	vec3_t modelorg;
	VectorSubtract(r_refdef.vieworg, ent->origin, modelorg);
	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
	{
		vec3_t temp, forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(ent->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	R_ClearLightmapPolys();

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (brushmodel->firstmodelsurface != 0)
		R_PushDlights(&brushmodel->nodes[brushmodel->hulls[0].firstclipnode],
				brushmodel->surfaces);

	// Push
	GLfloat mv_matrix[16];
	memcpy(mv_matrix, modelViewMatrix.get(), sizeof(mv_matrix));

	GL_RotateForEntity(ent, modelViewMatrix);

	// draw texture
	msurface_t *psurf = &brushmodel->surfaces[brushmodel->firstmodelsurface];
	for (int i = 0; i < brushmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		mplane_t *pplane = psurf->plane;
		float dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
		    (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_ChainSurface(psurf, chain_model);
			R_RenderDynamicLightmaps(psurf);
		}
	}

	R_UploadLightmaps();

	GL_DrawSurfaces(brushmodel, chain_model);

	// Pop
	modelViewMatrix.set(mv_matrix);
}

void GL_CreateBrushShaders(void)
{
	// generate program
	brush_program = LoadShader((const char *)brush_vs, brush_vs_len,
	                           (const char *)brush_fs, brush_fs_len);

	// get uniform locations
	brush_ProjectionUniform = GL_GetUniformLocation(brush_program, "Projection");
	brush_ModelViewUniform = GL_GetUniformLocation(brush_program, "ModelView");
	brush_TimeUniform = GL_GetUniformLocation(brush_program, "Time");
	brush_SpeedScaleUniform = GL_GetUniformLocation(brush_program, "SpeedScale");
	brush_VieworgUniform = GL_GetUniformLocation(brush_program, "Vieworg");

	brush_TexUniform = GL_GetUniformLocation(brush_program, "Tex");
	brush_UseLMTexUniform = GL_GetUniformLocation(brush_program, "UseLMTex");
	brush_UseOverbrightUniform = GL_GetUniformLocation(brush_program, "UseOverbright");
	brush_AlphaUniform = GL_GetUniformLocation(brush_program, "Alpha");

	// get attribute locations
	brush_VertexAttrib = GL_GetAttribLocation(brush_program, "Vertex");
	brush_TexCoordsAttrib = GL_GetAttribLocation(brush_program, "TexCoords");
	brush_LMTexCoordsAttrib = GL_GetAttribLocation(brush_program, "LMTexCoords");

	glEnableVertexAttribArray(brush_VertexAttrib);
	glEnableVertexAttribArray(brush_TexCoordsAttrib);
	glEnableVertexAttribArray(brush_LMTexCoordsAttrib);

	glGenBuffers(1, &brush_elements_VBO);
}
