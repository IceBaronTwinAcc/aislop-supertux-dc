#include "pvr_renderer.h"

#ifdef PVR_RENDERER

namespace
{
bool initialized = false;
bool frame_active = false;
float clear_r = 0.0f;
float clear_g = 0.0f;
float clear_b = 0.0f;

void begin_frame()
{
  if(frame_active)
    return;

  pvr_wait_ready();
  pvr_set_bg_color(clear_r, clear_g, clear_b);
  pvr_scene_begin();
  pvr_list_begin(PVR_LIST_TR_POLY);
  frame_active = true;
}

uint32_t pack_color(Uint8 a, Uint8 r, Uint8 g, Uint8 b)
{
  return PVR_PACK_COLOR(a / 255.0f, r / 255.0f, g / 255.0f, b / 255.0f);
}

void submit_vertex(pvr_vertex_t* vertex, uint32_t flags,
                   float x, float y, float u, float v, uint32_t color)
{
  vertex->flags = flags;
  vertex->x = x;
  vertex->y = y;
  vertex->z = 1.0f;
  vertex->u = u;
  vertex->v = v;
  vertex->argb = color;
  vertex->oargb = 0;
  pvr_prim(vertex, sizeof(*vertex));
}
}

bool
PVRRenderer::init()
{
  if(initialized)
    return true;

  pvr_init_params_t params = {
    { PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_0 },
    1024 * 1024,
    0,
    0,
    1,
    3,
    0
  };

  if(pvr_init(&params) < 0)
    return false;

  initialized = true;
  return true;
}

void
PVRRenderer::shutdown()
{
  if(!initialized)
    return;

  finish_frame();
  pvr_wait_render_done();
  pvr_shutdown();
  initialized = false;
}

void
PVRRenderer::set_clear_color(Uint8 r, Uint8 g, Uint8 b)
{
  clear_r = r / 255.0f;
  clear_g = g / 255.0f;
  clear_b = b / 255.0f;
}

void
PVRRenderer::finish_frame()
{
  begin_frame();
  pvr_list_finish();
  pvr_scene_finish();
  frame_active = false;
}

void
PVRRenderer::wait_for_render()
{
  if(frame_active)
    finish_frame();
  pvr_wait_render_done();
}

pvr_ptr_t
PVRRenderer::upload_texture(SDL_Surface* surface, int width, int height)
{
  SDL_Surface* converted = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16,
                                                0x0f00, 0x00f0, 0x000f, 0xf000);
  if(!converted)
    return 0;

  SDL_FillRect(converted, 0, 0);

  const Uint32 saved_flags = surface->flags & (SDL_SRCALPHA | SDL_RLEACCELOK);
  const Uint8 saved_alpha = surface->format->alpha;
  if(saved_flags & SDL_SRCALPHA)
    SDL_SetAlpha(surface, 0, 0);

  if(SDL_BlitSurface(surface, 0, converted, 0) < 0)
    {
      if(saved_flags & SDL_SRCALPHA)
        SDL_SetAlpha(surface, saved_flags, saved_alpha);
      SDL_FreeSurface(converted);
      return 0;
    }

  if(saved_flags & SDL_SRCALPHA)
    SDL_SetAlpha(surface, saved_flags, saved_alpha);

  const size_t size = width * height * sizeof(uint16_t);
  pvr_ptr_t texture = pvr_mem_malloc(size);
  if(texture)
    pvr_txr_load(converted->pixels, texture, size);
  SDL_FreeSurface(converted);
  return texture;
}

void
PVRRenderer::free_texture(pvr_ptr_t texture)
{
  if(texture)
    pvr_mem_free(texture);
}

void
PVRRenderer::draw_texture(pvr_ptr_t texture, int texture_width, int texture_height,
                          float sx, float sy, float sw, float sh,
                          float x, float y, float w, float h, Uint8 alpha)
{
  const float positions[] = { x, y };
  draw_texture_batch(texture, texture_width, texture_height,
                     sx, sy, sw, sh, positions, 1, w, h, alpha);
}

void
PVRRenderer::draw_texture_batch(pvr_ptr_t texture, int texture_width, int texture_height,
                                float sx, float sy, float sw, float sh,
                                const float* positions, unsigned int count,
                                float w, float h, Uint8 alpha)
{
  if(count == 0)
    return;

  begin_frame();

  pvr_poly_cxt_t context;
  pvr_poly_hdr_t header;
  pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY,
                   PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED,
                   texture_width, texture_height, texture, PVR_FILTER_BILINEAR);
  context.gen.culling = PVR_CULLING_NONE;
  context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
  context.depth.write = PVR_DEPTHWRITE_DISABLE;
  context.txr.uv_clamp = PVR_UVCLAMP_UV;
  context.txr.alpha = PVR_TXRALPHA_ENABLE;
  context.gen.alpha = PVR_ALPHA_ENABLE;
  context.blend.src = PVR_BLEND_SRCALPHA;
  context.blend.dst = PVR_BLEND_INVSRCALPHA;
  pvr_poly_compile(&header, &context);
  pvr_prim(&header, sizeof(header));

  const float left = sx / texture_width;
  const float top = sy / texture_height;
  const float right = (sx + sw) / texture_width;
  const float bottom = (sy + sh) / texture_height;
  const uint32_t color = pack_color(alpha, alpha, alpha, alpha);
  pvr_vertex_t vertex;

  for(unsigned int i = 0; i < count; ++i)
    {
      const float x = positions[i * 2];
      const float y = positions[i * 2 + 1];
      submit_vertex(&vertex, PVR_CMD_VERTEX, x, y, left, top, color);
      submit_vertex(&vertex, PVR_CMD_VERTEX, x + w, y, right, top, color);
      submit_vertex(&vertex, PVR_CMD_VERTEX, x, y + h, left, bottom, color);
      submit_vertex(&vertex, PVR_CMD_VERTEX_EOL, x + w, y + h, right, bottom, color);
    }
}

void
PVRRenderer::draw_colored_quad(float x, float y, float w, float h,
                               Uint8 top_r, Uint8 top_g, Uint8 top_b, Uint8 top_a,
                               Uint8 bottom_r, Uint8 bottom_g, Uint8 bottom_b, Uint8 bottom_a)
{
  begin_frame();

  pvr_poly_cxt_t context;
  pvr_poly_hdr_t header;
  pvr_poly_cxt_col(&context, PVR_LIST_TR_POLY);
  context.gen.culling = PVR_CULLING_NONE;
  context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
  context.depth.write = PVR_DEPTHWRITE_DISABLE;
  context.gen.alpha = PVR_ALPHA_ENABLE;
  context.blend.src = PVR_BLEND_SRCALPHA;
  context.blend.dst = PVR_BLEND_INVSRCALPHA;
  pvr_poly_compile(&header, &context);
  pvr_prim(&header, sizeof(header));

  const uint32_t top_color = pack_color(top_a, top_r, top_g, top_b);
  const uint32_t bottom_color = pack_color(bottom_a, bottom_r, bottom_g, bottom_b);
  pvr_vertex_t vertex;

  submit_vertex(&vertex, PVR_CMD_VERTEX, x, y, 0, 0, top_color);
  submit_vertex(&vertex, PVR_CMD_VERTEX, x + w, y, 0, 0, top_color);
  submit_vertex(&vertex, PVR_CMD_VERTEX, x, y + h, 0, 0, bottom_color);
  submit_vertex(&vertex, PVR_CMD_VERTEX_EOL, x + w, y + h, 0, 0, bottom_color);
}

#endif
