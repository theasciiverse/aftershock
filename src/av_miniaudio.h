/* Aftershock: miniaudio as the sound frontend uses it (macOS, Windows).
 *
 * The same configuration in every file that includes miniaudio, or its
 * structures would differ between them: playback only, no decoders or
 * engine, and only the backends each platform needs.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_MINIAUDIO_H
#define AV_MINIAUDIO_H

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#ifdef _WIN32
#define MA_ENABLE_WASAPI
#define MA_ENABLE_DSOUND
#define MA_ENABLE_WINMM
#elif defined(__APPLE__)
#define MA_ENABLE_COREAUDIO
#define MA_NO_RUNTIME_LINKING
#endif
#define MA_ENABLE_NULL

#include "../third_party/miniaudio/miniaudio.h"

#endif
