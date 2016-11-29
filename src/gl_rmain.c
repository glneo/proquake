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

vec3_t modelorg;
entity_t *currententity;

int r_visframecount; // bumped when going to a new PVS
int r_framecount; // used for dlight push checking

static mplane_t frustum[4];

int c_brush_polys, c_alias_polys;

int particletexture; // little dot for particles
int playertextures; // up to 16 color translated skins
bool envmap; // true during envmap command capture

int cnttextures[2] = { -1, -1 }; // cached

int skyboxtextures;

int mirrortexturenum; // quake texturenum, not gltexturenum
bool mirror;
mplane_t *mirror_plane;

// view origin
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];
float r_base_world_matrix[16];

// screen size info
refdef_t r_refdef;

mleaf_t *r_viewleaf, *r_oldviewleaf;

texture_t *r_notexture_mip;

int d_lightstylevalue[256]; // 8.8 fraction of base light value

cvar_t r_norefresh = { "r_norefresh", "0" };
cvar_t r_drawentities = { "r_drawentities", "1" };
cvar_t r_speeds = { "r_speeds", "0" };
cvar_t r_shadows = { "r_shadows", "0" };

cvar_t r_mirroralpha = { "r_mirroralpha", "1" };
cvar_t r_wateralpha = { "r_wateralpha", "1", true };
cvar_t r_dynamic = { "r_dynamic", "1" };
cvar_t r_novis = { "r_novis", "0" };
cvar_t r_colored_dead_bodies = { "r_colored_dead_bodies", "1" };
cvar_t r_interpolate_animation = { "r_interpolate_animation", "0", true };
cvar_t r_interpolate_transform = { "r_interpolate_transform", "0", true };
cvar_t r_interpolate_weapon = { "r_interpolate_weapon", "0", true };
cvar_t r_clearcolor = { "r_clearcolor", "0" };
cvar_t r_farclip = { "r_farclip", "16384", true };
cvar_t gl_nearwater_fix = { "gl_nearwater_fix", "1", true };
cvar_t gl_fadescreen_alpha = { "gl_fadescreen_alpha", "0.7", true };

cvar_t gl_clear = { "gl_clear", "0" };
cvar_t gl_cull = { "gl_cull", "1" };
cvar_t gl_smoothmodels = { "gl_smoothmodels", "1" };
cvar_t gl_affinemodels = { "gl_affinemodels", "0" };
cvar_t gl_polyblend = { "gl_polyblend", "1", true };
cvar_t gl_flashblend = { "gl_flashblend", "1", true };
cvar_t gl_playermip = { "gl_playermip", "0", true };
cvar_t gl_nocolors = { "gl_nocolors", "0" };
cvar_t gl_finish = { "gl_finish", "0" };

cvar_t r_truegunangle = { "r_truegunangle", "0", true };  // Baker 3.80x - Optional "true" gun positioning on viewmodel
cvar_t r_drawviewmodel = { "r_drawviewmodel", "1", true };  // Baker 3.80x - Save to config
cvar_t r_ringalpha = { "r_ringalpha", "0.4", true }; // Baker 3.80x - gl_ringalpha
cvar_t r_fullbright = { "r_fullbright", "0" };
cvar_t r_lightmap = { "r_lightmap", "0" };

cvar_t gl_fullbright = { "gl_fullbright", "0", true };
cvar_t gl_overbright = { "gl_overbright", "1", true };

/*
 =================
 R_CullBox

 Returns true if the box is completely outside the frustum
 =================
 */
bool R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++)
		if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
			return true;

	return false;
}

void R_RotateForEntity(entity_t *ent)
{
	glTranslatef(ent->origin[0], ent->origin[1], ent->origin[2]);

	glRotatef(ent->angles[1], 0, 0, 1);
	glRotatef(-ent->angles[0], 0, 1, 0);
	glRotatef(ent->angles[2], 1, 0, 0);
}

/*
 =================
 R_CullBox -- replaced with new function from lordhavoc

 Returns true if the box is completely outside the frustum
 =================
 */
bool R_CullBoxA(const vec3_t emins, const vec3_t emaxs)
{
	int i;
	mplane_t *p;
	for (i = 0; i < 4; i++)
	{
		p = frustum + i;
		switch (p->signbits)
		{
		default:
		case 0:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist)
				return true;
			break;
		}
	}
	return false;
}

/*
 =================
 R_CullSphere

 Returns true if the sphere is completely outside the frustum
 =================
 */
#define PlaneDiff(point, plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
bool R_CullSphere(const vec3_t centre, const float radius)
{
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++)
	{
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

bool R_CullForEntity(const entity_t *ent/*, vec3_t returned_center*/)
{
	vec3_t mins, maxs;

	if (ent->angles[0] || ent->angles[1] || ent->angles[2])
		return R_CullSphere(ent->origin, ent->model->radius); // Angles turned; do sphere cull test

	// Angles all 0; do box cull test
	VectorAdd(ent->origin, ent->model->mins, mins); // Add entity origin and model mins to calc mins
	VectorAdd(ent->origin, ent->model->maxs, maxs); // Add entity origin and model maxs to calc maxs

//	if (returned_center)
//		LerpVector (mins, maxs, 0.5, returned_center);
	return R_CullBoxA(mins, maxs);

}

void R_DrawEntitiesOnList(void)
{
	int i;
	void SortEntitiesByTransparency(void);

	if (!r_drawentities.value)
		return;

	// Baker: http://forums.inside3d.com/viewtopic.php?p=13458
	//        Transparent entities need sorted to ensure items behind transparent objects get drawn (z-buffer)

	// draw sprites seperately, because of alpha blending
	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel(currententity);
			break;

		case mod_brush:

			// Get rid of Z-fighting for textures by offsetting the
			// drawing of entity models compared to normal polygons.
			glEnable(GL_POLYGON_OFFSET_FILL);
			R_DrawBrushModel(currententity);
			glDisable(GL_POLYGON_OFFSET_FILL);

			break;

		default:
			break;
		}
	}

	for (i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel(currententity);
			break;

		default:
			break;
		}
	}
}

void R_DrawViewModel(void)
{
	float ambient[4], diffuse[4];
	int j;
	int lnum;
	vec3_t dist;
	float add;
	dlight_t *dl;
	int ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if ((cl.items & IT_INVISIBILITY) && (r_ringalpha.value == 1.0f))
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	j = R_LightPoint(currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights
	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract(currententity->origin, dl->origin, dist);
		add = dl->radius - VectorLength(dist);
		if (add > 0)
			ambientlight += add;
	}

	// JPG 3.02 - gamma correction
	if (ambientlight > 255)
		ambientlight = 255;
	if (shadelight > 255)
		shadelight = 255;

	ambientlight = gammatable[ambientlight];
	shadelight = gammatable[shadelight];

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float) ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float) shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	glDepthRangef(gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	R_DrawAliasModel(currententity);

	glDepthRangef(gldepthmin, gldepthmax);
}

void R_PolyBlend(void)
{
	if (!v_blend[3])	// No blends ... get outta here
		return;

	if (!gl_polyblend.value)
		return;

	GL_DisableMultitexture();

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	glPushMatrix();
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); // put Z going up
	glRotatef(90, 0, 0, 1); // put Z going up

	glColor4f(v_blend[0], v_blend[1], v_blend[2], v_blend[3]);

	GLfloat verts[] = {
		10, 100, 100,
		10, -100, 100,
		10, -100, -100,
		10, 100, -100,
	};

	glEnableClientState(GL_VERTEX_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glPopMatrix();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

}

int SignbitsForPlane(mplane_t *out)
{
	int bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

/*
 ===============
 TurnVector

 turn forward towards side on the plane defined by forward and side
 if angle = 90, the result will be equal to side
 assumes side and forward are perpendicular, and normalized
 to turn away from side, use a negative angle
 ===============
 */
void TurnVector(vec3_t out, vec3_t forward, vec3_t side, float angle)
{
	float scale_forward, scale_side;

	scale_forward = cos(DEG2RAD(angle));
	scale_side = sin(DEG2RAD(angle));

	out[0] = scale_forward * forward[0] + scale_side * side[0];
	out[1] = scale_forward * forward[1] + scale_side * side[1];
	out[2] = scale_forward * forward[2] + scale_side * side[2];
}

void R_SetFrustum(void)
{
	int i;

	TurnVector(frustum[0].normal, vpn, vright, r_refdef.fov_x / 2 - 90); //left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - r_refdef.fov_x / 2); //right plane
	TurnVector(frustum[2].normal, vpn, vup, 90 - r_refdef.fov_y / 2); //bottom plane
	TurnVector(frustum[3].normal, vpn, vup, r_refdef.fov_y / 2 - 90); //top plane

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane(&frustum[i]);
	}
}

void R_SetupFrame(void)
{
	// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set("r_fullbright", "0");

	R_AnimateLight();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy(r_refdef.vieworg, r_origin);

	AngleVectors(r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel->brushmodel);

	V_SetContentsColor(r_viewleaf->contents);

	V_CalcBlend();

	c_brush_polys = 0;
	c_alias_polys = 0;
}

void Q_glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar)
{
	GLfloat matrix[] = {
		(2.0f * zNear)/(right-left), 0.0f, 0.0f, 0.0f,
		0.0f, (2.0f * zNear)/(top-bottom), 0.0f, 0.0f,
		(right + left) / (right - left), (top + bottom) / (top - bottom), -(zFar + zNear) / (zFar - zNear), -1.0f,
		0.0f, 0.0f, -(2 * zFar * zNear) / (zFar - zNear), 0.0f,
	};

	glMultMatrixf(matrix);
}

void Q_gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	Q_glFrustumf(xmin, xmax, ymin, ymax, zNear, zFar);
}

void R_SetupGL(void)
{
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	glViewport(glx + x, gly + y2, w, h);
	screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
	farclip = max((int )r_farclip.value, 4096);
	Q_gluPerspective(r_refdef.fov_y, screenaspect, 4, farclip); // 4096

	if (mirror)
	{
		if (mirror_plane->normal[2])
			glScalef(1, -1, 1);
		else
			glScalef(-1, 1, 1);
		glCullFace(GL_BACK);
	}
	else
		glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0);	    // put Z going up
	glRotatef(90, 0, 0, 1);	    // put Z going up

	glRotatef(-r_refdef.viewangles[2], 1, 0, 0);
	glRotatef(-r_refdef.viewangles[0], 0, 1, 0);
	glRotatef(-r_refdef.viewangles[1], 0, 0, 1);
	glTranslatef(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	glGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	// set drawing parms
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

cvar_t r_waterwarp = { "r_waterwarp", "0", true }; // Baker 3.60 - Save this to config now

/*
 ===============
 R_TranslatePlayerSkin

 Translates a skin texture by the per-player color lookup
 ===============
 */

bool recentcolor_isSet[MAX_SCOREBOARD];
int recentcolor[MAX_SCOREBOARD];
int recentskinnum[MAX_SCOREBOARD];

void R_TranslatePlayerSkin(int playernum)
{
//	int top, bottom, i, j, size;
//	byte translate[256];
//	unsigned translate32[256];
//	model_t *model;
//	alias_model_t *aliasmodel;
//	byte *original;
//
//	unsigned pixels[512 * 256];
//
//	unsigned *out;
//	unsigned scaled_width, scaled_height;
//	int inwidth, inheight;
//	byte *inrow;
//	unsigned frac, fracstep;
//
//	// locate the original skin pixels
//	currententity = &cl_entities[1 + playernum];
//	if (!(model = currententity->model))
//		return;		// player doesn't have a model yet
//	if (model->type != mod_alias)
//		return; // only translate skins on alias models
//	if ((currententity->model->flags & MOD_PLAYER) == 0)
//		return; // Only translate player model
//	if (recentcolor_isSet[playernum] && recentcolor[playernum] == cl.scores[playernum].colors && recentskinnum[playernum] == currententity->skinnum)
//		return; // Same color as before
//
//	recentcolor_isSet[playernum] = true;
//	recentcolor[playernum] = cl.scores[playernum].colors;
//	recentskinnum[playernum] = currententity->skinnum;
//
//	GL_DisableMultitexture();
//
//	top = cl.scores[playernum].colors & 0xf0;
//	bottom = (cl.scores[playernum].colors & 15) << 4;
//
//	for (i = 0; i < 256; i++)
//		translate[i] = i;
//
//	for (i = 0; i < 16; i++)
//	{
//		// the artists made some backwards ranges. sigh.
//		translate[TOP_RANGE + i] = (top < 128) ? top + i : top + 15 - i;
//		translate[BOTTOM_RANGE + i] = (bottom < 128) ? bottom + i : bottom + 15 - i;
//	}
//
//	aliasmodel = (alias_model_t *) Mod_Extradata(model);
//	size = aliasmodel->skinwidth * aliasmodel->skinheight;
//	if (currententity->skinnum < 0 || currententity->skinnum >= aliasmodel->numskins)
//	{
//		Con_Printf("(%d): Invalid player skin #%d\n", playernum, currententity->skinnum);
//		original = (byte *) aliasmodel + aliasmodel->texels[0];
//	}
//	else
//	{
//		original = (byte *) aliasmodel + aliasmodel->texels[currententity->skinnum];
//	}
//
//	if (size & 3)
//		Sys_Error("R_TranslatePlayerSkin: bad size (%d)", size);
//
//	inwidth = aliasmodel->skinwidth;
//	inheight = aliasmodel->skinheight;
//
//	// because this happens during gameplay, do it fast
//	// instead of sending it through gl_upload 8
//	GL_Bind(playertextures + playernum);
//
//	scaled_width = gl_max_size < 512 ? gl_max_size : 512;
//	scaled_height = gl_max_size < 256 ? gl_max_size : 256;
//
//	// allow users to crunch sizes down even more if they want
//	scaled_width >>= (int) gl_playermip.value;
//	scaled_height >>= (int) gl_playermip.value;
//
//	for (i = 0; i < 256; i++)
//		translate32[i] = d_8to24table[translate[i]];
//
//	out = pixels;
//	fracstep = inwidth * 0x10000 / scaled_width;
//	for (i = 0; i < scaled_height; i++, out += scaled_width)
//	{
//		inrow = original + inwidth * (i * inheight / scaled_height);
//		frac = fracstep >> 1;
//		for (j = 0; j < scaled_width; j += 4)
//		{
//			out[j] = translate32[inrow[frac >> 16]];
//			frac += fracstep;
//			out[j + 1] = translate32[inrow[frac >> 16]];
//			frac += fracstep;
//			out[j + 2] = translate32[inrow[frac >> 16]];
//			frac += fracstep;
//			out[j + 3] = translate32[inrow[frac >> 16]];
//			frac += fracstep;
//		}
//	}
//	glTexImage2D(GL_TEXTURE_2D, 0, gl_solid_format, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
//
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

}

void R_NewMap(void)
{
	int i;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->brushmodel->numleafs; i++)
		cl.worldmodel->brushmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles();

	GL_BuildLightmaps();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;

	for (i = 0; i < cl.worldmodel->brushmodel->numtextures; i++)
	{
		if (!cl.worldmodel->brushmodel->textures[i])
			continue;
		if (!strncmp(cl.worldmodel->brushmodel->textures[i]->name, "sky", 3))
			skytexturenum = i;
		if (!strncmp(cl.worldmodel->brushmodel->textures[i]->name, "window02_1", 10))
			mirrortexturenum = i;
		cl.worldmodel->brushmodel->textures[i]->texturechain = NULL;
	}
}

/*
 ====================
 R_TimeRefresh_f

 For program optimization
 ====================
 */
void R_TimeRefresh_f(void)
{
	int i;
	float start, stop, time;

	if (cls.state != ca_connected)
		return;

//	glDrawBuffer(GL_FRONT);
	glFinish();

	start = Sys_DoubleTime();
	for (i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView();
	}

	glFinish();
	stop = Sys_DoubleTime();
	time = stop - start;
	Con_Printf("%f seconds (%f fps)\n", time, 128.0 / time);

//	glDrawBuffer(GL_BACK);
	GL_EndRendering();
}

void R_SetClearColor_f(struct cvar_s *cvar)
{
	byte *rgb;
	int s;
	extern cvar_t r_clearcolor;

	s = (int) r_clearcolor.value & 0xFF;
	rgb = (byte*) (d_8to24table + s);
	glClearColor(rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0, 0);
}

void R_Init(void)
{
	extern cvar_t gl_finish;
	extern cvar_t r_truegunangle;
	extern cvar_t r_farclip;
	extern cvar_t r_ringalpha;
	extern cvar_t gl_fullbright;

	Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand("pointfile", R_ReadPointFile_f);

	Cvar_RegisterVariable(&r_norefresh);
	Cvar_RegisterVariable(&r_lightmap);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_ringalpha);
	Cvar_RegisterVariable(&r_truegunangle);

	Cvar_RegisterVariable(&r_shadows);
	Cvar_RegisterVariable(&r_mirroralpha);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_dynamic);
	Cvar_RegisterVariable(&r_novis);
	Cvar_RegisterVariable(&r_colored_dead_bodies);
	Cvar_RegisterVariable(&r_speeds);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&r_farclip);

	// fenix@io.com: register new cvar for model interpolation
	Cvar_RegisterVariable(&r_interpolate_animation);
	Cvar_RegisterVariable(&r_interpolate_transform);
	Cvar_RegisterVariable(&r_interpolate_weapon);
	Cvar_RegisterVariable(&r_clearcolor);
	Cvar_SetCallback(&r_clearcolor, R_SetClearColor_f);

	Cvar_RegisterVariable(&gl_finish);

	Cvar_RegisterVariable(&gl_cull);
	Cvar_RegisterVariable(&gl_smoothmodels);
	Cvar_RegisterVariable(&gl_affinemodels);
	Cvar_RegisterVariable(&gl_polyblend);
	Cvar_RegisterVariable(&gl_flashblend);
	Cvar_RegisterVariable(&gl_playermip);
	Cvar_RegisterVariable(&gl_nocolors);

//	Cvar_RegisterVariable (&gl_keeptjunctions);
//	Cvar_RegisterVariable (&gl_reporttjunctions);
	Cvar_RegisterVariable(&gl_fullbright);
	Cvar_RegisterVariable(&gl_overbright);

//	Cvar_RegisterVariable (&gl_doubleeyes);

	Cvar_RegisterVariable(&gl_nearwater_fix);
	Cvar_RegisterVariable(&gl_fadescreen_alpha);

	R_InitTextures();
	R_InitParticles();
	R_InitParticleTexture();

	playertextures = texture_extension_number;
	texture_extension_number += MAX_SCOREBOARD;

	skyboxtextures = texture_extension_number;
	texture_extension_number += 6;
}

/*
 ================
 R_RenderScene

 r_refdef must be set before the first call
 ================
 */
void R_RenderScene(void)
{
	R_SetupFrame();
	R_SetFrustum();
	R_SetupGL();
	R_MarkLeaves(); // done here so we know if we're in water
	R_DrawWorld(); // adds static entities to the list
	S_ExtraUpdate(); // don't let sound get messed up if going slow
	R_DrawEntitiesOnList();
	GL_DisableMultitexture();
	R_RenderDlights();
	R_DrawParticles();
}

void R_Clear(void)
{
	GLbitfield clearbits = GL_DEPTH_BUFFER_BIT;

	// If gl_clear is 1 we always clear the color buffer
	if (gl_clear.value)
		clearbits |= GL_COLOR_BUFFER_BIT;

	if (r_mirroralpha.value < 1.0) // Baker 3.99: was != 1.0, changed in event gets set to # higher than 1.0
	{
		glClear(clearbits);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc(GL_LEQUAL);
	}
	else
	{
		glClear(clearbits);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc(GL_LEQUAL);
	}

	glDepthRangef(gldepthmin, gldepthmax);
}

void R_Mirror(void)
{
	float d;
	msurface_t *s;
	entity_t *ent;

	if (!mirror)
		return;

	memcpy(r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA(r_refdef.vieworg, -2 * d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct(vpn, mirror_plane->normal);
	VectorMA(vpn, -2 * d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin(vpn[2]) / M_PI * 180;
	r_refdef.viewangles[1] = atan2(vpn[1], vpn[0]) / M_PI * 180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRangef(gldepthmin, gldepthmax);
	glDepthFunc(GL_LEQUAL);

	R_RenderScene();
	R_DrawWaterSurfaces();

	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRangef(gldepthmin, gldepthmax);
	glDepthFunc(GL_LEQUAL);

	// blend on top
	glEnable(GL_BLEND);
	//Baker 3.60 - Mirror alpha fix - from QER

	if (r_mirroralpha.value < 1) // Baker 3.61 - Only run mirror alpha fix if it is being used; hopefully this may fix a possible crash issue on some video cards
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	//mirror fix
	glMatrixMode(GL_PROJECTION);
	if (mirror_plane->normal[2])
		glScalef(1, -1, 1);
	else
		glScalef(-1, 1, 1);
	glCullFace(GL_FRONT);
	glMatrixMode(GL_MODELVIEW);

	glLoadMatrixf(r_base_world_matrix);

	glColor4f(1.0f, 1.0f, 1.0f, r_mirroralpha.value);
	s = cl.worldmodel->brushmodel->textures[mirrortexturenum]->texturechain;
	for (; s; s = s->texturechain)
		R_RenderBrushPoly(s);
	cl.worldmodel->brushmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable(GL_BLEND);
	//Baker 3.60 - Mirror alpha fix - from QER
	if (r_mirroralpha.value < 1) // Baker 3.61 - Only run mirror alpha fix if it is being used; hopefully this may fix a possible crash issue on some video cards
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//mirror fix
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/*
 ================
 R_RenderView

 r_refdef must be set before the first call
 ================
 */
void R_RenderView(void)
{
	double time1 = 0.0, time2;

	if (r_norefresh.value)
		return;

	if (!cl.worldmodel)
		Sys_Error("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish();
		time1 = Sys_DoubleTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	if (gl_finish.value)
		glFinish();

	R_Clear();

	// render normal view
	R_RenderScene();
	R_DrawViewModel();
	R_DrawWaterSurfaces();

	// render mirror view
	R_Mirror();

	R_PolyBlend();

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime();
		Con_Printf("%3i ms  %4i wpoly %4i epoly\n", (int) ((time2 - time1) * 1000), c_brush_polys, c_alias_polys);
	}
}
