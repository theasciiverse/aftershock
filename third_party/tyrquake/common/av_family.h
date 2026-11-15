/* Aftershock: family-friendly effects and the game's own words (av_family.c) */
#ifndef AV_FAMILY_H
#define AV_FAMILY_H

#include "cvar.h"

extern cvar_t av_gore;

void AV_Family_Init(void);
const char *AV_GoreFile(const char *path);
int AV_ParticleColor(int color);
int AV_TrailType(int type);
const char *AV_Text(const char *in);

#endif
