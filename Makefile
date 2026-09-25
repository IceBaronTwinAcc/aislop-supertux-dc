#
# Basic KallistiOS skeleton / test program
# Copyright (C)2001-2004 Dan Potter
#   

# Put the filename of the output binary here
TARGET = supertux.elf

CXXFLAGS += -O2 -MMD -MP -D__DREAMCAST__ -I$(KOS_PORTS)/include/SDL -I$(KOS_PORTS)/include/zlib -Iports/fake_mixer -Iports/mikmod/lmikmod/include -Iports/dreamhal/inc

ifdef PROFILE_AUTORUN
PROFILE_AUTORUN_LEVEL ?= 1
PROFILE_AUTORUN_RUNTIME ?= 2000
CXXFLAGS += -DPROFILE_AUTORUN -DPROFILE_AUTORUN_LEVEL=$(PROFILE_AUTORUN_LEVEL) -DPROFILE_AUTORUN_RUNTIME=$(PROFILE_AUTORUN_RUNTIME)
ifdef PROFILE_AUTORUN_START_X
CXXFLAGS += -DPROFILE_AUTORUN_START_X=$(PROFILE_AUTORUN_START_X)
endif
ifdef PROFILE_AUTORUN_WARM_PREVIOUS
CXXFLAGS += -DPROFILE_AUTORUN_WARM_PREVIOUS
endif
ifdef PROFILE_AUTORUN_MOVE
CXXFLAGS += -DPROFILE_AUTORUN_MOVE
endif
ifdef PROFILE_AUTORUN_INVINCIBLE
CXXFLAGS += -DPROFILE_AUTORUN_INVINCIBLE
endif
ifdef PROFILE_AUTORUN_REPEAT_LEVEL
CXXFLAGS += -DPROFILE_AUTORUN_REPEAT_LEVEL=$(PROFILE_AUTORUN_REPEAT_LEVEL)
endif
ifdef PROFILE_AUTORUN_TITLE_FLOW
CXXFLAGS += -DPROFILE_AUTORUN_TITLE_FLOW
endif
ifdef PROFILE_AUTORUN_FINISH
CXXFLAGS += -DPROFILE_AUTORUN_FINISH
endif
ifdef PROFILE_KEEP_SESSION_TEXTURES
CXXFLAGS += -DPROFILE_KEEP_SESSION_TEXTURES
endif
ifdef PROFILE_REFRESH_TOGGLE
CXXFLAGS += -DPROFILE_REFRESH_TOGGLE
endif
ifdef PROFILE_AUTORUN_WORLDMAP
CXXFLAGS += -DPROFILE_AUTORUN_WORLDMAP
endif
endif

ifdef PROFILE_INPUT_PLAYBACK
CXXFLAGS += -DPROFILE_INPUT_PLAYBACK
endif

PVR_PIPELINE ?= 1
PVR_RENDERER ?= 0
GLDC_LIBDIR ?= $(KOS_PORTS)/lib

ifeq ($(PVR_PIPELINE),1)
CXXFLAGS += -DPVR_PIPELINE
endif

ifeq ($(PVR_RENDERER),1)
CXXFLAGS += -DPVR_RENDERER
endif

# List all of your C files here, but change the extension to ".o"
# Include "romdisk.o" if you want a rom disk.
OBJS = src/badguy.o src/bitmask.o src/button.o src/collision.o src/configfile.o src/dreamcast.o src/gameloop.o src/gameobjs.o src/globals.o src/high_scores.o src/intro.o src/level.o src/leveleditor.o src/lispreader.o src/menu.o src/mousecursor.o src/music_manager.o src/musicref.o src/particlesystem.o src/physic.o src/player.o src/pvr_renderer.o src/resources.o src/scene.o src/screen.o src/setup.o src/sound.o src/special.o src/sprite.o src/sprite_manager.o src/supertux.o src/text.o src/texture.o src/tile.o src/timer.o src/title.o src/type.o src/world.o src/worldmap.o
DEPS = $(OBJS:.o=.d)

# If you define this, the Makefile.rules will create a romdisk.o for you
# from the named dir.
#KOS_ROMDISK_DIR = romdisk

# The rm-elf step is to remove the target before building, to force the
# re-creation of the rom disk.
all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) $(DEPS) romdisk.*

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-c++ -o $(TARGET) $(OBJS) $(LDFLAGS) -g -lFAKE_mixer -lSDL_image -lSDL $(GLDC_LIBDIR)/libGL.a -lmikmod -lmp3 -ljpeg -lpng -lz -lm

-include $(DEPS)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
#	rm -f $(OBJS) romdisk.o romdisk.img
	$(KOS_STRIP) $(TARGET)
