/*
 * Status bar code
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
#ifdef PSP
#include <pspgu.h>
#endif
cvar_t	pq_teamscores = {"pq_teamscores", "1", false}; // JPG - show teamscores
cvar_t	pq_timer = {"pq_timer", "1", false}; // JPG - show timer
cvar_t	pq_scoreboard_pings = {"pq_scoreboard_pings", "1", false};	// JPG - show ping times in the scoreboard
cvar_t  cl_sbar = {"cl_sbar", "1", true}; // Baker 3.97: transparent sbar capability
//cvar_t  cl_scoreboardback = {"cl_scoreboardback", "0", true}; // Baker 3.99n: "easy read scoreboard" derived from Qrack
cvar_t  cl_scoreboard_clean = {"cl_scoreboard_clean", "0", true}; // Baker 3.99q: idea from FitzQuake's don't draw centerprint while paused

void SBAR_cl_sbar_f (void) {
	// Baker 3.97: if transparent sbar is turned on/off
	// we need a vid.recalc
	if (host_initialized)
		vid.recalc_refdef = 1;
}


int			sb_updates;		// if >= vid.numpages, no update needed

#define STAT_MINUS		10	// num frame for '-' stats digit
qpic_t		*sb_nums[2][11];
qpic_t		*sb_colon, *sb_slash;
qpic_t		*sb_ibar;
qpic_t		*sb_sbar;
qpic_t		*sb_scorebar;

qpic_t      *sb_weapons[7][8];   // 0 is active, 1 is owned, 2-5 are flashes
qpic_t      *sb_ammo[4];
qpic_t		*sb_sigil[4];
qpic_t		*sb_armor[3];
qpic_t		*sb_items[32];

qpic_t	*sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
							// 0 is static, 1 is temporary animation
qpic_t	*sb_face_invis;
qpic_t	*sb_face_quad;
qpic_t	*sb_face_invuln;
qpic_t	*sb_face_invis_invuln;

qboolean	sb_showscores;

int			sb_lines;			// scan lines to draw

qpic_t      *rsb_invbar[2];
qpic_t      *rsb_weapons[5];
qpic_t      *rsb_items[2];
qpic_t      *rsb_ammo[3];
qpic_t      *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
qpic_t      *hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
int         hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};
//MED 01/04/97 added hipnotic items array
qpic_t      *hsb_items[2];

void Sbar_MiniDeathmatchOverlay (void);
void Sbar_DeathmatchOverlay (void);
void M_DrawPic (int x, int y, qpic_t *pic);

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores_f (void)
{
	if (sb_showscores)
		return;
	sb_showscores = true;
	sb_updates = 0;
}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores_f (void)
{
	sb_showscores = false;
	sb_updates = 0;
}

/*
===============
Sbar_Changed
===============
*/
void Sbar_Changed (void)
{
	sb_updates = 0;	// update next frame
}

/*
===============
Sbar_LoadPics -- johnfitz -- load all the sbar pics
===============
*/
void Sbar_LoadPics (void)
{
	int		i;

	for (i=0 ; i<10 ; i++)
	{
		sb_nums[0][i] = Draw_PicFromWad (va("num_%i",i));
		sb_nums[1][i] = Draw_PicFromWad (va("anum_%i",i));
	}

	sb_nums[0][10] = Draw_PicFromWad ("num_minus");
	sb_nums[1][10] = Draw_PicFromWad ("anum_minus");

	sb_colon = Draw_PicFromWad ("num_colon");
	sb_slash = Draw_PicFromWad ("num_slash");

	sb_weapons[0][0] = Draw_PicFromWad ("inv_shotgun");
	sb_weapons[0][1] = Draw_PicFromWad ("inv_sshotgun");
	sb_weapons[0][2] = Draw_PicFromWad ("inv_nailgun");
	sb_weapons[0][3] = Draw_PicFromWad ("inv_snailgun");
	sb_weapons[0][4] = Draw_PicFromWad ("inv_rlaunch");
	sb_weapons[0][5] = Draw_PicFromWad ("inv_srlaunch");
	sb_weapons[0][6] = Draw_PicFromWad ("inv_lightng");

	sb_weapons[1][0] = Draw_PicFromWad ("inv2_shotgun");
	sb_weapons[1][1] = Draw_PicFromWad ("inv2_sshotgun");
	sb_weapons[1][2] = Draw_PicFromWad ("inv2_nailgun");
	sb_weapons[1][3] = Draw_PicFromWad ("inv2_snailgun");
	sb_weapons[1][4] = Draw_PicFromWad ("inv2_rlaunch");
	sb_weapons[1][5] = Draw_PicFromWad ("inv2_srlaunch");
	sb_weapons[1][6] = Draw_PicFromWad ("inv2_lightng");

	for (i=0 ; i<5 ; i++)
	{
		sb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_shotgun",i+1));
		sb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_sshotgun",i+1));
		sb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_nailgun",i+1));
		sb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_snailgun",i+1));
		sb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_rlaunch",i+1));
		sb_weapons[2+i][5] = Draw_PicFromWad (va("inva%i_srlaunch",i+1));
		sb_weapons[2+i][6] = Draw_PicFromWad (va("inva%i_lightng",i+1));
	}

	sb_ammo[0] = Draw_PicFromWad ("sb_shells");
	sb_ammo[1] = Draw_PicFromWad ("sb_nails");
	sb_ammo[2] = Draw_PicFromWad ("sb_rocket");
	sb_ammo[3] = Draw_PicFromWad ("sb_cells");

	sb_armor[0] = Draw_PicFromWad ("sb_armor1");
	sb_armor[1] = Draw_PicFromWad ("sb_armor2");
	sb_armor[2] = Draw_PicFromWad ("sb_armor3");

	sb_items[0] = Draw_PicFromWad ("sb_key1");
	sb_items[1] = Draw_PicFromWad ("sb_key2");
	sb_items[2] = Draw_PicFromWad ("sb_invis");
	sb_items[3] = Draw_PicFromWad ("sb_invuln");
	sb_items[4] = Draw_PicFromWad ("sb_suit");
	sb_items[5] = Draw_PicFromWad ("sb_quad");

	sb_sigil[0] = Draw_PicFromWad ("sb_sigil1");
	sb_sigil[1] = Draw_PicFromWad ("sb_sigil2");
	sb_sigil[2] = Draw_PicFromWad ("sb_sigil3");
	sb_sigil[3] = Draw_PicFromWad ("sb_sigil4");

	sb_faces[4][0] = Draw_PicFromWad ("face1");
	sb_faces[4][1] = Draw_PicFromWad ("face_p1");
	sb_faces[3][0] = Draw_PicFromWad ("face2");
	sb_faces[3][1] = Draw_PicFromWad ("face_p2");
	sb_faces[2][0] = Draw_PicFromWad ("face3");
	sb_faces[2][1] = Draw_PicFromWad ("face_p3");
	sb_faces[1][0] = Draw_PicFromWad ("face4");
	sb_faces[1][1] = Draw_PicFromWad ("face_p4");
	sb_faces[0][0] = Draw_PicFromWad ("face5");
	sb_faces[0][1] = Draw_PicFromWad ("face_p5");

	sb_face_invis = Draw_PicFromWad ("face_invis");
	sb_face_invuln = Draw_PicFromWad ("face_invul2");
	sb_face_invis_invuln = Draw_PicFromWad ("face_inv2");
	sb_face_quad = Draw_PicFromWad ("face_quad");


	sb_sbar = Draw_PicFromWad ("sbar");
	sb_ibar = Draw_PicFromWad ("ibar");
	sb_scorebar = Draw_PicFromWad ("scorebar");

//MED 01/04/97 added new hipnotic weapons
	if (hipnotic)
	{
	  hsb_weapons[0][0] = Draw_PicFromWad ("inv_laser");
	  hsb_weapons[0][1] = Draw_PicFromWad ("inv_mjolnir");
	  hsb_weapons[0][2] = Draw_PicFromWad ("inv_gren_prox");
	  hsb_weapons[0][3] = Draw_PicFromWad ("inv_prox_gren");
	  hsb_weapons[0][4] = Draw_PicFromWad ("inv_prox");

	  hsb_weapons[1][0] = Draw_PicFromWad ("inv2_laser");
	  hsb_weapons[1][1] = Draw_PicFromWad ("inv2_mjolnir");
	  hsb_weapons[1][2] = Draw_PicFromWad ("inv2_gren_prox");
	  hsb_weapons[1][3] = Draw_PicFromWad ("inv2_prox_gren");
	  hsb_weapons[1][4] = Draw_PicFromWad ("inv2_prox");

	  for (i=0 ; i<5 ; i++)
	  {
		 hsb_weapons[2+i][0] = Draw_PicFromWad (va("inva%i_laser",i+1));
		 hsb_weapons[2+i][1] = Draw_PicFromWad (va("inva%i_mjolnir",i+1));
		 hsb_weapons[2+i][2] = Draw_PicFromWad (va("inva%i_gren_prox",i+1));
		 hsb_weapons[2+i][3] = Draw_PicFromWad (va("inva%i_prox_gren",i+1));
		 hsb_weapons[2+i][4] = Draw_PicFromWad (va("inva%i_prox",i+1));
	  }

	  hsb_items[0] = Draw_PicFromWad ("sb_wsuit");
	  hsb_items[1] = Draw_PicFromWad ("sb_eshld");
	}

	if (rogue)
	{
		rsb_invbar[0] = Draw_PicFromWad ("r_invbar1");
		rsb_invbar[1] = Draw_PicFromWad ("r_invbar2");

		rsb_weapons[0] = Draw_PicFromWad ("r_lava");
		rsb_weapons[1] = Draw_PicFromWad ("r_superlava");
		rsb_weapons[2] = Draw_PicFromWad ("r_gren");
		rsb_weapons[3] = Draw_PicFromWad ("r_multirock");
		rsb_weapons[4] = Draw_PicFromWad ("r_plasma");

		rsb_items[0] = Draw_PicFromWad ("r_shield1");
        rsb_items[1] = Draw_PicFromWad ("r_agrav1");

// PGM 01/19/97 - team color border
        rsb_teambord = Draw_PicFromWad ("r_teambord");
// PGM 01/19/97 - team color border

		rsb_ammo[0] = Draw_PicFromWad ("r_ammolava");
		rsb_ammo[1] = Draw_PicFromWad ("r_ammomulti");
		rsb_ammo[2] = Draw_PicFromWad ("r_ammoplasma");
	}
}

/*
===============
Sbar_Init -- johnfitz -- rewritten
===============
*/

extern cvar_t cl_scoreboard_clean;
void Sbar_Init (void)
{
	Cmd_AddCommand ("+showscores", Sbar_ShowScores_f);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores_f);

	Cvar_RegisterVariable (&pq_teamscores, NULL); // JPG - status bar teamscores
	Cvar_RegisterVariable (&pq_timer, NULL); // JPG - status bar timer
	Cvar_RegisterVariable (&pq_scoreboard_pings, NULL); // JPG - ping times in the scoreboard

	Cvar_RegisterVariable (&cl_sbar, SBAR_cl_sbar_f);
	Cvar_RegisterVariable (&cl_scoreboard_clean, NULL);

	Sbar_LoadPics ();
}

//=============================================================================

// drawing routines are relative to the status bar location

/*
=============
Sbar_DrawPic
=============
*/
void Sbar_DrawPic (int x, int y, qpic_t *pic)
{
	if (cl.gametype == GAME_DEATHMATCH)
		Draw_Pic (x /* + ((vid.width - 320)>>1)*/, y + (vid.height-SBAR_HEIGHT), pic);
	else
		Draw_Pic (x + ((vid.width - 320)>>1), y + (vid.height-SBAR_HEIGHT), pic);
}

/*
=============
Sbar_DrawSubPic
=============
JACK: Draws a portion of the picture in the status bar.
*/
void Sbar_DrawSubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
{
	Draw_SubPic (x, y + (vid.height-SBAR_HEIGHT), pic, srcx, srcy, width, height);
}

/*
=============
Sbar_DrawTransPic
=============
*/

#ifdef SUPPORTS_2DPICS_ALPHA
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha);
void Sbar_DrawAlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	if (cl.gametype == GAME_DEATHMATCH)
		Draw_AlphaPic (x /*+ ((vid.width - 320)>>1)*/, y + (vid.height-SBAR_HEIGHT), pic, alpha);
	else
		Draw_AlphaPic (x + ((vid.width - 320)>>1), y + (vid.height-SBAR_HEIGHT), pic, alpha);
}
#endif
void Sbar_DrawTransPic (int x, int y, qpic_t *pic)
{
	if (cl.gametype == GAME_DEATHMATCH)
		Draw_TransPic (x /*+ ((vid.width - 320)>>1)*/, y + (vid.height-SBAR_HEIGHT), pic);
	else
		Draw_TransPic (x + ((vid.width - 320)>>1), y + (vid.height-SBAR_HEIGHT), pic);
}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (int x, int y, int num)
{
	if (cl.gametype == GAME_DEATHMATCH)
		Draw_Character ( x /*+ ((vid.width - 320)>>1) */ + 4 , y + vid.height-SBAR_HEIGHT, num);
	else
		Draw_Character ( x + ((vid.width - 320)>>1) + 4 , y + vid.height-SBAR_HEIGHT, num);
}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (int x, int y, char *str)
{
	if (cl.gametype == GAME_DEATHMATCH)
		Draw_String (x /*+ ((vid.width - 320)>>1)*/, y+ vid.height-SBAR_HEIGHT, str);
	else
		Draw_String (x + ((vid.width - 320)>>1), y+ vid.height-SBAR_HEIGHT, str);
}

/*
=============
Sbar_itoa
=============
*/
int Sbar_itoa (int num, char *buf)
{
	char	*str;
	int	pow10, dig;

	str = buf;

	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
	;

	do {
		pow10 /= 10;
		dig = num/pow10;
		*str++ = '0'+dig;
		num -= dig*pow10;
	} while (pow10 != 1);

	*str = 0;

	return str-buf;
}

/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{
	char	str[12], *ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Sbar_DrawTransPic (x,y,sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

//=============================================================================

int		fragsort[MAX_SCOREBOARD];

char	scoreboardtext[MAX_SCOREBOARD][20];
int		scoreboardtop[MAX_SCOREBOARD];
int		scoreboardbottom[MAX_SCOREBOARD];
int		scoreboardcount[MAX_SCOREBOARD];
int		scoreboardlines;

/*
===============
Sbar_SortFrags
===============
*/
void Sbar_SortFrags (void)
{
	int		i, j, k;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<cl.maxclients ; i++)
	{
		if (cl.scores[i].name[0])
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.scores[fragsort[j]].frags < cl.scores[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
}

/* JPG - added this for teamscores in status bar
==================
Sbar_SortTeamFrags
==================
*/
void Sbar_SortTeamFrags (void)
{
	int		i, j, k;

// sort by frags
	scoreboardlines = 0;
	for (i=0 ; i<14 ; i++)
	{
		if (cl.teamscores[i].colors)
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.teamscores[fragsort[j]].frags < cl.teamscores[fragsort[j+1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j+1];
				fragsort[j+1] = k;
			}
}

int	Sbar_ColorForMap (int m)
{
	return m < 128 ? m + 8 : m + 8;
}

/*
===============
Sbar_UpdateScoreboard
===============
*/
void Sbar_UpdateScoreboard (void)
{
	int		i, k, top, bottom;
	scoreboard_t	*s;

	Sbar_SortFrags ();

// draw the text
	memset (scoreboardtext, 0, sizeof(scoreboardtext));

	for (i=0 ; i<scoreboardlines; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		SNPrintf (scoreboardtext[i], sizeof(scoreboardtext[i]), "%3i %s", s->frags, s->name);

		top = s->colors & 0xf0;
		bottom = (s->colors & 15) <<4;
		scoreboardtop[i] = Sbar_ColorForMap (top);
		scoreboardbottom[i] = Sbar_ColorForMap (bottom);
	}
}


/*
===============
Sbar_SoloScoreboard
===============
*/
extern cvar_t show_speed;
void Sbar_SoloScoreboard (void)
{
	char	str[80];
	int		len, minutes, seconds, tens, units;
	int		current_skill;


	if ((!cls.demoplayback) && cl.gametype != GAME_DEATHMATCH) // Baker 3.75 - do not display skill if demo is playing, because we don't know!
	{
		current_skill = (int)(skill.value);
		if (current_skill < 0)
			current_skill = 0;
		if (current_skill > 3)
			current_skill = 3;

		switch (current_skill)
		{
		case 0:
			SNPrintf (str, sizeof(str), "skill: %i", current_skill);
			break;
		case 1:
			SNPrintf (str, sizeof(str), "skill: %i", current_skill);
			break;
		case 2:
			SNPrintf (str, sizeof(str), "skill: %i", current_skill);
			break;
		case 3:
			SNPrintf (str, sizeof(str), "skill: %i", current_skill);
			break;
		}

		Draw_String (vid.width - (24 + strlen(str) * 8), 8 + (pq_drawfps.value ? 8:0) + (show_speed.value ? 8:0), str);
	}

	SNPrintf (str, sizeof(str),"Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
	Sbar_DrawString (8, 4, str);

	SNPrintf (str, sizeof(str),"Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
	Sbar_DrawString (8, 12, str);

// time
	minutes = cl.time / 60;
	seconds = cl.time - 60*minutes;
	tens = seconds / 10;
	units = seconds - 10*tens;
	SNPrintf (str, sizeof(str),"Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

// draw level name
	len = strlen (cl.levelname);
	Sbar_DrawString (232 - len*4, 12, cl.levelname);
}

/*
===============
Sbar_DrawScoreboard
===============
*/
void Sbar_DrawScoreboard (void)
{
	Sbar_SoloScoreboard ();
	if (cl.gametype == GAME_DEATHMATCH)
		Sbar_DeathmatchOverlay ();
}

//=============================================================================

/*
===============
Sbar_DrawInventory
===============
*/
void Sbar_DrawInventory (void)
{
	int		i, flashon, ystart;
	char	num[6];
	float	time;
	qboolean	headsup;

	// Baker: we only draw the sbar if 0 and viewsize is 100+
	headsup = (cl_sbar.value == 0 && scr_viewsize.value>=100);

	if (!headsup) {
		if (rogue) {
			if (cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
#ifdef SUPPORTS_2DPICS_ALPHA
				Sbar_DrawAlphaPic (0, -24, rsb_invbar[0], cl_sbar.value);
#else
				Sbar_DrawPic (0, -24, rsb_invbar[0]);
#endif
			else
#ifdef SUPPORTS_2DPICS_ALPHA
				Sbar_DrawAlphaPic (0, -24, rsb_invbar[1], cl_sbar.value);
#else
				Sbar_DrawPic (0, -24, rsb_invbar[1]);
#endif

		} else {

	#ifdef SUPPORTS_2DPICS_ALPHA
			Sbar_DrawAlphaPic (0, -24, sb_ibar, cl_sbar.value);
	#else
			Sbar_DrawPic (0, -24, sb_ibar);
	#endif
		}
	}

// weapons
	ystart = hipnotic ? -100 : -68;
	for (i=0 ; i<7 ; i++)
	{
		if (cl.items & (IT_SHOTGUN<<i) )
		{
			time = cl.item_gettime[i];
			flashon = (int)((cl.time - time)*10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10)
				flashon = (cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i)) ? 1 : 0;
			else
				flashon = (flashon%5) + 2;

		if (headsup) {
			if (i || vid.height>200)
					Sbar_DrawSubPic ((vid.width-24), ystart - (7-i)*16, sb_weapons[flashon][i], 0, 0, 24, 16);
		} else {
#ifdef SUPPORTS_2DPICS_ALPHA
         Sbar_DrawAlphaPic (i*24, -16, sb_weapons[flashon][i], cl_sbar.value);
#else
		 Sbar_DrawPic (i*24, -16, sb_weapons[flashon][i]);
#endif
			}

			if (flashon > 1)
				sb_updates = 0;		// force update to remove flash
		}
	}

// MED 01/04/97
// hipnotic weapons
    if (hipnotic)
    {
      int grenadeflashing=0;

      for (i=0 ; i<4 ; i++)
      {
         if (cl.items & (1<<hipweapons[i]) )
         {
            time = cl.item_gettime[hipweapons[i]];
            flashon = (int)((cl.time - time)*10);
				if (flashon < 0)
					flashon = 0;
            if (flashon >= 10)
					flashon = (cl.stats[STAT_ACTIVEWEAPON] == (1 << hipweapons[i])) ? 1 : 0;
            else
               flashon = (flashon%5) + 2;

            // check grenade launcher
            if (i == 2)
            {
               if ((cl.items & HIT_PROXIMITY_GUN) && flashon)
               {
                     grenadeflashing = 1;
						if (headsup)
						{
							if (i || vid.height>200)
								Sbar_DrawSubPic ((vid.width-24), ystart - 48, hsb_weapons[flashon][2], 0, 0, 24, 16);
						}
						else
						{
                     Sbar_DrawPic (96, -16, hsb_weapons[flashon][2]);
						}
                  }
               }
            else if (i==3)
            {
               if (cl.items & (IT_SHOTGUN<<4))
               {
						if (!grenadeflashing)
						{
							if (flashon)
							{
								if (headsup)
								{
									if (i || vid.height>200)
										Sbar_DrawSubPic ((vid.width-24), ystart - 48, hsb_weapons[flashon][3], 0, 0, 24, 16);
								}
								else
                  {
                     Sbar_DrawPic (96, -16, hsb_weapons[flashon][3]);
                  }
							}
							else
							{
								if (headsup)
								{
									if (i || vid.height>200)
										Sbar_DrawSubPic ((vid.width-24), ystart - 48, hsb_weapons[0][3], 0, 0, 24, 16);
								}
								else
                  {
                     Sbar_DrawPic (96, -16, hsb_weapons[0][3]);
                  }
               }
						}
					}
					else
					{
						if (headsup)
						{
							if (i || vid.height>200)
								Sbar_DrawSubPic ((vid.width-24), ystart - 48, hsb_weapons[flashon][4], 0, 0, 24, 16);
						}
               else
						{
                  Sbar_DrawPic (96, -16, hsb_weapons[flashon][4]);
            }
					}
				}
            else
				{
					if (headsup) {
						if (i || vid.height>200)
							Sbar_DrawSubPic ((vid.width-24), ystart + i*16, hsb_weapons[flashon][i], 0, 0, 24, 16);
					} else {
               Sbar_DrawPic (176 + (i*24), -16, hsb_weapons[flashon][i]);
					}
				}
            if (flashon > 1)
               sb_updates = 0;      // force update to remove flash
         }
      }
    }

// rogue weapons
	if (rogue)
	{	// check for powered up weapon
		if ( cl.stats[STAT_ACTIVEWEAPON] >= RIT_LAVA_NAILGUN )
		{
			for (i=0;i<5;i++)
			{
				if (cl.stats[STAT_ACTIVEWEAPON] == (RIT_LAVA_NAILGUN << i))
					Sbar_DrawPic ((i+2)*24, -16, rsb_weapons[i]);
			}
		}
	}

// ammo counts
	for (i=0 ; i<4 ; i++)
	{
		SNPrintf (num, sizeof(num), "%3i",cl.stats[STAT_SHELLS+i] );
		if (headsup) {
			Sbar_DrawSubPic ((vid.width-42), -24 - (4-i)*11, sb_ibar, 3+(i*48), 0, 42, 11);
			if (num[0] != ' ')
				Draw_Character ((vid.width - 35), vid.height-SBAR_HEIGHT-24 - (4-i)*11, 18 + num[0] - '0');
			if (num[1] != ' ')
				Draw_Character ((vid.width - 27), vid.height-SBAR_HEIGHT-24 - (4-i)*11, 18 + num[1] - '0');
			if (num[2] != ' ')
				Draw_Character ((vid.width - 19), vid.height-SBAR_HEIGHT-24 - (4-i)*11, 18 + num[2] - '0');
		} else {
			if (num[0] != ' ')
				Sbar_DrawCharacter ( (6*i+1)*8 - 2, -24, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter ( (6*i+2)*8 - 2, -24, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter ( (6*i+3)*8 - 2, -24, 18 + num[2] - '0');
		}
	}

	flashon = 0;

   // items
   for (i=0 ; i<6 ; i++) {
	      if (cl.items & (1<<(17+i)))
	      {
	         time = cl.item_gettime[17+i];
	         if (time && time > cl.time - 2 && flashon )
	         {  // flash frame
	            sb_updates = 0;
	         }
	         else
	         { //MED 01/04/97 changed keys
	            if (!hipnotic || (i>1)) {
	               Sbar_DrawPic (192 + i*16, -16, sb_items[i]);
	         	}
	         }
	         if (time && time > cl.time - 2)
	            sb_updates = 0;
	      }
	}

   //MED 01/04/97 added hipnotic items
   // hipnotic items
   if (hipnotic) {
      for (i=0 ; i<2 ; i++) {
         if (cl.items & (1<<(24+i))) {
            time = cl.item_gettime[24+i];
			if (time && time > cl.time - 2 && flashon)	// flash frame
               sb_updates = 0;
            else
               Sbar_DrawPic (288 + i*16, -16, hsb_items[i]);

            if (time && time > cl.time - 2)
               sb_updates = 0;
         }
   }
	}

// rogue items
	if (rogue)
	{
	// new rogue items
		for (i=0 ; i<2 ; i++)
		{
			if (cl.items & (1<<(29+i)))
			{
				time = cl.item_gettime[29+i];
				if (time && time > cl.time - 2 && flashon)	// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (288 + i*16, -16, rsb_items[i]);
				if (time &&	time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}
	else
	{
	// sigils
		for (i=0 ; i<4 ; i++)
		{
			if (cl.items & (1<<(28+i)))
			{
				time = cl.item_gettime[28+i];
				if (time && time > cl.time - 2 && flashon)	// flash frame
					sb_updates = 0;
				else
					Sbar_DrawPic (320-32 + i*8, -16, sb_sigil[i]);
				if (time &&	time > cl.time - 2)
					sb_updates = 0;
			}
		}
	}
}

//=============================================================================

/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (void)
{
	int				i, k, numscores;
	int				top, bottom;
	int				x, y, f;
	int				xofs;
	char			num[12];
	scoreboard_t	*s;
	int				teamscores, colors, ent, minutes, seconds, mask; // JPG - added these
	int				match_time; // JPG - added this

	// JPG - check to see if we should sort teamscores instead
	teamscores = pq_teamscores.value && cl.teamgame;
	if (teamscores)
		Sbar_SortTeamFrags();
	else
		Sbar_SortFrags ();

// draw the text
	numscores = scoreboardlines <= 4 ? scoreboardlines : 4;

	x = 23;
	if (cl.gametype == GAME_DEATHMATCH)
		xofs = 0;
	else
		xofs = (vid.width - 320)>>1;
	y = vid.height - SBAR_HEIGHT - 23;

	// JPG - check to see if we need to draw the timer
	if (pq_timer.value && (cl.minutes != 255))
	{
		if (numscores > 2)
			numscores = 2;
		mask = 0;
		if (cl.minutes == 254)
		{
			strcpy(num, "    SD");
			mask = 128;
		}
		else if (cl.minutes || cl.seconds)
		{
			if (cl.seconds >= 128)
				SNPrintf (num, sizeof(num), " -0:%02d", cl.seconds - 128);
			else
			{
				if (cl.match_pause_time)
					match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.match_pause_time - cl.last_match_time));
				else
					match_time = ceil(60.0 * cl.minutes + cl.seconds - (cl.time - cl.last_match_time));
				minutes = match_time / 60;
				seconds = match_time - 60 * minutes;
				SNPrintf (num, sizeof(num), "%3d:%02d", minutes, seconds);
				if (!minutes)
					mask = 128;
			}
		}
		else
		{
			minutes = cl.time / 60;
			seconds = cl.time - 60 * minutes;
			minutes = minutes & 511;
			SNPrintf (num, sizeof(num), "%3d:%02d", minutes, seconds);
		}

		for (i = 0 ; i < 6 ; i++)
			Sbar_DrawCharacter ((x+9+i)*8, -24, num[i] + mask);
	}

	for (i=0 ; i<numscores ; i++)
	{
		k = fragsort[i];

		// JPG - check for teamscores
		if (teamscores)
		{
			colors = cl.teamscores[k].colors;
			f = cl.teamscores[k].frags;
		}
		else
		{
			s = &cl.scores[k];
			if (!s->name[0])
				continue;
			colors = s->colors;
			f = s->frags;
		}

	// draw background
		top = colors & 0xf0;
		bottom = (colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill (xofs + x*8 + 10, y, 28, 4, top);
		Draw_Fill (xofs + x*8 + 10, y+4, 28, 3, bottom);

	// draw number
		SNPrintf (num, sizeof(num), "%3i",f);

		Sbar_DrawCharacter ( (x+1)*8 , -24, num[0]);
		Sbar_DrawCharacter ( (x+2)*8 , -24, num[1]);
		Sbar_DrawCharacter ( (x+3)*8 , -24, num[2]);

		// JPG - check for self's team
		ent = cl.viewentity - 1;

/*
		// Baker 3.99n: show color
//		if (cl_colorshow.value && cl.maxclients>1) {
//			Draw_AlphaFill	(12, 12, 16, 16, Sbar_ColorForMap((cl.scores[ent].colors & 15) & 15<<4), 0.8);	// Baker 3.99n: display pants color in top/left
*/

		if ((teamscores && ((colors & 15) == (cl.scores[ent].colors & 15))) || (!teamscores && (k == ent)))
		{
			Sbar_DrawCharacter (x*8+2, -24, 16);
			Sbar_DrawCharacter ( (x+4)*8-4, -24, 17);
		}
		x+=4;
	}
}

//=============================================================================

/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (void)
{
	int		f, anim;

// PGM 01/19/97 - team color drawing
// PGM 03/02/97 - fixed so color swatch only appears in CTF modes
	if (rogue && (cl.maxclients != 1) && (teamplay.value>3) && (teamplay.value<7))
	{
		int		top, bottom, xofs;
		char			num[12];
		scoreboard_t	*s;

		s = &cl.scores[cl.viewentity - 1];
		// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		xofs = (cl.gametype == GAME_DEATHMATCH) ? 113 : ((vid.width - 320) >> 1) + 113;

		Sbar_DrawPic (112, 0, rsb_teambord);
		Draw_Fill (xofs, vid.height-SBAR_HEIGHT+3, 22, 9, top);
		Draw_Fill (xofs, vid.height-SBAR_HEIGHT+12, 22, 9, bottom);

		// draw number
		f = s->frags;
		SNPrintf (num, sizeof(num), "%3i",f);

		if (top==8)
		{
			if (num[0] != ' ')
				Sbar_DrawCharacter(109, 3, 18 + num[0] - '0');
			if (num[1] != ' ')
				Sbar_DrawCharacter(116, 3, 18 + num[1] - '0');
			if (num[2] != ' ')
				Sbar_DrawCharacter(123, 3, 18 + num[2] - '0');
		}
		else
		{
			Sbar_DrawCharacter ( 109, 3, num[0]);
			Sbar_DrawCharacter ( 116, 3, num[1]);
			Sbar_DrawCharacter ( 123, 3, num[2]);
		}

		return;
	}
// PGM 01/19/97 - team color drawing

	if ( (cl.items & (IT_INVISIBILITY | IT_INVULNERABILITY) ) == (IT_INVISIBILITY | IT_INVULNERABILITY) )
	{
		Sbar_DrawPic (112, 0, sb_face_invis_invuln);
		return;
	}
	if (cl.items & IT_QUAD)
	{
		Sbar_DrawPic (112, 0, sb_face_quad );
		return;
	}
	if (cl.items & IT_INVISIBILITY)
	{
		Sbar_DrawPic (112, 0, sb_face_invis );
		return;
	}
	if (cl.items & IT_INVULNERABILITY)
	{
		Sbar_DrawPic (112, 0, sb_face_invuln);
		return;
	}

		f = cl.stats[STAT_HEALTH] / 20;
	f = CLAMP (0, f, 4);

	if (cl.time <= cl.faceanimtime)
	{
		anim = 1;
		sb_updates = 0;		// make sure the anim gets drawn over
	}
	else
	{
		anim = 0;
	}

	Sbar_DrawPic (112, 0, sb_faces[f][anim]);
}

// Begin D3DQuake
int gNoStatusBar;
// End D3DQuake
/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (void)
{
//	qboolean	headsup;	// joe
// Begin D3DQuake
	if ( gNoStatusBar ) return;
// End D3DQuake

//	headsup = (cl_sbar.value == 0 && scr_viewsize.value >= 100);
//	if ((sb_updates >= vid.numpages) && !headsup)
//		return;

	if (scr_con_current == vid.height)
		return;		// console is full screen



	if (cl_sbar.value >= 1.0)
	{
		// Baker: 3.97 We don't do this with transparent sbar
		if (sb_updates >= vid.numpages && !gl_clear.value) //Baker 3.60 - gl_clear fix with sbar
			return;
	}

//	scr_copyeverything = 1;
	sb_updates++;

	if (cl_sbar.value >=1.0 || scr_viewsize.value < 100.0)
	{
		// Baker: For viewsize < 100 clear the tile
		if (sb_lines && vid.width > 320)
		{
			Draw_TileClear (0, vid.height - sb_lines, vid.width, sb_lines);
			//Con_Printf("Clearing sbar\n");
		}
	}

	if (sb_lines > 24)
	{
		Sbar_DrawInventory ();
		if (cl.maxclients != 1)
			Sbar_DrawFrags ();
	}

	if (sb_showscores || cl.stats[STAT_HEALTH] <= 0)
	{
#ifdef SUPPORTS_2DPICS_ALPHA
		Sbar_DrawAlphaPic (0, 0, sb_scorebar, cl_sbar.value);
#else
		Sbar_DrawPic (0, 0, sb_scorebar);
#endif
		Sbar_DrawScoreboard ();
		sb_updates = 0;
	}
	else if (sb_lines)
	{
#ifdef SUPPORTS_2DPICS_ALPHA
		Sbar_DrawAlphaPic (0, 0, sb_sbar, cl_sbar.value);
#else
		if (cl_sbar.value || scr_viewsize.value < 100)
			Sbar_DrawPic (0, 0, sb_sbar);
#endif

   // keys (hipnotic only)
      //MED 01/04/97 moved keys here so they would not be overwritten
      if (hipnotic)
      {
         if (cl.items & IT_KEY1)
            Sbar_DrawPic (209, 3, sb_items[0]);
         if (cl.items & IT_KEY2)
            Sbar_DrawPic (209, 12, sb_items[1]);
      }

   // armor
		if (cl.items & IT_INVULNERABILITY)
		{
			Sbar_DrawNum (24, 0, 666, 3, 1);
			Sbar_DrawPic (0, 0, draw_disc);
		}
		else
		{
			Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);

			if (rogue)
			{
				if (cl.items & RIT_ARMOR3)
					Sbar_DrawPic (0, 0, sb_armor[2]);
				else if (cl.items & RIT_ARMOR2)
					Sbar_DrawPic (0, 0, sb_armor[1]);
				else if (cl.items & RIT_ARMOR1)
					Sbar_DrawPic (0, 0, sb_armor[0]);
			}
			else
			{
				if (cl.items & IT_ARMOR3)
					Sbar_DrawPic (0, 0, sb_armor[2]);
				else if (cl.items & IT_ARMOR2)
					Sbar_DrawPic (0, 0, sb_armor[1]);
				else if (cl.items & IT_ARMOR1)
					Sbar_DrawPic (0, 0, sb_armor[0]);
			}
		}

	// face
		Sbar_DrawFace ();

	// health
		Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	// ammo icon
		if (rogue)
		{
			if (cl.items & RIT_SHELLS)
				Sbar_DrawPic (224, 0, sb_ammo[0]);
			else if (cl.items & RIT_NAILS)
				Sbar_DrawPic (224, 0, sb_ammo[1]);
			else if (cl.items & RIT_ROCKETS)
				Sbar_DrawPic (224, 0, sb_ammo[2]);
			else if (cl.items & RIT_CELLS)
				Sbar_DrawPic (224, 0, sb_ammo[3]);
			else if (cl.items & RIT_LAVA_NAILS)
				Sbar_DrawPic (224, 0, rsb_ammo[0]);
			else if (cl.items & RIT_PLASMA_AMMO)
				Sbar_DrawPic (224, 0, rsb_ammo[1]);
			else if (cl.items & RIT_MULTI_ROCKETS)
				Sbar_DrawPic (224, 0, rsb_ammo[2]);
		}
		else
		{
			if (cl.items & IT_SHELLS)
				Sbar_DrawPic (224, 0, sb_ammo[0]);
			else if (cl.items & IT_NAILS)
				Sbar_DrawPic (224, 0, sb_ammo[1]);
			else if (cl.items & IT_ROCKETS)
				Sbar_DrawPic (224, 0, sb_ammo[2]);
			else if (cl.items & IT_CELLS)
				Sbar_DrawPic (224, 0, sb_ammo[3]);
		}

		Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
	}

	if (vid.width > 320) {
		if (cl.gametype == GAME_DEATHMATCH)
			Sbar_MiniDeathmatchOverlay ();
	}
}

//=============================================================================

/*
==================
Sbar_IntermissionNumber
==================
*/
void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{
	char	str[12], *ptr;
	int				l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l-digits);
	if (l < digits)
		x += (digits-l)*24;

	while (*ptr)
	{
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Draw_TransPic (x,y,sb_nums[color][frame]);
		x += 24;
		ptr++;
	}
}

/*
==================
Sbar_DeathmatchOverlay
==================
*/
void Sbar_DeathmatchOverlay (void)
{
	qpic_t			*pic;
	int				i, k, l, top, bottom, x, y, f;
	char			num[12];
	scoreboard_t	*s;
	int				j, ping; // JPG - added these

	// JPG 1.05 (rewrote this) - check to see if we need to update ping times
	if ((cl.last_ping_time < cl.time - 5) && pq_scoreboard_pings.value)
	{
		MSG_WriteByte (&cls.message, clc_stringcmd);
		SZ_Print (&cls.message, "ping\n");
		cl.last_ping_time = cl.time;
	}

//	scr_copyeverything = 1;
//	scr_fullupdate = 0;

	pic = Draw_CachePic ("gfx/ranking.lmp");
	M_DrawPic ((320-pic->width)/2, 8, pic);

// scores
	Sbar_SortFrags ();

// draw the text
	l = scoreboardlines;

	x = 80 + ((vid.width - 320)>>1);
	y = 40;

	ping = 0;  // JPG - this will tell us if some client's ping is showing

/*
#ifdef SUPPORTS_2DPICS_ALPHA
	if (cl_scoreboardback.value)
		Draw_AlphaFill	(x - (56+1), y - 1, (15+15)*8+2 , 10*16+2, 16, 0.9);	//inside
#endif
*/

	for (i=0 ; i<l ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill ( x, y, 40, 4, top);
		Draw_Fill ( x, y+4, 40, 4, bottom);

	// JPG - draw ping
		if (s->ping && pq_scoreboard_pings.value)
		{
			ping = 1;
			SNPrintf(num, sizeof(num), "%4d", s->ping);

			for (j = 0 ; j < 4 ; j++)
				Draw_Character(x-56+j*8, y, num[j]);
		}

	// draw number
		f = s->frags;
		SNPrintf (num, sizeof(num), "%3i",f);

		Draw_Character ( x+8 , y, num[0]);
		Draw_Character ( x+16 , y, num[1]);
		Draw_Character ( x+24 , y, num[2]);

		if (k == cl.viewentity - 1)
		{
			//Draw_Character ( x - 8, y, 12);
			Draw_Character(x, y, 16);		// JPG 3.00 from katua - draw [ ] around our score in the
			Draw_Character(x+32, y, 17);	// scoreboard overlay

		}

#if 0
{
	int				total;
	int				n, minutes, tens, units;

	// draw time
		total = cl.completed_time - s->entertime;
		minutes = (int)total/60;
		n = total - minutes*60;
		tens = n/10;
		units = n%10;

		SNPrintf (num, sizeof(num), "%3i:%i%i", minutes, tens, units);

		Draw_String ( x+48 , y, num);
}
#endif

	// draw name
		Draw_String (x+64, y, s->name);

		y += 10;
	}

// JPG - draw "ping"
	if (ping)
	{
		strcpy(num, "ping");
		for (i = 0 ; i < 4 ; i++)
			Draw_Character(x-56+i*8, 30, num[i]+128);
	}
}

/*
==================
Sbar_MiniDeathmatchOverlay
==================
*/
void Sbar_MiniDeathmatchOverlay (void)
{
	int		i, k, top, bottom, x, y, f, numlines;
	char			num[12];
	scoreboard_t	*s;

	if (vid.width < 512 || !sb_lines)
		return;

//	scr_copyeverything = 1;
//	scr_fullupdate = 0;

// scores
	Sbar_SortFrags ();

// draw the text
//	l = scoreboardlines;
	y = vid.height - sb_lines;
	numlines = sb_lines/8;
	if (numlines < 3)
		return;

	//find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == cl.viewentity - 1)
			break;

	i = (i == scoreboardlines) ? 0 : i - numlines/2;
	i = CLAMP (0, i, scoreboardlines - numlines);

	x = 324;
	for (; i < scoreboardlines && y < vid.height - 8 ; i++)
	{
		k = fragsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw colors
		top = s->colors & 0xf0;
		bottom = (s->colors & 15)<<4;
		top = Sbar_ColorForMap (top);
		bottom = Sbar_ColorForMap (bottom);

		Draw_Fill ( x, y+1, 40, 3, top);
		Draw_Fill ( x, y+4, 40, 4, bottom);

	// draw number
		f = s->frags;
		SNPrintf (num, sizeof(num), "%3i",f);

		Draw_Character ( x+8 , y, num[0]);
		Draw_Character ( x+16 , y, num[1]);
		Draw_Character ( x+24 , y, num[2]);

	// brackets
		if (k == cl.viewentity - 1) {
			Draw_Character ( x, y, 16);
			Draw_Character ( x + 32, y, 17);
		}

#if 0
{
	int				total;
	int				n, minutes, tens, units;

	// draw time
		total = cl.completed_time - s->entertime;
		minutes = (int)total/60;
		n = total - minutes*60;
		tens = n/10;
		units = n%10;

		SNPrintf (num, sizeof(num), "%3i:%i%i", minutes, tens, units);

		Draw_String ( x+48 , y, num);
}
#endif

	// draw name
		Draw_String (x+48, y, s->name);

		y += 8;
	}
}

/*
==================
Sbar_IntermissionOverlay
==================
*/
void Sbar_IntermissionOverlay (void)
{
	qpic_t	*pic;
	int		dig;
	int		num;

//	scr_copyeverything = 1;
//	scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		Sbar_DeathmatchOverlay ();
		return;
	}

	pic = Draw_CachePic ("gfx/complete.lmp");
	Draw_Pic (64, 24, pic);

	pic = Draw_CachePic ("gfx/inter.lmp");
	Draw_TransPic (0, 56, pic);

// time
	dig = cl.completed_time/60;
	Sbar_IntermissionNumber (160, 64, dig, 3, 0);
	num = cl.completed_time - dig*60;
	Draw_TransPic (234,64,sb_colon);
	Draw_TransPic (246,64,sb_nums[0][num/10]);
	Draw_TransPic (266,64,sb_nums[0][num%10]);

	Sbar_IntermissionNumber (160, 104, cl.stats[STAT_SECRETS], 3, 0);
	Draw_TransPic (232,104,sb_slash);
	Sbar_IntermissionNumber (240, 104, cl.stats[STAT_TOTALSECRETS], 3, 0);

	Sbar_IntermissionNumber (160, 144, cl.stats[STAT_MONSTERS], 3, 0);
	Draw_TransPic (232,144,sb_slash);
	Sbar_IntermissionNumber (240, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0);

}


/*
==================
Sbar_FinaleOverlay
==================
*/
void Sbar_FinaleOverlay (void)
{
	qpic_t	*pic;

//	scr_copyeverything = 1;

	pic = Draw_CachePic ("gfx/finale.lmp");
	Draw_TransPic ( (vid.width-pic->width)/2, 16, pic);
}
