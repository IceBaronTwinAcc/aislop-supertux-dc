#ifndef SUPERTUX_PVR_RENDERER_H
#define SUPERTUX_PVR_RENDERER_H

#ifdef PVR_RENDERER

#include <SDL.h>
#include <dc/pvr.h>

namespace PVRRenderer
{
bool init();
void shutdown();
void set_clear_color(Uint8 r, Uint8 g, Uint8 b);
void finish_frame();
void wait_for_render();

pvr_ptr_t upload_texture(SDL_Surface* surface, int width, int height);
void free_texture(pvr_ptr_t texture);

void draw_texture(pvr_ptr_t texture, int texture_width, int texture_height,
                  float sx, float sy, float sw, float sh,
                  float x, float y, float w, float h, Uint8 alpha);
void draw_texture_batch(pvr_ptr_t texture, int texture_width, int texture_height,
                        float sx, float sy, float sw, float sh,
                        const float* positions, unsigned int count,
                        float w, float h, Uint8 alpha);
void draw_colored_quad(float x, float y, float w, float h,
                       Uint8 top_r, Uint8 top_g, Uint8 top_b, Uint8 top_a,
                       Uint8 bottom_r, Uint8 bottom_g, Uint8 bottom_b, Uint8 bottom_a);
}

#endif

#endif
