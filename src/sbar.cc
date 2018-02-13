/*
 * Status bar code
 *
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2007-2008 Kristian Duske
 * Copyright (C) 2010-2014 QuakeSpasm developers
 * Copyright (C) 2015-2018 QuickQuake team
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

#define STAT_MINUS 10 // num frame for '-' stats digit

qpic_t *sb_nums[2][11];
qpic_t *sb_colon, *sb_slash;
qpic_t *sb_ibar;
qpic_t *sb_sbar;
qpic_t *sb_scorebar;

qpic_t *sb_weapons[7][8]; // 0 is active, 1 is owned, 2-5 are flashes
qpic_t *sb_ammo[4];
qpic_t *sb_sigil[4];
qpic_t *sb_armor[3];
qpic_t *sb_items[32];

qpic_t *sb_faces[7][2]; // 0 is gibbed, 1 is dead, 2-6 are alive
                        // 0 is static, 1 is temporary animation
qpic_t *sb_face_invis;
qpic_t *sb_face_quad;
qpic_t *sb_face_invuln;
qpic_t *sb_face_invis_invuln;

bool sb_showscores;

int sb_lines; // scan lines to draw

qpic_t *rsb_invbar[2];
qpic_t *rsb_weapons[5];
qpic_t *rsb_items[2];
qpic_t *rsb_ammo[3];
qpic_t *rsb_teambord; // PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
qpic_t *hsb_weapons[7][5]; // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
int hipweapons[4] =
{
	HIT_LASER_CANNON_BIT,
	HIT_MJOLNIR_BIT,
	4,
	HIT_PROXIMITY_GUN_BIT,
};
//MED 01/04/97 added hipnotic items array
qpic_t *hsb_items[2];

/* Tab key down */
void Sbar_ShowScores(void)
{
	sb_showscores = true;
}

/* Tab key up */
void Sbar_DontShowScores(void)
{
	sb_showscores = false;
}

//=============================================================================

// drawing routines are relative to the status bar location
void Sbar_DrawPic(int x, int y, qpic_t *pic)
{
	Draw_Pic(x, y, pic, scr_sbaralpha.value);
}

void Sbar_DrawCharacter(int x, int y, int num)
{
	Draw_Character(x, y + 24, num, scr_sbaralpha.value);
}

void Sbar_DrawString(int x, int y, const char *str)
{
	Draw_String(x, y + 24, str, scr_sbaralpha.value);
}

void Sbar_DrawNum(int x, int y, int num, int color)
{
	int val = CLAMP(-99, num, 999);
	for (int j = 0; j < 3; j++)
	{
		int digit = val % 10;
		Sbar_DrawPic(x + (24 * (2 - j)), y, sb_nums[color][digit]);
		val /= 10;
		if (val == 0)
			break;
	}
}

//=============================================================================

int fragsort[MAX_SCOREBOARD];

char scoreboardtext[MAX_SCOREBOARD][20];
int scoreboardtop[MAX_SCOREBOARD];
int scoreboardbottom[MAX_SCOREBOARD];
int scoreboardcount[MAX_SCOREBOARD];
int scoreboardlines;

void Sbar_SortFrags(void)
{
	int i, j, k;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < cl.maxclients; i++)
	{
		if (cl.scores[i].name[0])
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i = 0; i < scoreboardlines; i++)
	{
		for (j = 0; j < scoreboardlines - 1 - i; j++)
		{
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j + 1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}

int Sbar_ColorForMap(int m)
{
	return m < 128 ? m + 8 : m + 8; // FIXME: lol wut?
}

void Sbar_UpdateScoreboard(void)
{
	int i, k;
	int top, bottom;
	scoreboard_t *s;

	Sbar_SortFrags();

// draw the text
	memset(scoreboardtext, 0, sizeof(scoreboardtext));

	for (i = 0; i < scoreboardlines; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		sprintf(&scoreboardtext[i][1], "%3i %s", s->frags, s->name);

		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		scoreboardtop[i] = Sbar_ColorForMap(top);
		scoreboardbottom[i] = Sbar_ColorForMap(bottom);
	}
}

void Sbar_SoloScoreboard(void)
{
	char str[256];
	int minutes, seconds, tens, units;
	int len;

	sprintf(str, "Kills: %i/%i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString(8, 12, str);

	sprintf(str, "Secrets: %i/%i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString(312 - strlen(str) * 8, 12, str);

//	if (!fitzmode)
//	{ /* QuakeSpasm customization: */
//		snprintf (str, sizeof(str), "skill %i", (int)(skill.value + 0.5));
//		Sbar_DrawString (160 - strlen(str)*4, 12, str);
//
//		snprintf (str, sizeof(str), "%s (%s)", cl.levelname, cl.mapname);
//		len = strlen (str);
//		if (len > 40)
//			Sbar_DrawScrollString (0, 4, 320, str);
//		else
//			Sbar_DrawString (160 - len*4, 4, str);
//		return;
//	}
	minutes = cl.time / 60;
	seconds = cl.time - 60 * minutes;
	tens = seconds / 10;
	units = seconds - 10 * tens;
	sprintf(str, "%i:%i%i", minutes, tens, units);
	Sbar_DrawString(160 - strlen(str) * 4, 12, str);

	len = strlen(cl.levelname);
	Sbar_DrawString(160 - len * 4, 4, cl.levelname);
}

void Sbar_DeathmatchOverlay(void)
{
	Draw_SetCanvas(CANVAS_MENU); //johnfitz

	qpic_t *ranking_pic = Draw_CachePic("gfx/ranking.lmp");
	Draw_Pic((320 - ranking_pic->width) / 2, 8, ranking_pic, 1.0f);

	// scores
	Sbar_SortFrags();

	int x = 80;
	int y = 40;
	for (int i = 0; i < scoreboardlines; i++)
	{
		int k = fragsort[i];
		scoreboard_t *s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// draw background
		int top = Sbar_ColorForMap(s->colors & 0xf0);
		int bottom = Sbar_ColorForMap((s->colors & 15) << 4);
		Draw_Fill(x, y, 40, 4, top, 1);
		Draw_Fill(x, y + 4, 40, 4, bottom, 1);

		// draw frags
		int frags = CLAMP(0, s->frags, 999);
		for (int j = 1; j <= 3; j++)
		{
			int digit = frags % 10;
			Draw_Character(x + (j * 8), -24, 18 + digit, scr_sbaralpha.value);
			frags /= 10;
		}

		if (k == cl.viewentity - 1)
			Draw_Character(x - 8, y, 12, scr_sbaralpha.value);

		// draw name
		Draw_String(x + 64, y, s->name, scr_sbaralpha.value);

		y += 10;
	}

	Draw_SetCanvas(CANVAS_SBAR); //johnfitz
}

void Sbar_DrawScoreboard(void)
{
	Sbar_SoloScoreboard();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay();
}

void Sbar_DrawInventory(void)
{
	int flashon;

	if (rogue)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN)
			Sbar_DrawPic(0, 0, rsb_invbar[0]);
		else
			Sbar_DrawPic(0, 0, rsb_invbar[1]);
	}
	else
		Sbar_DrawPic(0, 0, sb_ibar);

	// weapons
	for (int i = 0; i < 7; i++)
	{
		if (cl.items & (IT_SHOTGUN << i))
		{
			float time = cl.item_gettime[i];
			flashon = (int) ((cl.time - time) * 10);
			if (flashon >= 10)
			{
				if (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
					flashon = 1;
				else
					flashon = 0;
			}
			else
				flashon = (flashon % 5) + 2;

			Sbar_DrawPic(i * 24, 8, sb_weapons[flashon][i]);
		}
	}

	// MED 01/04/97
	// hipnotic weapons
	if (hipnotic)
	{
		int grenadeflashing = 0;
		for (int i = 0; i < 4; i++)
		{
			if (cl.items & (1 << hipweapons[i]))
			{
				float time = cl.item_gettime[hipweapons[i]];
				flashon = (int) ((cl.time - time) * 10);
				if (flashon >= 10)
				{
					if (cl.stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[i]))
						flashon = 1;
					else
						flashon = 0;
				}
				else
					flashon = (flashon % 5) + 2;

				// check grenade launcher
				if (i == 2)
				{
					if (cl.items & HIT_PROXIMITY_GUN)
					{
						if (flashon)
						{
							grenadeflashing = 1;
							Sbar_DrawPic(96, 8, hsb_weapons[flashon][2]);
						}
					}
				}
				else if (i == 3)
				{
					if (cl.items & (IT_SHOTGUN << 4))
					{
						if (flashon && !grenadeflashing)
							Sbar_DrawPic(96, 8, hsb_weapons[flashon][3]);
						else if (!grenadeflashing)
							Sbar_DrawPic(96, 8, hsb_weapons[0][3]);
					}
					else
						Sbar_DrawPic(96, 8, hsb_weapons[flashon][4]);
				}
				else
					Sbar_DrawPic(176 + (i * 24), 8, hsb_weapons[flashon][i]);
			}
		}
	}

	if (rogue)
		if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN) // check for powered up weapon.
			for (int i = 0; i < 5; i++)
				if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
					Sbar_DrawPic((i + 2) * 24, 8, rsb_weapons[i]);

	// ammo counts
	for (int i = 0; i < 4; i++)
	{
		int val = CLAMP(0, cl.stats[STAT_SHELLS + i], 999);
		for (int j = 0; j < 3; j++)
		{
			int digit = val % 10;
			if (digit == 0)
				break;
			Sbar_DrawCharacter((6 * i + (3 - j)) * 8 + 2, -24, 18 + digit);
			val /= 10;
		}
	}

	flashon = 0;
	// items
	for (int i = 0; i < 6; i++)
	{
		if (cl.items & (1 << (17 + i)))
		{
			float time = cl.item_gettime[17 + i];
			if (!time || time <= cl.time - 2 || !flashon)
			{
				//MED 01/04/97 changed keys
				if (!hipnotic || (i > 1))
					Sbar_DrawPic(192 + i * 16, 8, sb_items[i]);
			}
		}
	}

	//MED 01/04/97 added hipnotic items
	if (hipnotic) // hipnotic items
	{
		for (int i = 0; i < 2; i++)
		{
			if (cl.items & (1 << (24 + i)))
			{
				float time = cl.item_gettime[24 + i];
				if (!time || time <= cl.time - 2 || !flashon)
					Sbar_DrawPic(288 + i * 16, 8, hsb_items[i]);
			}
		}
	}

	if (rogue) // new rogue items
	{
		for (int i = 0; i < 2; i++)
		{
			if (cl.items & (1 << (29 + i)))
			{
				float time = cl.item_gettime[29 + i];
				if (!time || time <= cl.time - 2 || !flashon)
					Sbar_DrawPic(288 + i * 16, 8, rsb_items[i]);
			}
		}
	}
	else
	{
		// sigils
		for (int i = 0; i < 4; i++)
		{
			if (cl.items & (1 << (28 + i)))
			{
				float time = cl.item_gettime[28 + i];
				if (!time || time <= cl.time - 2 || !flashon)
					Sbar_DrawPic(320 - 32 + i * 8, 8, sb_sigil[i]);
			}
		}
	}
}

static void Sbar_DrawFrags(void)
{
	Sbar_SortFrags();

	// draw the text
	int x = 184;
	int numscores = min(scoreboardlines, 4);
	for (int i = 0; i < numscores; i++)
	{
		scoreboard_t *s = &cl.scores[fragsort[i]];
		if (!s->name[0])
			continue;

		int color;
		// top color
		color = s->colors & 0xf0;
		color = Sbar_ColorForMap(color);
		Draw_Fill(x + 10, 1, 28, 4, color, 1);

		// bottom color
		color = (s->colors & 15) << 4;
		color = Sbar_ColorForMap(color);
		Draw_Fill(x + 10, 5, 28, 3, color, 1);

		// number
		char num[12];
		sprintf(num, "%3i", s->frags);
		Sbar_DrawCharacter(x + 12, -24, num[0]);
		Sbar_DrawCharacter(x + 20, -24, num[1]);
		Sbar_DrawCharacter(x + 28, -24, num[2]);

		// brackets
		if (fragsort[i] == cl.viewentity - 1)
		{
			Sbar_DrawCharacter(x + 6, -24, 16);
			Sbar_DrawCharacter(x + 32, -24, 17);
		}

		x += 32;
	}
}

static void Sbar_MiniDeathmatchOverlay(void)
{
	int i, k, top, bottom, x, y, f;
	char num[12];
	scoreboard_t *s;

	float scale = CLAMP(1.0, scr_sbarscale.value, (float)vid.width / 320.0);
	// MAX_SCOREBOARDNAME = 32, so total width for this overlay plus sbar is 632, but we can cut off some i guess
	if (vid.width / scale < 512 || scr_viewsize.value >= 120)
		return;

	// scores
	Sbar_SortFrags();

	// draw the text
	int numlines = (scr_viewsize.value >= 110) ? 3 : 6;

	//find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.viewentity - 1)
			break;
	if (i == scoreboardlines) // we're not there
		i = 0;
	else // figure out start
		i = i - numlines / 2;
	if (i > scoreboardlines - numlines)
		i = scoreboardlines - numlines;
	if (i < 0)
		i = 0;

	x = 324;
	y = (scr_viewsize.value >= 110) ? 24 : 0; //johnfitz -- start at the right place
	for (; i < scoreboardlines && y <= 48; i++, y += 8) //johnfitz -- change y init, test, inc
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

		// colors
		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		top = Sbar_ColorForMap(top);
		bottom = Sbar_ColorForMap(bottom);

		Draw_Fill(x, y + 1, 40, 4, top, 1);
		Draw_Fill(x, y + 5, 40, 3, bottom, 1);

		// number
		f = s->frags;
		sprintf(num, "%3i", f);
		Draw_Character(x + 8, y, num[0], scr_sbaralpha.value);
		Draw_Character(x + 16, y, num[1], scr_sbaralpha.value);
		Draw_Character(x + 24, y, num[2], scr_sbaralpha.value);

		// brackets
		if (k == cl.viewentity - 1)
		{
			Draw_Character(x, y, 16, scr_sbaralpha.value);
			Draw_Character(x + 32, y, 17, scr_sbaralpha.value);
		}

		// name
		Draw_String(x + 48, y, s->name, scr_sbaralpha.value);
	}
}

static void Sbar_DrawFace(void)
{
	// PGM 01/19/97 - team color drawing
	// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
	if (rogue &&
	    (cl.maxclients != 1) &&
	    (teamplay.value > 3) &&
	    (teamplay.value < 7))
	{
		int top, bottom;
		int xofs;
		char num[12];
		scoreboard_t *s;

		s = &cl.scores[cl.viewentity - 1];
		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15) << 4;
		top = Sbar_ColorForMap(top);
		bottom = Sbar_ColorForMap(bottom);

		if (cl.gametype == GAME_DEATHMATCH)
			xofs = 113;
		else
			xofs = ((vid.width - 320) >> 1) + 113;

		Sbar_DrawPic(112, 24, rsb_teambord);
		Draw_Fill(xofs, /*vid.height-*/
		24 + 3, 22, 9, top, 1); //johnfitz -- sbar coords are now relative
		Draw_Fill(xofs, /*vid.height-*/
		24 + 12, 22, 9, bottom, 1); //johnfitz -- sbar coords are now relative

		// draw number
		sprintf(num, "%3i", s->frags);

		if (top == 8)
		{
			if (num[0] != ' ')
				Sbar_DrawCharacter(113, 3, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter(120, 3, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter(127, 3, 18 + num[2] - '0');
		}
		else
		{
			Sbar_DrawCharacter(113, 3, num[0]);
			Sbar_DrawCharacter(120, 3, num[1]);
			Sbar_DrawCharacter(127, 3, num[2]);
		}

		return;
	}
	// PGM 01/19/97 - team color drawing

	if ((cl.items & IT_INVISIBILITY) &&
	    (cl.items & IT_INVULNERABILITY))
	{
		Sbar_DrawPic(112, 24, sb_face_invis_invuln);
		return;
	}
	if (cl.items & IT_QUAD)
	{
		Sbar_DrawPic(112, 24, sb_face_quad);
		return;
	}
	if (cl.items & IT_INVISIBILITY)
	{
		Sbar_DrawPic(112, 24, sb_face_invis);
		return;
	}
	if (cl.items & IT_INVULNERABILITY)
	{
		Sbar_DrawPic(112, 24, sb_face_invuln);
		return;
	}

	int anim;
	int face = CLAMP(0, cl.stats[STAT_HEALTH] / 20, 4);
	if (cl.time <= cl.faceanimtime)
		anim = 1;
	else
		anim = 0;
	Sbar_DrawPic(112, 24, sb_faces[face][anim]);
}

void Sbar_Draw(void)
{
	if (scr_con_current == vid.height)
		return;	// console is full screen

	if (cl.intermission)
		return; // never draw sbar during intermission

	Draw_SetCanvas(CANVAS_DEFAULT);

	// don't waste fillrate by clearing the area behind the sbar
	float w = CLAMP(320.0f, scr_sbarscale.value * 320.0f, (float)vid.width);
	if (sb_lines && vid.width > w)
	{
		if (scr_sbaralpha.value < 1.0f)
			SRC_DrawTileClear(0, vid.height - sb_lines, vid.width, sb_lines);
		if (cl.gametype == GAME_DEATHMATCH)
			SRC_DrawTileClear(w, vid.height - sb_lines, vid.width - w, sb_lines);
		else
		{
			SRC_DrawTileClear(0, vid.height - sb_lines, (vid.width - w) / 2.0f, sb_lines);
			SRC_DrawTileClear((vid.width - w) / 2.0f + w, vid.height - sb_lines, (vid.width - w) / 2.0f, sb_lines);
		}
	}

	Draw_SetCanvas(CANVAS_SBAR);

	if (scr_viewsize.value < 110) // check viewsize instead of sb_lines
	{
		Sbar_DrawInventory();
		if (cl.maxclients != 1)
			Sbar_DrawFrags();
	}

	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
	{
		Sbar_DrawPic(0, 24, sb_scorebar);
		Sbar_DrawScoreboard();
	}
	else if (scr_viewsize.value < 120)
	{
		Sbar_DrawPic(0, 24, sb_sbar);

		// keys (hipnotic only)
		//MED 01/04/97 moved keys here so they would not be overwritten
		if (hipnotic)
		{
			if (cl.items & IT_KEY1)
				Sbar_DrawPic(209, 27, sb_items[0]);
			if (cl.items & IT_KEY2)
				Sbar_DrawPic(209, 36, sb_items[1]);
		}
		// armor
		if (cl.items & IT_INVULNERABILITY)
		{
			Sbar_DrawNum(24, 24, 666, 1);
//			Draw_Pic(0, 24, draw_disc);
		}
		else
		{
			if (rogue)
			{
				Sbar_DrawNum(24, 24, cl.stats[STAT_ARMOR], cl.stats[STAT_ARMOR] <= 25);
				if (cl.items & RIT_ARMOR3)
					Sbar_DrawPic(0, 24, sb_armor[2]);
				else if (cl.items & RIT_ARMOR2)
					Sbar_DrawPic(0, 24, sb_armor[1]);
				else if (cl.items & RIT_ARMOR1)
					Sbar_DrawPic(0, 24, sb_armor[0]);
			}
			else
			{
				Sbar_DrawNum(24, 24, cl.stats[STAT_ARMOR], cl.stats[STAT_ARMOR] <= 25);
				if (cl.items & IT_ARMOR3)
					Sbar_DrawPic(0, 24, sb_armor[2]);
				else if (cl.items & IT_ARMOR2)
					Sbar_DrawPic(0, 24, sb_armor[1]);
				else if (cl.items & IT_ARMOR1)
					Sbar_DrawPic(0, 24, sb_armor[0]);
			}
		}

		// face
		Sbar_DrawFace();

		// health
		Sbar_DrawNum(136, 24, cl.stats[STAT_HEALTH], cl.stats[STAT_HEALTH] <= 25);

		// ammo icon
		if (rogue)
		{
			if (cl.items & RIT_SHELLS)
				Sbar_DrawPic(224, 24, sb_ammo[0]);
			else if (cl.items & RIT_NAILS)
				Sbar_DrawPic(224, 24, sb_ammo[1]);
			else if (cl.items & RIT_ROCKETS)
				Sbar_DrawPic(224, 24, sb_ammo[2]);
			else if (cl.items & RIT_CELLS)
				Sbar_DrawPic(224, 24, sb_ammo[3]);
			else if (cl.items & RIT_LAVA_NAILS)
				Sbar_DrawPic(224, 24, rsb_ammo[0]);
			else if (cl.items & RIT_PLASMA_AMMO)
				Sbar_DrawPic(224, 24, rsb_ammo[1]);
			else if (cl.items & RIT_MULTI_ROCKETS)
				Sbar_DrawPic(224, 24, rsb_ammo[2]);
		}
		else
		{
			if (cl.items & IT_SHELLS)
				Sbar_DrawPic(224, 24, sb_ammo[0]);
			else if (cl.items & IT_NAILS)
				Sbar_DrawPic(224, 24, sb_ammo[1]);
			else if (cl.items & IT_ROCKETS)
				Sbar_DrawPic(224, 24, sb_ammo[2]);
			else if (cl.items & IT_CELLS)
				Sbar_DrawPic(224, 24, sb_ammo[3]);
		}

		Sbar_DrawNum(248, 24, cl.stats[STAT_AMMO], cl.stats[STAT_AMMO] <= 10);
	}

	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_MiniDeathmatchOverlay();
}

void Sbar_IntermissionOverlay(void)
{
	if (cl.gametype == GAME_DEATHMATCH)
	{
		Sbar_DeathmatchOverlay();
		return;
	}

	Draw_SetCanvas(CANVAS_MENU);

	qpic_t *complete_pic = Draw_CachePic("gfx/complete.lmp");
	Draw_Pic(64, 24, complete_pic, 1.0f);

	qpic_t *inter_pic = Draw_CachePic("gfx/inter.lmp");
	Draw_Pic(0, 56, inter_pic, 1.0f);

	int dig = cl.completed_time / 60;
	Sbar_DrawNum(152, 64, dig, 0);
	int num = cl.completed_time - dig * 60;
	Draw_Pic(224, 64, sb_colon, 1.0f);
	Draw_Pic(240, 64, sb_nums[0][num / 10], 1.0f);
	Draw_Pic(264, 64, sb_nums[0][num % 10], 1.0f);

	Sbar_DrawNum(152, 104, cl.stats[STAT_SECRETS], 0);
	Draw_Pic(224, 104, sb_slash, 1.0f);
	Sbar_DrawNum(240, 104, cl.stats[STAT_TOTALSECRETS], 0);

	Sbar_DrawNum(152, 144, cl.stats[STAT_MONSTERS], 0);
	Draw_Pic(224, 144, sb_slash, 1.0f);
	Sbar_DrawNum(240, 144, cl.stats[STAT_TOTALMONSTERS], 0);
}

void Sbar_FinaleOverlay(void)
{
	Draw_SetCanvas(CANVAS_MENU);

	qpic_t *pic = Draw_CachePic("gfx/finale.lmp");
	Draw_Pic((320 - pic->width) / 2, 16, pic, 1.0f);
}

/* load all the sbar pics */
static void Sbar_LoadPics(void)
{
	for (int i = 0; i < 10; i++)
	{
		sb_nums[0][i] = Draw_PicFromWad(va("num_%i", i));
		sb_nums[1][i] = Draw_PicFromWad(va("anum_%i", i));
	}

	sb_nums[0][10] = Draw_PicFromWad("num_minus");
	sb_nums[1][10] = Draw_PicFromWad("anum_minus");

	sb_colon = Draw_PicFromWad("num_colon");
	sb_slash = Draw_PicFromWad("num_slash");

	sb_weapons[0][0] = Draw_PicFromWad("inv_shotgun");
	sb_weapons[0][1] = Draw_PicFromWad("inv_sshotgun");
	sb_weapons[0][2] = Draw_PicFromWad("inv_nailgun");
	sb_weapons[0][3] = Draw_PicFromWad("inv_snailgun");
	sb_weapons[0][4] = Draw_PicFromWad("inv_rlaunch");
	sb_weapons[0][5] = Draw_PicFromWad("inv_srlaunch");
	sb_weapons[0][6] = Draw_PicFromWad("inv_lightng");

	sb_weapons[1][0] = Draw_PicFromWad("inv2_shotgun");
	sb_weapons[1][1] = Draw_PicFromWad("inv2_sshotgun");
	sb_weapons[1][2] = Draw_PicFromWad("inv2_nailgun");
	sb_weapons[1][3] = Draw_PicFromWad("inv2_snailgun");
	sb_weapons[1][4] = Draw_PicFromWad("inv2_rlaunch");
	sb_weapons[1][5] = Draw_PicFromWad("inv2_srlaunch");
	sb_weapons[1][6] = Draw_PicFromWad("inv2_lightng");

	for (int i = 0; i < 5; i++)
	{
		sb_weapons[2 + i][0] = Draw_PicFromWad(va("inva%i_shotgun", i + 1));
		sb_weapons[2 + i][1] = Draw_PicFromWad(va("inva%i_sshotgun", i + 1));
		sb_weapons[2 + i][2] = Draw_PicFromWad(va("inva%i_nailgun", i + 1));
		sb_weapons[2 + i][3] = Draw_PicFromWad(va("inva%i_snailgun", i + 1));
		sb_weapons[2 + i][4] = Draw_PicFromWad(va("inva%i_rlaunch", i + 1));
		sb_weapons[2 + i][5] = Draw_PicFromWad(va("inva%i_srlaunch", i + 1));
		sb_weapons[2 + i][6] = Draw_PicFromWad(va("inva%i_lightng", i + 1));
	}

	sb_ammo[0] = Draw_PicFromWad("sb_shells");
	sb_ammo[1] = Draw_PicFromWad("sb_nails");
	sb_ammo[2] = Draw_PicFromWad("sb_rocket");
	sb_ammo[3] = Draw_PicFromWad("sb_cells");

	sb_armor[0] = Draw_PicFromWad("sb_armor1");
	sb_armor[1] = Draw_PicFromWad("sb_armor2");
	sb_armor[2] = Draw_PicFromWad("sb_armor3");

	sb_items[0] = Draw_PicFromWad("sb_key1");
	sb_items[1] = Draw_PicFromWad("sb_key2");
	sb_items[2] = Draw_PicFromWad("sb_invis");
	sb_items[3] = Draw_PicFromWad("sb_invuln");
	sb_items[4] = Draw_PicFromWad("sb_suit");
	sb_items[5] = Draw_PicFromWad("sb_quad");

	sb_sigil[0] = Draw_PicFromWad("sb_sigil1");
	sb_sigil[1] = Draw_PicFromWad("sb_sigil2");
	sb_sigil[2] = Draw_PicFromWad("sb_sigil3");
	sb_sigil[3] = Draw_PicFromWad("sb_sigil4");

	sb_faces[4][0] = Draw_PicFromWad("face1");
	sb_faces[4][1] = Draw_PicFromWad("face_p1");
	sb_faces[3][0] = Draw_PicFromWad("face2");
	sb_faces[3][1] = Draw_PicFromWad("face_p2");
	sb_faces[2][0] = Draw_PicFromWad("face3");
	sb_faces[2][1] = Draw_PicFromWad("face_p3");
	sb_faces[1][0] = Draw_PicFromWad("face4");
	sb_faces[1][1] = Draw_PicFromWad("face_p4");
	sb_faces[0][0] = Draw_PicFromWad("face5");
	sb_faces[0][1] = Draw_PicFromWad("face_p5");

	sb_face_invis = Draw_PicFromWad("face_invis");
	sb_face_invuln = Draw_PicFromWad("face_invul2");
	sb_face_invis_invuln = Draw_PicFromWad("face_inv2");
	sb_face_quad = Draw_PicFromWad("face_quad");

	sb_sbar = Draw_PicFromWad("sbar");
	sb_ibar = Draw_PicFromWad("ibar");
	sb_scorebar = Draw_PicFromWad("scorebar");

	//MED 01/04/97 added new hipnotic weapons
	if (hipnotic)
	{
		hsb_weapons[0][0] = Draw_PicFromWad("inv_laser");
		hsb_weapons[0][1] = Draw_PicFromWad("inv_mjolnir");
		hsb_weapons[0][2] = Draw_PicFromWad("inv_gren_prox");
		hsb_weapons[0][3] = Draw_PicFromWad("inv_prox_gren");
		hsb_weapons[0][4] = Draw_PicFromWad("inv_prox");

		hsb_weapons[1][0] = Draw_PicFromWad("inv2_laser");
		hsb_weapons[1][1] = Draw_PicFromWad("inv2_mjolnir");
		hsb_weapons[1][2] = Draw_PicFromWad("inv2_gren_prox");
		hsb_weapons[1][3] = Draw_PicFromWad("inv2_prox_gren");
		hsb_weapons[1][4] = Draw_PicFromWad("inv2_prox");

		for (int i = 0; i < 5; i++)
		{
			hsb_weapons[2 + i][0] = Draw_PicFromWad(va("inva%i_laser", i + 1));
			hsb_weapons[2 + i][1] = Draw_PicFromWad(va("inva%i_mjolnir", i + 1));
			hsb_weapons[2 + i][2] = Draw_PicFromWad(va("inva%i_gren_prox", i + 1));
			hsb_weapons[2 + i][3] = Draw_PicFromWad(va("inva%i_prox_gren", i + 1));
			hsb_weapons[2 + i][4] = Draw_PicFromWad(va("inva%i_prox", i + 1));
		}

		hsb_items[0] = Draw_PicFromWad("sb_wsuit");
		hsb_items[1] = Draw_PicFromWad("sb_eshld");
	}

	if (rogue)
	{
		rsb_invbar[0] = Draw_PicFromWad("r_invbar1");
		rsb_invbar[1] = Draw_PicFromWad("r_invbar2");

		rsb_weapons[0] = Draw_PicFromWad("r_lava");
		rsb_weapons[1] = Draw_PicFromWad("r_superlava");
		rsb_weapons[2] = Draw_PicFromWad("r_gren");
		rsb_weapons[3] = Draw_PicFromWad("r_multirock");
		rsb_weapons[4] = Draw_PicFromWad("r_plasma");

		rsb_items[0] = Draw_PicFromWad("r_shield1");
		rsb_items[1] = Draw_PicFromWad("r_agrav1");

		// PGM 01/19/97 - team color border
		rsb_teambord = Draw_PicFromWad("r_teambord");

		rsb_ammo[0] = Draw_PicFromWad("r_ammolava");
		rsb_ammo[1] = Draw_PicFromWad("r_ammomulti");
		rsb_ammo[2] = Draw_PicFromWad("r_ammoplasma");
	}
}

void Sbar_Init(void)
{
	Cmd_AddCommand("+showscores", Sbar_ShowScores);
	Cmd_AddCommand("-showscores", Sbar_DontShowScores);

	Sbar_LoadPics();
}
