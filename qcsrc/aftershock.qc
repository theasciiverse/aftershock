/*
	Aftershock: family-friendly by default.

	Copyright (C) 2026 the Aftershock authors.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	With the av_gore cvar at 0 (the default, kept in the engine) bodies
	break into stone debris instead of gibs and heads, hits throw sparks
	instead of blood, and the squelching gib sounds become thuds. av_gore 1
	brings back LibreQuake's original effects, from the next gib on.
*/

float() AV_Gore =
{
	return cvar ("av_gore");
};

// A gib becomes one of three stones; zom_gib (thrown flesh) the largest.
string(string gibname) AV_GibModel =
{
	if (AV_Gore ())
		return gibname;
	if (gibname == "progs/gib1.mdl")
		return "progs/av_rubble1.mdl";
	if (gibname == "progs/gib2.mdl")
		return "progs/av_rubble2.mdl";
	return "progs/av_rubble3.mdl";
};

// What a body leaves when it breaks apart: a pile of stone, not a head.
string(string headname) AV_HeadModel =
{
	if (AV_Gore ())
		return headname;
	return "progs/av_pile.mdl";
};

string(string soundname) AV_GibSound =
{
	if (AV_Gore ())
		return soundname;
	if (soundname == "zombie/z_gib.wav")
		return "weapons/tink1.wav";
	return "player/land2.wav";
};

// Blood is palette colour 73 (a red ramp); sparks are the fire ramp.
float(float color) AV_BloodColor =
{
	if (AV_Gore ())
		return color;
	return 226;
};

void() AV_Precache =
{
	precache_model ("progs/av_rubble1.mdl");
	precache_model ("progs/av_rubble2.mdl");
	precache_model ("progs/av_rubble3.mdl");
	precache_model ("progs/av_pile.mdl");
	precache_sound ("player/land2.wav");
	precache_sound ("weapons/tink1.wav");
};
