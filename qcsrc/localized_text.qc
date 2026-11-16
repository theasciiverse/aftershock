/*  
	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2023 ZungryWare <zungrysoftentertainment@gmail.com>
	Copyright (C) 2024 Ivy Bowling <motolegacy@proton.me>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.
*/

//
// localized_text.qc -- Centralized location for text that can be printed out
// during gameplay. Formerly localization.qc
//
// Aftershock: one set of texts, our own. LibreQuake's #ifdef __LIBREQUAKE__
// texts are the base; id's original texts (the #else branches) are gone;
// endings are new; obituaries and a few names are family-friendly.
//

/*==============================================================================

			EPISODE EPILOGUE STRINGS

		These strings in-engine have a 40 character limit before
		truncation.

==============================================================================*/

#define LOC_EPILOGUE_0		"\nAs the ethereal essence of your very\nbeing drifts through time and space,\nyou experience relentless calm and\nserene determination. You embrace the\nliminal, formless nature of your\nexistence within the amorphous expanse\nbetween realities.\n\nSuspended between the veils of life\nand death, among the realm of celestial\ndissolution, you wait to be called upon\nagain. To regain your corporeal form\nand step into the crucible of fate.\nTo finish your task and keep at bay the\nonslaught of cosmic horrors.\n"
#define LOC_EPILOGUE_1		"The first rune is yours.\n\nThe ground still trembles under your\nfeet, but far away the gate has felt\nit. Three more runes wait in worlds\nthat should never have been joined.\n\nRest a moment. Then go on."
#define LOC_EPILOGUE_1_FREEWARE	LOC_EPILOGUE_1
#define LOC_EPILOGUE_2		"Two runes now, and the hum of the gate\ngrows louder in your ears.\n\nWhatever waits behind it has noticed\nyou. Two more to find."
#define LOC_EPILOGUE_3		"Three runes. The joined worlds bend\ntoward the gate like grass in a wind.\n\nOne more, and it will open."
#define LOC_EPILOGUE_4		"The last rune burns cold in your hand.\n\nAll four are yours. The gate is open,\nand something on the other side has\nbeen waiting a long time.\n\nGo and meet it."
#define LOC_EPILOGUE_RUNES	"You have all four runes!\nThe way to the last gate is open."
#define LOC_EPILOGUE_FINALE	"The gate falls silent. The ground stops\nshaking. For the first time since it\nall began, the worlds are still.\n\nThank you for playing Aftershock."

/*==============================================================================

			CLIENT & SERVER STATE STRINGS

==============================================================================*/

#define LOC_CLIENT_JOIN				" joined the server"
#define LOC_CLIENT_EXIT_LEVEL		" exited the level"
#define LOC_CLIENT_DISCON_A			" left the game with "
#define LOC_CLIENT_DISCON_B			" frags"

/*==============================================================================

			CLIENT WEAPON STATUS STRINGS

==============================================================================*/

#define LOC_WEAPON_INVALID				"Invalid weapon"
#define LOC_WEAPON_NOAMMO				"Out of ammo"

/*==============================================================================

			CLIENT ITEM COLLECTION STRINGS

==============================================================================*/

#define LOC_ITEM_PICKUP_NORMAL			"You got "
#define LOC_ITEM_PICKUP_HEALTH			"You receive "
#define LOC_ITEM_PICKUP_ADD_HEALTH  	" additional health"

#define LOC_ITEM_FROM_BACKPACK			" from a backpack"  // Unused..

#define LOC_ITEM_TYPE_HEALTH			" health"
#define LOC_ITEM_TYPE_ARMOR				" armor"

#define LOC_ITEM_NAME_QUADDMG			"the Crusher Sigil"
#define LOC_ITEM_NAME_OCTDMG			"the Demolisher Sigil"
#define LOC_ITEM_NAME_PENTPROT			"the Aegis"
#define LOC_ITEM_NAME_SHADRING			"the Eye of Enigma"
#define LOC_ITEM_NAME_BIOSUIT			"the Diving Suit"
#define LOC_ITEM_NAME_BACKPACK			"Backpack"

#define LOC_ITEM_FADE_QUADDMG			"Your Crusher Sigil is fading..."
#define LOC_ITEM_FADE_OCTDMG			"Your Demolisher Sigil is fading..."
#define LOC_ITEM_FADE_PENTPROT			"The Aegis is fading..."
#define LOC_ITEM_FADE_SHADRING			"The Eye of Enigma is fading..."
#define LOC_ITEM_FADE_BIOSUIT			"Air in Diving Suit is running out..."

#define LOC_BONUS_POWERS				" is coming for you all!!!"

#define LOC_ITEM_LOST_A					" lost "
#define LOC_ITEM_LOST_B					" with "
#define LOC_ITEM_LOST_C					" seconds left"

/*==============================================================================

			WEAPON AND AMMO STRINGS

==============================================================================*/

#define LOC_WEAPON_NAME_AXE				"Axe"		// Unused..
#define LOC_WEAPON_NAME_SHOTGUN			"Shotgun"   // Unused..
#define LOC_WEAPON_NAME_SUPER_SHOTGUN   "Double-Barreled Shotgun"
#define LOC_WEAPON_NAME_NAILGUN			"Spike Gun"
#define LOC_WEAPON_NAME_SUPER_NAILGUN   "Spike Cannon"
#define LOC_WEAPON_NAME_GRENADE_LAUNCH  "Grenade Launcher"
#define LOC_WEAPON_NAME_ROCKET_LAUNCH   "Rocket Launcher"
#define LOC_WEAPON_NAME_THUNDERBOLT		"Lightning Gun"

#define LOC_WEAPON_AMMO_SHELLS			"shells"
#define LOC_WEAPON_AMMO_NAILS			"spikes"
#define LOC_WEAPON_AMMO_ROCKETS			"rockets"
#define LOC_WEAPON_AMMO_CELLS			"cells"

/*==============================================================================

			KEY STRINGS

==============================================================================*/

#define LOC_KEY_NAME_SILVER				"Silver Key"
#define LOC_KEY_NAME_SILVER_RUNE		"Silver Key"
#define LOC_KEY_NAME_SILVER_CARD		"Silver Keycard"
#define LOC_KEY_NAME_GOLD				"Gold Key"
#define LOC_KEY_NAME_GOLD_RUNE			"Gold Key"
#define LOC_KEY_NAME_GOLD_CARD			"Gold Keycard"
#define LOC_KEY_NEED					"You need "

/*==============================================================================

			MISC STRINGS

==============================================================================*/

#define LOC_COUNTER_COUNT_1A			"Only "
#define LOC_COUNTER_COUNT_1B			" more to go..."
#define LOC_COUNTER_COUNT_2				"There are more to go..."
#define LOC_COUNTER_COMPLETE			"Sequence completed!"

#define LOC_SECRET						"You found a secret!"

/*==============================================================================

			RUNE STRINGS

==============================================================================*/

#define LOC_RUNE_GET					"You got the rune!"

#define LOC_RUNE_LQ_YOU					"You"
#define LOC_RUNE_LQ_BLOOD				" got the Rune of Blood!"
#define LOC_RUNE_LQ_BONE				" got the Rune of Bone!"
#define LOC_RUNE_LQ_MIND				" got the Rune of Mind!"
#define LOC_RUNE_LQ_SOUL				" got the Rune of Soul!"

/*==============================================================================

			CLIENT DEATH TO MONSTER STRINGS

==============================================================================*/

#define LOC_DEATH_ARMY 					" was shot by a Chosen"
#define LOC_DEATH_ENFORCER 				" was mowed down by a Cyberhunter"
#define LOC_DEATH_DOG 					" was bowled over by a Grave Hound"
#define LOC_DEATH_OGRE 					" was demolished by a Barrel Fang"
#define LOC_DEATH_DEMON1 				" was flattened by a Moon Rat"
#define LOC_DEATH_WIZARD 				" was shot by a Messenger"
#define LOC_DEATH_ZOMBIE				" was pelted by a Deathless Cultist"
#define LOC_DEATH_KNIGHT 				" was struck down by a Carcosan Knight"
#define LOC_DEATH_HELL_KNIGHT 			" was knocked flat by a Biohell Warrior"
#define LOC_DEATH_FISH 					" was nibbled by a Spine Eel"
#define LOC_DEATH_SHAMBLER 				" was swatted aside by a Hollow"
#define LOC_DEATH_SHALRATH 				" was blasted by a Servant of Nyarlathotep"
#define LOC_DEATH_TARBABY 				" was swarmed by a Bog Face"
#define LOC_DEATH_BOSS 					" was obliterated by Cthulhi"
#define LOC_DEATH_OLDONE 				" became one with Shoggoth"

/*==============================================================================

			CLIENT DEATH TO ENVIRONMENT STRINGS

==============================================================================*/

#define LOC_DEATH_EXPLO_BOX				" was sent sky high"
#define LOC_DEATH_SQUISH 				" was crushed"
#define LOC_DEATH_SQUISH2 				" was crushed by "
#define LOC_DEATH_TRAP_SPIKE			" was spiked"
#define LOC_DEATH_TRAP_LASER			" was lasered"
#define LOC_DEATH_FIREBALL				" was hit by a lavaball"
#define LOC_DEATH_LEAVE					" tried to escape"
#define LOC_DEATH_DROWN1				" forgot how to swim"
#define LOC_DEATH_DROWN2				" ran out of air"
#define LOC_DEATH_SLIME1				" dissolved in acid"
#define LOC_DEATH_SLIME2				" was disincorporated"
#define LOC_DEATH_LAVA1					" fell into lava"
#define LOC_DEATH_LAVA2					" combusted"
#define LOC_DEATH_LAVA3					" dove headfirst into hot lava"
#define LOC_DEATH_FALL1					" fell off a cliff"
#define LOC_DEATH_FALL2					" took a long walk off a short pier"
#define LOC_DEATH_SPACE1				" fell into space"
#define LOC_DEATH_SPACE2				" drifted off into space"
#define LOC_DEATH_VOID1					" fell into the void"
#define LOC_DEATH_VOID2					" fell out of existence"
#define LOC_DEATH_DISCHARGE_WATER		" took a toaster bath"
#define LOC_DEATH_DISCHARGE_SLIME		" took a toaster bath"
#define LOC_DEATH_DISCHARGE_LAVA		" took a toaster bath"
#define LOC_DEATH_DISCHARGE_SELF		"'s Lightning Gun Malfunctioned"
#define LOC_DEATH_SELF_GRENADE			" didn't launch the grenade far enough"
#define LOC_DEATH_SELF_ROCKET			" played with explosives"
#define LOC_DEATH_SELF_OTHER			" misfired"
#define LOC_DEATH_SUICIDE				" started over"
#define LOC_DEATH_OTHER					" died"

/*==============================================================================

			CLIENT DEATH TO CLIENT STRINGS

==============================================================================*/

// Deathstring 1 is the standard one
// Deathstring 2 has a 1/100 chance to be used instead
// Deathstring G is used when the victim broke apart (Grenade Launcher and Rocket Launcher only)
// Deathstring Q is used if the attacker had a Crusher Sigil
// Deathstring D1 is used if the attack was a discharge (Lightning Gun only)
// Deathstring D2 has a 1/100 chance to be used instead (Lightning Gun only)
// The format for printing is: victim name, deathstring A, attacker name, deathstring B

#define LOC_DEATH_AXE_1A				" was cut down by "
#define LOC_DEATH_AXE_1B				""
#define LOC_DEATH_AXE_2A				" was chopped down by "
#define LOC_DEATH_AXE_2B				""
#define LOC_DEATH_AXE_QA				"'s bucket was kicked by "
#define LOC_DEATH_AXE_QB				""

#define LOC_DEATH_SHOTGUN_1A			" breathed in "
#define LOC_DEATH_SHOTGUN_1B			"'s exhaust"
#define LOC_DEATH_SHOTGUN_2A			" ate "
#define LOC_DEATH_SHOTGUN_2B			"'s dust"
#define LOC_DEATH_SHOTGUN_QA			" was put down by "
#define LOC_DEATH_SHOTGUN_QB			""

#define LOC_DEATH_SUPER_SHOTGUN_1A		" was blasted by "
#define LOC_DEATH_SUPER_SHOTGUN_1B		""
#define LOC_DEATH_SUPER_SHOTGUN_2A		" was shredded by "
#define LOC_DEATH_SUPER_SHOTGUN_2B		""
#define LOC_DEATH_SUPER_SHOTGUN_QA		" was sent packing by "
#define LOC_DEATH_SUPER_SHOTGUN_QB		""

#define LOC_DEATH_NAILGUN_1A			" was pierced by "
#define LOC_DEATH_NAILGUN_1B			""
#define LOC_DEATH_NAILGUN_2A			" was filled with holes by "
#define LOC_DEATH_NAILGUN_2B			""
#define LOC_DEATH_NAILGUN_QA			" was pinned down by "
#define LOC_DEATH_NAILGUN_QB			""

#define LOC_DEATH_SUPER_NAILGUN_1A		" was perforated by "
#define LOC_DEATH_SUPER_NAILGUN_1B		""
#define LOC_DEATH_SUPER_NAILGUN_2A		" was given speed holes by "
#define LOC_DEATH_SUPER_NAILGUN_2B		""
#define LOC_DEATH_SUPER_NAILGUN_QA		"'s chips were cashed by "
#define LOC_DEATH_SUPER_NAILGUN_QB		""

#define LOC_DEATH_GRENADE_LAUNCHER_1A	" tripped over "
#define LOC_DEATH_GRENADE_LAUNCHER_1B	"'s grenade"
#define LOC_DEATH_GRENADE_LAUNCHER_2A	" stepped on "
#define LOC_DEATH_GRENADE_LAUNCHER_2B	"'s grenade"
#define LOC_DEATH_GRENADE_LAUNCHER_GA	" was blown away by "
#define LOC_DEATH_GRENADE_LAUNCHER_GB	"'s grenade"
#define LOC_DEATH_GRENADE_LAUNCHER_QA	"'s clock was cleaned by "
#define LOC_DEATH_GRENADE_LAUNCHER_QB	""

#define LOC_DEATH_ROCKET_LAUNCHER_1A	" was blasted by "
#define LOC_DEATH_ROCKET_LAUNCHER_1B	"'s rocket"
#define LOC_DEATH_ROCKET_LAUNCHER_2A	" was fulminated by "
#define LOC_DEATH_ROCKET_LAUNCHER_2B	"'s rocket"
#define LOC_DEATH_ROCKET_LAUNCHER_GA	" was blown away by "
#define LOC_DEATH_ROCKET_LAUNCHER_GB	"'s rocket"
#define LOC_DEATH_ROCKET_LAUNCHER_Q1A	" rode "
#define LOC_DEATH_ROCKET_LAUNCHER_Q1B	"'s rocket all the way"

#define LOC_DEATH_LIGHTNING_1A			" was electrocuted by "
#define LOC_DEATH_LIGHTNING_1B			""
#define LOC_DEATH_LIGHTNING_2A			" was given shock treatment by "
#define LOC_DEATH_LIGHTNING_2B			""
#define LOC_DEATH_LIGHTNING_QA			" was lit up by "
#define LOC_DEATH_LIGHTNING_QB			""
#define LOC_DEATH_LIGHTNING_DA			" was given a toaster bath by "
#define LOC_DEATH_LIGHTNING_DB			""

#define LOC_DEATH_TELEFRAG				" was telefragged by "

#define LOC_DEATH_DEFLECT_A				"The Aegis deflected "
#define LOC_DEATH_DEFLECT_B				"'s telefrag on "

#define LOC_TEAMKILL_1					" killed a teammate"
#define LOC_TEAMKILL_2					" friendly-fired"
#define LOC_TEAMKILL_3					" needs an eye exam"
#define LOC_TEAMKILL_4					" betrayed their friend"
#define LOC_TEAMKILL_SQUISH				" crushed a teammate"

#define LOC_DEATH_KILL					" killed "

/*==============================================================================

			LIBREQUAKE UTILITY STRINGS

==============================================================================*/

#define LOC_LQ_ISDEVELOPER				"You must be a developer!"

/*==============================================================================

			LIBREQUAKE CAMERA STRINGS

==============================================================================*/

#define LOC_LQ_CAMERAS_MAX				"You have too many cameras!"
#define LOC_LQ_CAMERA_SPAWN_A			"Camera placed successfully, "
#define LOC_LQ_CAMERA_SPAWN_B			" remaining"
#define LOC_LQ_CAMERA_TIME_A			"Time between cameras changed to "
#define LOC_LQ_CAMERA_TIME_B			" seconds"
#define LOC_LQ_CAMERA_NONE				"Oh noes! No camera found! :("
