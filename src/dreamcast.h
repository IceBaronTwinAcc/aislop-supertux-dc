#ifndef SUPERTUX_DREAMCAST_H
#define SUPERTUX_DREAMCAST_H

#ifdef __DREAMCAST__

#include <stdio.h>
#include <string>
#include <kos.h>

std::string loadFromVMU(FILE* f);
void saveToVMU(FILE* f, const char* data, const char* longdesc);

uint32 getPressed(int port=0);
uint32 getButtons(int port=0);

#ifdef PROFILE_INPUT_PLAYBACK
enum DreamcastProfileInputContext {
  PROFILE_INPUT_TITLE,
  PROFILE_INPUT_WORLDMAP,
  PROFILE_INPUT_GAME
};

void dreamcast_profile_input_begin();
void dreamcast_profile_input_set_context(DreamcastProfileInputContext context);
#endif

bool dreamcast_default_60hz();
int dreamcast_mp3_start(const char* file, int loop);
void dreamcast_mp3_stop();
void dreamcast_mp3_shutdown();

#endif // __DREAMCAST__
#endif // SUPERTUX_DREAMCAST_H
