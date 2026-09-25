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

Uint32 get_surface_pixel(SDL_Surface* surface, int x, int y)
{
  const int bytes_per_pixel = surface->format->BytesPerPixel;
  Uint8* pixel = static_cast<Uint8*>(surface->pixels) +
                 y * surface->pitch + x * bytes_per_pixel;

  switch(bytes_per_pixel)
    {
    case 1:
      return *pixel;
    case 2:
      return *reinterpret_cast<Uint16*>(pixel);
    case 3:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      return pixel[0] << 16 | pixel[1] << 8 | pixel[2];
#else
      return pixel[0] | pixel[1] << 8 | pixel[2] << 16;
#endif
    case 4:
      return *reinterpret_cast<Uint32*>(pixel);
    default:
      return 0;
    }
}

bool dither_rgb565(SDL_Surface* source, SDL_Surface* destination)
{
  static const Uint8 bayer[4][4] = {
    { 0,  8,  2, 10 },
    { 12, 4, 14,  6 },
    { 3, 11,  1,  9 },
    { 15, 7, 13,  5 }
  };

  if(SDL_LockSurface(source) < 0)
    return false;
  if(SDL_LockSurface(destination) < 0)
    {
      SDL_UnlockSurface(source);
      return false;
    }

  const int copy_width = source->w < destination->w ? source->w : destination->w;
  const int copy_height = source->h < destination->h ? source->h : destination->h;
  for(int y = 0; y < copy_height; ++y)
    {
      Uint16* output = reinterpret_cast<Uint16*>(
          static_cast<Uint8*>(destination->pixels) + y * destination->pitch);
      for(int x = 0; x < copy_width; ++x)
        {
          Uint8 red;
          Uint8 green;
          Uint8 blue;
          SDL_GetRGB(get_surface_pixel(source, x, y), source->format,
                     &red, &green, &blue);

          const unsigned int threshold = bayer[y & 3][x & 3] * 255 / 16;
          const unsigned int red5 = (red * 31 + threshold) / 255;
          const unsigned int green6 = (green * 63 + threshold) / 255;
          const unsigned int blue5 = (blue * 31 + threshold) / 255;
          output[x] = static_cast<Uint16>((red5 << 11) | (green6 << 5) | blue5);
        }
    }

  SDL_UnlockSurface(destination);
  SDL_UnlockSurface(source);
  return true;
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
PVRRenderer::surface_has_transparency(SDL_Surface* surface)
{
  if(!surface->format->Amask)
    return false;

  if(SDL_LockSurface(surface) < 0)
    return true;

  bool has_transparency = false;
  for(int y = 0; y < surface->h && !has_transparency; ++y)
    for(int x = 0; x < surface->w; ++x)
      {
        Uint8 alpha;
        Uint8 red;
        Uint8 green;
        Uint8 blue;
        SDL_GetRGBA(get_surface_pixel(surface, x, y), surface->format,
                    &red, &green, &blue, &alpha);
        if(alpha != 255)
          {
            has_transparency = true;
            break;
          }
      }

  SDL_UnlockSurface(surface);
  return has_transparency;
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
PVRRenderer::upload_texture(SDL_Surface* surface, int width, int height, bool has_alpha)
{
  const Uint32 red_mask = has_alpha ? 0x0f00 : 0xf800;
  const Uint32 green_mask = has_alpha ? 0x00f0 : 0x07e0;
  const Uint32 blue_mask = has_alpha ? 0x000f : 0x001f;
  const Uint32 alpha_mask = has_alpha ? 0xf000 : 0;
  SDL_Surface* converted = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16,
                                                red_mask, green_mask, blue_mask, alpha_mask);
  if(!converted)
    return 0;

  SDL_FillRect(converted, 0, 0);

  if(!has_alpha)
    {
      if(!dither_rgb565(surface, converted))
        {
          SDL_FreeSurface(converted);
          return 0;
        }
    }
  else
    {
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
    }

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
PVRRenderer::draw_texture(pvr_ptr_t texture, int texture_width, int texture_height, bool has_alpha,
                          float sx, float sy, float sw, float sh,
                          float x, float y, float w, float h, Uint8 alpha)
{
  const float positions[] = { x, y };
  draw_texture_batch(texture, texture_width, texture_height, has_alpha,
                     sx, sy, sw, sh, positions, 1, w, h, alpha);
}

void
PVRRenderer::draw_texture_batch(pvr_ptr_t texture, int texture_width, int texture_height,
                                bool has_alpha,
                                float sx, float sy, float sw, float sh,
                                const float* positions, unsigned int count,
                                float w, float h, Uint8 alpha)
{
  if(count == 0)
    return;

  begin_frame();

  pvr_poly_cxt_t context;
  pvr_poly_hdr_t header;
  const uint32_t texture_format = has_alpha ? PVR_TXRFMT_ARGB4444 : PVR_TXRFMT_RGB565;
  pvr_poly_cxt_txr(&context, PVR_LIST_TR_POLY,
                   texture_format | PVR_TXRFMT_NONTWIDDLED,
                   texture_width, texture_height, texture, PVR_FILTER_BILINEAR);
  context.gen.culling = PVR_CULLING_NONE;
  context.depth.comparison = PVR_DEPTHCMP_ALWAYS;
  context.depth.write = PVR_DEPTHWRITE_DISABLE;
  context.txr.uv_clamp = PVR_UVCLAMP_UV;
  context.txr.alpha = has_alpha ? PVR_TXRALPHA_ENABLE : PVR_TXRALPHA_DISABLE;
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
