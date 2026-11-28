# Aftershock: a first-person shooter in the terminal.
#
# One binary, build/aftershock: our frontend (src/av_*.c) linked with the
# libretro tyrquake core built as a static archive (STATIC_LINKING=1).
#
#   make            the game (chafa, libcaca and ALSA when pkg-config finds them)
#
# Linux is the home platform.  macOS (cc) and Windows (MinGW-w64, e.g.
# make CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar) build the same
# frontend without chafa and libcaca (and, so far, silent);
# the compiler's target decides which (cc -dumpmachine).
#   make CHAFA=0    without chafa (chafa is LGPL-3.0-or-later; a build with it
#                   is distributed under GPL-3.0-or-later)
#   make CHAFA_STATIC=1   chafa and GLib from their static archives, for a
#                   release binary that needs nothing but libc
#   make CACA=0     without libcaca
#   make backends   which optional libraries were found
#   make test       the evdev device-selection test
#
# SPDX-License-Identifier: GPL-2.0-or-later

PREFIX     ?= /usr/local
CC         ?= cc
PKG_CONFIG ?= pkg-config

# Which system the compiler builds for, cross-compiling included.
MACHINE    := $(shell $(CC) -dumpmachine)
ifneq (,$(findstring mingw,$(MACHINE)))
TARGET_OS  := windows
else ifneq (,$(findstring darwin,$(MACHINE)))
TARGET_OS  := macos
else
TARGET_OS  := linux
endif

B          := build
ifeq ($(TARGET_OS),windows)
EXE        := .exe
endif
BIN        := $(B)/aftershock$(EXE)

CORE_DIR   := third_party/tyrquake
CORE_LIB   := $(CORE_DIR)/tyrquake_libretro_unix.a
LRC        := $(CORE_DIR)/libretro-common

SOURCES    := av_main av_term av_keys av_look av_pointer av_evdev av_host \
              av_render av_pictures av_sound av_data \
              av_half av_overlay av_pad

# Linking statically, the core leaves these libretro-common files to the
# frontend (its Makefile.common says which).  A missing one shows up as a
# named link error.
LRC_FILES  := file/retro_dirent encodings/encoding_utf string/stdstring \
              streams/file_stream streams/file_stream_transforms \
              vfs/vfs_implementation file/file_path file/file_path_io \
              features/features_cpu compat/fopen_utf8 compat/compat_strl \
              compat/compat_posix_string compat/compat_strcasestr \
              compat/compat_snprintf time/rtime net/net_compat net/net_socket

CFLAGS     ?= -O2 -g
CFLAGS     += -Wall -Wextra -std=gnu99 -pthread
# libretro.h, and the core's av_textui.h (the engine's text record).
CPPFLAGS   += -I$(LRC)/include -iquote $(CORE_DIR)/common
LDLIBS     += -lm -pthread

# Optional libraries, each adding its file and a define.  On macOS and
# Windows they are off unless asked for, and ALSA is Linux's alone.
ifneq ($(TARGET_OS),linux)
CHAFA ?= 0
CACA ?= 0
endif
CHAFA ?= 1
ifneq ($(CHAFA),0)
HAVE_CHAFA := $(shell $(PKG_CONFIG) --exists chafa && echo 1)
endif
CACA ?= 1
ifneq ($(CACA),0)
HAVE_CACA  := $(shell $(PKG_CONFIG) --exists caca && echo 1)
endif
ifeq ($(TARGET_OS),linux)
HAVE_ALSA  := $(shell $(PKG_CONFIG) --exists alsa && echo 1)
endif

ifeq ($(HAVE_CHAFA),1)
SOURCES    += av_chafa
CPPFLAGS   += -DAV_HAVE_CHAFA $(shell $(PKG_CONFIG) --cflags chafa)
ifeq ($(CHAFA_STATIC),1)
LDLIBS     += $(shell sh asciiverse/static-libs.sh chafa)
else
LDLIBS     += $(shell $(PKG_CONFIG) --libs chafa)
endif
endif
ifeq ($(HAVE_CACA),1)
SOURCES    += av_caca
CPPFLAGS   += -DAV_HAVE_CACA $(shell $(PKG_CONFIG) --cflags caca)
LDLIBS     += $(shell $(PKG_CONFIG) --libs caca)
endif
# Sound is always compiled: without ALSA's headers it is a set of stubs.
# libasound itself is loaded at run time (av_sound.c), never linked.
ifeq ($(HAVE_ALSA),1)
CPPFLAGS   += -DAV_HAVE_ALSA $(shell $(PKG_CONFIG) --cflags alsa)
LDLIBS     += -ldl
endif

# One self-contained .exe: winpthreads and libgcc linked in.  Sockets for
# libretro-common's net files, winmm for the 1 ms timer period.
ifeq ($(TARGET_OS),windows)
LDFLAGS    += -static
LDLIBS     += -lws2_32 -lwinmm
endif

OBJS       := $(SOURCES:%=$(B)/%.o) $(LRC_FILES:%=$(B)/lrc/%.o)

# make does not notice flags changing.  When a library appears or goes,
# the flags do, and this stamp makes everything that depends on them
# rebuild (a stale object once left chafa out of a binary built "with" it).
STAMP      := $(B)/.flags

.PHONY: all engine backends test clean install always
.DELETE_ON_ERROR:

all: $(BIN)

$(BIN): $(OBJS) $(CORE_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(CORE_LIB) $(LDLIBS)

$(B)/%.o: src/%.c $(STAMP)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c -o $@ $<

# Third-party code: built quietly so our own warnings stand out.
$(B)/lrc/%.o: $(LRC)/%.c $(STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -I$(LRC)/include -c -o $@ $<

$(STAMP): always
	@mkdir -p $(B)
	@echo '$(CC) $(CFLAGS) $(CPPFLAGS) $(LDLIBS)' > $@.new
	@if cmp -s $@.new $@; then rm -f $@.new; else mv $@.new $@; fi

# The core carries our changes, so its own make always runs; it rebuilds
# only what changed.
engine: $(CORE_LIB)
# platform=unix everywhere: the core has nothing system-specific beyond
# what libretro-common handles, and the archive keeps one name.
$(CORE_LIB): always
	$(MAKE) -C $(CORE_DIR) STATIC_LINKING=1 platform=unix

backends:
	@echo "chafa: $(if $(HAVE_CHAFA),yes,no$(if $(filter 0,$(CHAFA)), (CHAFA=0), (install chafa)))"
	@echo "caca:  $(if $(HAVE_CACA),yes,no$(if $(filter 0,$(CACA)), (CACA=0), (install libcaca)))"
	@echo "target: $(TARGET_OS) ($(MACHINE))"
	@echo "alsa:  $(if $(HAVE_ALSA),yes,$(if $(filter linux,$(TARGET_OS)),no (install alsa-lib; the game is silent),no (silent)))"

# Device selection is the one part that can do real harm (reading a
# keyboard is keylogging), so it has a test of its own.
TEST_BIN   := $(B)/av_evdev_test

test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): tests/av_evdev_test.c src/av_evdev.c src/av_evdev.h
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -Isrc -o $@ tests/av_evdev_test.c src/av_evdev.c

clean:
	rm -rf $(B)
	-$(MAKE) -C $(CORE_DIR) clean

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/aftershock$(EXE)

always:

-include $(SOURCES:%=$(B)/%.d)
