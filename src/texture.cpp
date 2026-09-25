//  $Id: texture.cpp 1053 2004-05-09 18:08:02Z tobgle $
//
//  SuperTux
//  Copyright (C) 2004 Tobias Glaesser <tobi.web@gmx.de>
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//  02111-1307, USA.

#include <assert.h>
#include <iostream>
#include <algorithm>
#include <map>
#include <sstream>
#include <vector>
#include "SDL.h"
#include "SDL_image.h"
#include "texture.h"
#include "screen.h"
#include "pvr_renderer.h"

#ifdef PVR_PIPELINE
#include <dc/pvr.h>
#endif
#include "globals.h"
#include "setup.h"

Surface::Surfaces Surface::surfaces;

static std::string source_image_cache_file;
static SDL_Surface* source_image_cache = 0;

static SDL_Surface*
load_source_image(const std::string& file)
{
  if(source_image_cache && source_image_cache_file == file)
    return source_image_cache;

  SDL_Surface* loaded = IMG_Load(file.c_str());
  if(!loaded)
    st_abort("Can't load", file);

  SDL_FreeSurface(source_image_cache);
  source_image_cache = loaded;
  source_image_cache_file = file;
  return source_image_cache;
}

#ifdef PVR_RENDERER
static bool pvr_texture_release_safe = false;
#endif

SurfaceData::SurfaceData(SDL_Surface* temp, int use_alpha_)
    : type(SURFACE), surface(0), use_alpha(use_alpha_)
{
  // Copy the given surface and make sure that it is not stored in
  // video memory
  surface = SDL_CreateRGBSurface(temp->flags & (~SDL_HWSURFACE),
                                 temp->w, temp->h,
                                 temp->format->BitsPerPixel,
                                 temp->format->Rmask,
                                 temp->format->Gmask,
                                 temp->format->Bmask,
                                 temp->format->Amask);
  if(!surface)
    st_abort("No memory left.", "");
  SDL_SetAlpha(temp,0,0);
  SDL_BlitSurface(temp, NULL, surface, NULL);
}

SurfaceData::SurfaceData(const std::string& file_, int use_alpha_)
    : type(LOAD), surface(0), file(file_), use_alpha(use_alpha_)
{}

SurfaceData::SurfaceData(const std::string& file_, int x_, int y_, int w_, int h_, int use_alpha_)
    : type(LOAD_PART), surface(0), file(file_), use_alpha(use_alpha_),
    x(x_), y(y_), w(w_), h(h_)
{}

SurfaceData::~SurfaceData()
{
  SDL_FreeSurface(surface);
}

SurfaceImpl*
SurfaceData::create()
{
#ifdef PVR_RENDERER
  return create_SurfacePVR();
#else
#ifndef NOOPENGL
  if (use_gl)
    return create_SurfaceOpenGL();
  else
    return create_SurfaceSDL();
#else
  return create_SurfaceSDL();
#endif
#endif
}

SurfaceSDL*
SurfaceData::create_SurfaceSDL()
{
  switch(type)
  {
  case LOAD:
    return new SurfaceSDL(file, use_alpha);
  case LOAD_PART:
    return new SurfaceSDL(file, x, y, w, h, use_alpha);
  case SURFACE:
    return new SurfaceSDL(surface, use_alpha);
  }
  assert(0);
}

SurfaceOpenGL*
SurfaceData::create_SurfaceOpenGL()
{
#ifndef NOOPENGL
  switch(type)
  {
  case LOAD:
    return new SurfaceOpenGL(file, use_alpha);
  case LOAD_PART:
    return new SurfaceOpenGL(file, x, y, w, h, use_alpha);
  case SURFACE:
    return new SurfaceOpenGL(surface, use_alpha);
  }
#endif
  assert(0);
}

#ifdef PVR_RENDERER
SurfacePVR*
SurfaceData::create_SurfacePVR()
{
  switch(type)
  {
  case LOAD:
    return new SurfacePVR(file, use_alpha);
  case LOAD_PART:
    return new SurfacePVR(file, x, y, w, h, use_alpha);
  case SURFACE:
    return new SurfacePVR(surface, use_alpha);
  }
  assert(0);
}
#endif

#ifndef NOOPENGL
struct CachedGLTexture
{
  GLuint texture;
  unsigned int references;
};

typedef std::map<std::string, CachedGLTexture> GLTextureCache;
static GLTextureCache gl_texture_cache;
static bool gl_draw_batch_active = false;
static bool gl_texture_release_safe = false;

static bool acquire_cached_texture(const std::string& key, GLuint* texture)
{
  GLTextureCache::iterator i = gl_texture_cache.find(key);
  if(i == gl_texture_cache.end())
    return false;

  i->second.references++;
  *texture = i->second.texture;
  return true;
}

static void cache_texture(const std::string& key, GLuint texture)
{
  CachedGLTexture cached = { texture, 1 };
  gl_texture_cache[key] = cached;
}

static void release_cached_texture(const std::string& key)
{
  GLTextureCache::iterator i = gl_texture_cache.find(key);
  if(i == gl_texture_cache.end())
    return;

  if(--i->second.references == 0)
  {
    glDeleteTextures(1, &i->second.texture);
    gl_texture_cache.erase(i);
  }
}

/* Quick utility function for texture creation */
static int power_of_two(int input)
{
  int value = 1;

  while ( value < input )
  {
    value <<= 1;
  }
  return value;
}
#endif

Surface::Surface(SDL_Surface* surf, int use_alpha)
    : data(surf, use_alpha), w(0), h(0)
{
  impl = data.create();
  if (impl)
  {
    w = impl->w;
    h = impl->h;
  }
  surfaces.push_back(this);
}

Surface::Surface(const std::string& file, int use_alpha)
    : data(file, use_alpha), w(0), h(0)
{
  impl = data.create();
  if (impl)
  {
    w = impl->w;
    h = impl->h;
  }
  surfaces.push_back(this);
}

Surface::Surface(const std::string& file, int x, int y, int w, int h, int use_alpha)
    : data(file, x, y, w, h, use_alpha), w(0), h(0)
{
  impl = data.create();
  if (impl)
  {
    w = impl->w;
    h = impl->h;
  }
  surfaces.push_back(this);
}

void
Surface::begin_draw_batch()
{
#ifndef NOOPENGL
  if(!use_gl || gl_draw_batch_active)
    return;

  glEnable(GL_TEXTURE_2D);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4ub(255, 255, 255, 255);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_draw_batch_active = true;
#endif
}

void
Surface::end_draw_batch()
{
#ifndef NOOPENGL
  if(!gl_draw_batch_active)
    return;

  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
  gl_draw_batch_active = false;
#endif
}

void
Surface::begin_synchronized_texture_release()
{
#ifdef PVR_RENDERER
  PVRRenderer::wait_for_render();
#else
#ifdef PVR_PIPELINE
  for(int frame = 0; frame < 3; ++frame)
    fadeout();
#endif
#endif
  begin_texture_release();
}

void
Surface::begin_texture_release()
{
#ifdef PVR_RENDERER
  pvr_texture_release_safe = true;
#endif
#ifndef NOOPENGL
  gl_texture_release_safe = true;
#endif
}

void
Surface::end_texture_release()
{
#ifdef PVR_RENDERER
  pvr_texture_release_safe = false;
#endif
#ifndef NOOPENGL
  gl_texture_release_safe = false;
#endif
}

void
Surface::reload()
{
  delete impl;
  impl = data.create();
  if (impl)
  {
    w = impl->w;
    h = impl->h;
  }
}

void
Surface::unprepare()
{
  impl->unprepare();
}

Surface::~Surface()
{
#ifdef DEBUG
  bool found = false;
  for(std::list<Surface*>::iterator i = surfaces.begin(); i != surfaces.end();
      ++i)
  {
    if(*i == this)
    {
      found = true; break;
    }
  }
  if(!found)
    printf("Error: Surface freed twice!!!\n");
#endif
  surfaces.remove(this);
  delete impl;
}

void
Surface::reload_all()
{
  for(Surfaces::iterator i = surfaces.begin(); i != surfaces.end(); ++i)
  {
    (*i)->reload();
  }
}

void
Surface::unprepare_all()
{
  for(Surfaces::iterator i = surfaces.begin(); i != surfaces.end(); ++i)
  (*i)->unprepare();
}

void
Surface::clear_file_cache()
{
  SDL_FreeSurface(source_image_cache);
  source_image_cache = 0;
  source_image_cache_file.clear();
}

void
Surface::debug_check()
{
  for(Surfaces::iterator i = surfaces.begin(); i != surfaces.end(); ++i)
  {
    printf("Surface not freed: T:%d F:%s.\n", (*i)->data.type,
           (*i)->data.file.c_str());
  }
}

void
Surface::draw(float x, float y, Uint8 alpha, bool update)
{
  if (impl)
  {
    if (impl->draw(x, y, alpha, update) == -2)
      reload();
  }
}

void
Surface::draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update)
{
  if (impl)
  {
    if (impl->draw_batch(positions, count, alpha, update) == -2)
      reload();
  }
}

void
Surface::draw_part_batch(float sx, float sy, float w, float h, const float* positions,
                         unsigned int count, Uint8 alpha, bool update)
{
  if (impl)
  {
    if (impl->draw_part_batch(sx, sy, w, h, positions, count, alpha, update) == -2)
      reload();
  }
}

void
Surface::draw_bg(Uint8 alpha, bool update)
{
  if (impl)
  {
    if (impl->draw_bg(alpha, update) == -2)
      reload();
  }
}

void
Surface::draw_part(float sx, float sy, float x, float y, float w, float h,  Uint8 alpha, bool update)
{
  if (impl)
  {
    if (impl->draw_part(sx, sy, x, y, w, h, alpha, update) == -2)
      reload();
  }
}

void
Surface::draw_stretched(float x, float y, int w, int h, Uint8 alpha, bool update)
{
  if (impl)
  {
    if (impl->draw_stretched(x, y, w, h, alpha, update) == -2)
      reload();
  }
}

void
Surface::resize(int w_, int h_)
{
  if (impl)
  {
    w = w_;
    h = h_;
    if (impl->resize(w_,h_) == -2)
      reload();
  }
}

void
Surface::prepare()
{
  if(impl)
    impl->prepare();
}

Surface* Surface::CaptureScreen()
{
  Surface *cap_screen;

  if (!(screen->flags & SDL_OPENGL))
  {
    cap_screen = new Surface(SDL_GetVideoSurface(),false);
  }

#ifndef NOOPENGL
  if (use_gl)
  {
    SDL_Surface *temp;
    unsigned char *pixels;
    int i;
    temp = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h, 24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                0x000000FF, 0x0000FF00, 0x00FF0000, 0
#else
                                0x00FF0000, 0x0000FF00, 0x000000FF, 0
#endif
                               );
    if (temp == NULL)
      st_abort("Error while trying to capture the screen in OpenGL mode","");

    pixels = (unsigned char*) malloc(3 * screen->w * screen->h);
    if (pixels == NULL)
    {
      SDL_FreeSurface(temp);
      st_abort("Error while trying to capture the screen in OpenGL mode","");
    }

    //glReadPixels(0, 0, screen->w, screen->h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    for (i=0; i<screen->h; i++)
      memcpy(((char *) temp->pixels) + temp->pitch * i, pixels + 3*screen->w * (screen->h-i-1), screen->w*3);
    free(pixels);

    cap_screen = new Surface(temp,false);
    SDL_FreeSurface(temp);

  }
#endif
  
return cap_screen;
}

SDL_Surface*
sdl_surface_part_from_file(const std::string& file, int x, int y, int w, int h,  int use_alpha)
{
  SDL_Rect src;
  SDL_Surface * sdl_surface;
  SDL_Surface * temp;
  SDL_Surface * conv;

  temp = load_source_image(file);

  /* Set source rectangle for conv: */

  src.x = x;
  src.y = y;
  src.w = w;
  src.h = h;

  conv = SDL_CreateRGBSurface(temp->flags, w, h, temp->format->BitsPerPixel,
                              temp->format->Rmask,
                              temp->format->Gmask,
                              temp->format->Bmask,
                              temp->format->Amask);

  /* #if SDL_BYTEORDER == SDL_BIG_ENDIAN
     0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
     #else

     0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
     #endif*/

  SDL_SetAlpha(temp,0,0);

  SDL_BlitSurface(temp, &src, conv, NULL);
  if(use_alpha == IGNORE_ALPHA && !use_gl)
    sdl_surface = SDL_DisplayFormat(conv);
  else
    sdl_surface = SDL_DisplayFormatAlpha(conv);

  if (sdl_surface == NULL)
    st_abort("Can't covert to display format (part)", file);

  if (use_alpha == IGNORE_ALPHA && !use_gl)
    SDL_SetAlpha(sdl_surface, 0, 0);

  SDL_FreeSurface(conv);

  return sdl_surface;
}

SDL_Surface*
sdl_surface_from_file(const std::string& file, int use_alpha)
{
  SDL_Surface* sdl_surface;
  SDL_Surface* temp;

  temp = load_source_image(file);

  if(use_alpha == IGNORE_ALPHA && !use_gl)
    sdl_surface = SDL_DisplayFormat(temp);
  else
    sdl_surface = SDL_DisplayFormatAlpha(temp);

  if (sdl_surface == NULL)
    st_abort("Can't covert to display format", file);

  if (use_alpha == IGNORE_ALPHA && !use_gl)
    SDL_SetAlpha(sdl_surface, 0, 0);

  return sdl_surface;
}

SDL_Surface*
sdl_surface_from_sdl_surface(SDL_Surface* sdl_surf, int use_alpha)
{
  SDL_Surface* sdl_surface;
  Uint32 saved_flags;
  Uint8  saved_alpha;

  /* Save the alpha blending attributes */
  saved_flags = sdl_surf->flags&(SDL_SRCALPHA|SDL_RLEACCELOK);
  saved_alpha = sdl_surf->format->alpha;
  if ( (saved_flags & SDL_SRCALPHA)
       == SDL_SRCALPHA )
  {
    SDL_SetAlpha(sdl_surf, 0, 0);
  }

  if(use_alpha == IGNORE_ALPHA && !use_gl)
    sdl_surface = SDL_DisplayFormat(sdl_surf);
  else
    sdl_surface = SDL_DisplayFormatAlpha(sdl_surf);

  /* Restore the alpha blending attributes */
  if ( (saved_flags & SDL_SRCALPHA)
       == SDL_SRCALPHA )
  {
    SDL_SetAlpha(sdl_surface, saved_flags, saved_alpha);
  }

  if (sdl_surface == NULL)
    st_abort("Can't covert to display format", "SURFACE");

  if (use_alpha == IGNORE_ALPHA && !use_gl)
    SDL_SetAlpha(sdl_surface, 0, 0);

  return sdl_surface;
}

//---------------------------------------------------------------------------

SurfaceImpl::SurfaceImpl()
{}

SurfaceImpl::~SurfaceImpl()
{
  SDL_FreeSurface(sdl_surface);
}

SDL_Surface* SurfaceImpl::get_sdl_surface() const
{
  return sdl_surface;
}

int SurfaceImpl::resize(int w_, int h_)
{
  w = w_;
  h = h_;
  SDL_Rect dest;
  dest.x = 0;
  dest.y = 0;
  dest.w = w;
  dest.h = h;
  int ret = SDL_SoftStretch(sdl_surface, NULL,
                            sdl_surface, &dest);
  return ret;
}

#ifndef NOOPENGL
SurfaceOpenGL::SurfaceOpenGL(SDL_Surface* surf, int use_alpha)
  : gl_texture(0), gl_texture_cache_key(), has_alpha(use_alpha == USE_ALPHA)
{
  sdl_surface = sdl_surface_from_sdl_surface(surf, use_alpha);
  this->w = sdl_surface->w;
  this->h = sdl_surface->h;
}

SurfaceOpenGL::SurfaceOpenGL(const std::string& file, int use_alpha)
  : gl_texture(0), has_alpha(use_alpha == USE_ALPHA)
{
  std::ostringstream key;
  key << "file:" << use_alpha << ':' << file;
  gl_texture_cache_key = key.str();

  sdl_surface = sdl_surface_from_file(file, use_alpha);
  this->w = sdl_surface->w;
  this->h = sdl_surface->h;
}

SurfaceOpenGL::SurfaceOpenGL(const std::string& file, int x, int y, int w, int h, int use_alpha)
  : gl_texture(0), has_alpha(use_alpha == USE_ALPHA)
{
  std::ostringstream key;
  key << "part:" << use_alpha << ':' << file << ':' << x << ':' << y << ':' << w << ':' << h;
  gl_texture_cache_key = key.str();

  sdl_surface = sdl_surface_part_from_file(file,x,y,w,h,use_alpha);
  this->w = sdl_surface->w;
  this->h = sdl_surface->h;
}

SurfaceOpenGL::~SurfaceOpenGL()
{
  unprepare();
}

void
SurfaceOpenGL::unprepare()
{
  if(!gl_texture)
    return;

#ifdef PVR_PIPELINE
  if(!gl_texture_release_safe)
    pvr_wait_render_done();
#endif
  if(gl_texture_cache_key.empty())
    glDeleteTextures(1, &gl_texture);
  else
    release_cached_texture(gl_texture_cache_key);

  gl_texture = 0;
}

void
SurfaceOpenGL::ensure_gl()
{
  if(gl_texture)
    return;

  gl_texture_width = power_of_two(w);
  gl_texture_height = power_of_two(h);

  if(!gl_texture_cache_key.empty() &&
     acquire_cached_texture(gl_texture_cache_key, &gl_texture))
    return;

  create_gl(sdl_surface, &gl_texture);
  if(!gl_texture_cache_key.empty())
    cache_texture(gl_texture_cache_key, gl_texture);
}

void
SurfaceOpenGL::create_gl(SDL_Surface * surf, GLuint * tex)
{
  Uint32 saved_flags;
  Uint8  saved_alpha;
  SDL_Surface *conv;

  gl_texture_width = power_of_two(surf->w);
  gl_texture_height = power_of_two(surf->h);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      conv = SDL_CreateRGBSurface(SDL_SWSURFACE, gl_texture_width, gl_texture_height, 32,
                                  0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
#else
      conv = SDL_CreateRGBSurface(SDL_SWSURFACE, gl_texture_width, gl_texture_height, 32,
                                  0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
#endif

  if (conv == NULL)
    st_abort("Can't create OpenGL texture surface", SDL_GetError());

  SDL_FillRect(conv, NULL, 0);

  /* Save the alpha blending attributes */
  saved_flags = surf->flags&(SDL_SRCALPHA|SDL_RLEACCELOK);
  saved_alpha = surf->format->alpha;
  if ( (saved_flags & SDL_SRCALPHA)
       == SDL_SRCALPHA )
  {
    SDL_SetAlpha(surf, 0, 0);
  }

  if (SDL_BlitSurface(surf, 0, conv, 0) < 0)
  {
    SDL_FreeSurface(conv);
    st_abort("Can't convert OpenGL texture", SDL_GetError());
  }

  /* Restore the alpha blending attributes */
  if ( (saved_flags & SDL_SRCALPHA)
       == SDL_SRCALPHA )
  {
    SDL_SetAlpha(surf, saved_flags, saved_alpha);
  }

  glGenTextures(1, &*tex);
  glBindTexture(GL_TEXTURE_2D , *tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gl_texture_width, gl_texture_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, conv->pixels);

  SDL_FreeSurface(conv);
}

void
SurfaceOpenGL::set_alpha_state(Uint8 alpha)
{
  glDisable(GL_ALPHA_TEST);
  if(alpha != 255 || has_alpha)
    {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  else
    {
      glDisable(GL_BLEND);
    }
}

int
SurfaceOpenGL::draw(float x, float y, Uint8 alpha, bool update)
{
  ensure_gl();
  const float tw = (float)w / gl_texture_width;
  const float th = (float)h / gl_texture_height;
  const GLfloat vertices[] = {
    x,     y,     0,
    x + w, y,     0,
    x + w, y + h, 0,
    x,     y + h, 0
  };
  const GLfloat texcoords[] = {
    0,  0,
    tw, 0,
    tw, th,
    0,  th
  };

  glBindTexture(GL_TEXTURE_2D, gl_texture);
  set_alpha_state(alpha);
  glColor4ub(alpha, alpha, alpha, alpha);
  glEnable(GL_TEXTURE_2D);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, vertices);
  glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  (void) update; // avoid compiler warning

  return 0;
}

int
SurfaceOpenGL::draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update)
{
  ensure_gl();
  const float tw = (float)w / gl_texture_width;
  const float th = (float)h / gl_texture_height;
  static std::vector<GLfloat> vertices;
  static std::vector<GLfloat> texcoords;

  vertices.resize(count * 12);
  texcoords.resize(count * 8);
  for(unsigned int i = 0; i < count; ++i)
  {
    const float x = positions[i * 2];
    const float y = positions[i * 2 + 1];
    const unsigned int vertex = i * 12;
    const unsigned int texcoord = i * 8;

    vertices[vertex] = x;
    vertices[vertex + 1] = y;
    vertices[vertex + 2] = 0;
    vertices[vertex + 3] = x + w;
    vertices[vertex + 4] = y;
    vertices[vertex + 5] = 0;
    vertices[vertex + 6] = x + w;
    vertices[vertex + 7] = y + h;
    vertices[vertex + 8] = 0;
    vertices[vertex + 9] = x;
    vertices[vertex + 10] = y + h;
    vertices[vertex + 11] = 0;

    texcoords[texcoord] = 0;
    texcoords[texcoord + 1] = 0;
    texcoords[texcoord + 2] = tw;
    texcoords[texcoord + 3] = 0;
    texcoords[texcoord + 4] = tw;
    texcoords[texcoord + 5] = th;
    texcoords[texcoord + 6] = 0;
    texcoords[texcoord + 7] = th;
  }

  if(!gl_draw_batch_active)
  {
    glEnable(GL_TEXTURE_2D);
    set_alpha_state(alpha);
    glColor4ub(alpha, alpha, alpha, alpha);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  glBindTexture(GL_TEXTURE_2D, gl_texture);

  glVertexPointer(3, GL_FLOAT, 0, &vertices[0]);
  glTexCoordPointer(2, GL_FLOAT, 0, &texcoords[0]);
  glDrawArrays(GL_QUADS, 0, count * 4);
  if(!gl_draw_batch_active)
  {
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
  }

  (void) update;
  return 0;
}

int
SurfaceOpenGL::draw_part_batch(float sx, float sy, float part_w, float part_h,
                               const float* positions, unsigned int count,
                               Uint8 alpha, bool update)
{
  ensure_gl();
  const float pw = gl_texture_width;
  const float ph = gl_texture_height;
  const float left = sx / pw;
  const float top = sy / ph;
  const float right = (sx + part_w) / pw;
  const float bottom = (sy + part_h) / ph;
  static std::vector<GLfloat> vertices;
  static std::vector<GLfloat> texcoords;

  vertices.resize(count * 12);
  texcoords.resize(count * 8);
  for(unsigned int i = 0; i < count; ++i)
  {
    const float x = positions[i * 2];
    const float y = positions[i * 2 + 1];
    const unsigned int vertex = i * 12;
    const unsigned int texcoord = i * 8;

    vertices[vertex] = x;
    vertices[vertex + 1] = y;
    vertices[vertex + 2] = 0;
    vertices[vertex + 3] = x + part_w;
    vertices[vertex + 4] = y;
    vertices[vertex + 5] = 0;
    vertices[vertex + 6] = x + part_w;
    vertices[vertex + 7] = y + part_h;
    vertices[vertex + 8] = 0;
    vertices[vertex + 9] = x;
    vertices[vertex + 10] = y + part_h;
    vertices[vertex + 11] = 0;

    texcoords[texcoord] = left;
    texcoords[texcoord + 1] = top;
    texcoords[texcoord + 2] = right;
    texcoords[texcoord + 3] = top;
    texcoords[texcoord + 4] = right;
    texcoords[texcoord + 5] = bottom;
    texcoords[texcoord + 6] = left;
    texcoords[texcoord + 7] = bottom;
  }

  glEnable(GL_TEXTURE_2D);
  set_alpha_state(alpha);
  glColor4ub(alpha, alpha, alpha, alpha);
  glBindTexture(GL_TEXTURE_2D, gl_texture);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, &vertices[0]);
  glTexCoordPointer(2, GL_FLOAT, 0, &texcoords[0]);
  glDrawArrays(GL_QUADS, 0, count * 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  (void) update;
  return 0;
}

int
SurfaceOpenGL::draw_bg(Uint8 alpha, bool update)
{
  ensure_gl();
  const float tw = (float)w / gl_texture_width;
  const float th = (float)h / gl_texture_height;
  const GLfloat vertices[] = {
    0,         0,         0,
    screen->w, 0,         0,
    screen->w, screen->h, 0,
    0,         screen->h, 0
  };
  const GLfloat texcoords[] = {
    0,  0,
    tw, 0,
    tw, th,
    0,  th
  };

  glBindTexture(GL_TEXTURE_2D, gl_texture);
  set_alpha_state(alpha);
  glColor4ub(alpha, alpha, alpha, alpha);
  glEnable(GL_TEXTURE_2D);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, vertices);
  glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  (void) update; // avoid compiler warning

  return 0;
}

int
SurfaceOpenGL::draw_part(float sx, float sy, float x, float y, float w, float h, Uint8 alpha, bool update)
{
  ensure_gl();
  float pw = gl_texture_width;
  float ph = gl_texture_height;
  const GLfloat vertices[] = {
    x,     y,     0,
    x + w, y,     0,
    x + w, y + h, 0,
    x,     y + h, 0
  };
  const GLfloat texcoords[] = {
    sx / pw,       sy / ph,
    (sx + w) / pw, sy / ph,
    (sx + w) / pw, (sy + h) / ph,
    sx / pw,       (sy + h) / ph
  };

  glBindTexture(GL_TEXTURE_2D, gl_texture);

  set_alpha_state(alpha);

  glColor4ub(alpha, alpha, alpha, alpha);

  glEnable(GL_TEXTURE_2D);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, vertices);
  glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  (void) update; // avoid warnings
  return 0;
}

int
SurfaceOpenGL::draw_stretched(float x, float y, int sw, int sh, Uint8 alpha, bool update)
{
  ensure_gl();
  const float tw = (float)w / gl_texture_width;
  const float th = (float)h / gl_texture_height;
  const GLfloat vertices[] = {
    x,      y,      0,
    x + sw, y,      0,
    x + sw, y + sh, 0,
    x,      y + sh, 0
  };
  const GLfloat texcoords[] = {
    0,  0,
    tw, 0,
    tw, th,
    0,  th
  };

  glBindTexture(GL_TEXTURE_2D, gl_texture);
  set_alpha_state(alpha);
  glColor4ub(alpha, alpha, alpha, alpha);
  glEnable(GL_TEXTURE_2D);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, vertices);
  glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);

  (void) update; // avoid warnings
  return 0;
}

#endif

#ifdef PVR_RENDERER
struct CachedPVRTexture
{
  pvr_ptr_t texture;
  unsigned int references;
};

typedef std::map<std::string, CachedPVRTexture> PVRTextureCache;
static PVRTextureCache pvr_texture_cache;

void
Surface::print_memory_stats(const char* label)
{
  struct mallinfo heap = mallinfo();
  unsigned int references = 0;
  for(PVRTextureCache::const_iterator i = pvr_texture_cache.begin();
      i != pvr_texture_cache.end(); ++i)
    references += i->second.references;

  printf("Memory %s: pvr_free=%lu pvr_cache=%lu pvr_refs=%u "
         "heap_used=%d heap_free=%d surfaces=%lu\n",
         label, (unsigned long)pvr_mem_available(),
         (unsigned long)pvr_texture_cache.size(), references,
         heap.uordblks, heap.fordblks, (unsigned long)surfaces.size());
}

static bool acquire_cached_pvr_texture(const std::string& key, pvr_ptr_t* texture)
{
  PVRTextureCache::iterator i = pvr_texture_cache.find(key);
  if(i == pvr_texture_cache.end())
    return false;

  i->second.references++;
  *texture = i->second.texture;
  return true;
}

static void cache_pvr_texture(const std::string& key, pvr_ptr_t texture)
{
  CachedPVRTexture cached = { texture, 1 };
  pvr_texture_cache[key] = cached;
}

static void release_cached_pvr_texture(const std::string& key)
{
  PVRTextureCache::iterator i = pvr_texture_cache.find(key);
  if(i == pvr_texture_cache.end())
    return;

  if(--i->second.references == 0)
    {
      PVRRenderer::free_texture(i->second.texture);
      pvr_texture_cache.erase(i);
    }
}

SurfacePVR::SurfacePVR(SDL_Surface* surf, int use_alpha)
  : pvr_texture(0), pvr_texture_width(0), pvr_texture_height(0)
{
  sdl_surface = sdl_surface_from_sdl_surface(surf, use_alpha);
  w = sdl_surface->w;
  h = sdl_surface->h;
}

SurfacePVR::SurfacePVR(const std::string& file, int use_alpha)
  : pvr_texture(0), pvr_texture_width(0), pvr_texture_height(0)
{
  std::ostringstream key;
  key << "file:" << use_alpha << ':' << file;
  pvr_texture_cache_key = key.str();
  sdl_surface = sdl_surface_from_file(file, use_alpha);
  w = sdl_surface->w;
  h = sdl_surface->h;
}

SurfacePVR::SurfacePVR(const std::string& file, int x, int y, int width, int height, int use_alpha)
  : pvr_texture(0), pvr_texture_width(0), pvr_texture_height(0)
{
  std::ostringstream key;
  key << "part:" << use_alpha << ':' << file << ':' << x << ':' << y << ':' << width << ':' << height;
  pvr_texture_cache_key = key.str();
  sdl_surface = sdl_surface_part_from_file(file, x, y, width, height, use_alpha);
  w = sdl_surface->w;
  h = sdl_surface->h;
}

SurfacePVR::~SurfacePVR()
{
  unprepare();
}

void
SurfacePVR::ensure_pvr()
{
  if(pvr_texture)
    return;

  pvr_texture_width = power_of_two(w);
  pvr_texture_height = power_of_two(h);
  pvr_ptr_t texture = 0;
  if(!pvr_texture_cache_key.empty() &&
     acquire_cached_pvr_texture(pvr_texture_cache_key, &texture))
    {
      pvr_texture = texture;
      return;
    }

  texture = PVRRenderer::upload_texture(sdl_surface, pvr_texture_width, pvr_texture_height);
  if(!texture)
    st_abort("No PVR texture memory left.", pvr_texture_cache_key);

  pvr_texture = texture;
  if(!pvr_texture_cache_key.empty())
    cache_pvr_texture(pvr_texture_cache_key, texture);
}

void
SurfacePVR::prepare()
{
  ensure_pvr();
}

void
SurfacePVR::unprepare()
{
  if(!pvr_texture)
    return;

  if(!pvr_texture_release_safe)
    PVRRenderer::wait_for_render();

  if(pvr_texture_cache_key.empty())
    PVRRenderer::free_texture(static_cast<pvr_ptr_t>(pvr_texture));
  else
    release_cached_pvr_texture(pvr_texture_cache_key);
  pvr_texture = 0;
}

int
SurfacePVR::draw(float x, float y, Uint8 alpha, bool update)
{
  ensure_pvr();
  PVRRenderer::draw_texture(static_cast<pvr_ptr_t>(pvr_texture),
                            pvr_texture_width, pvr_texture_height,
                            0, 0, w, h, x, y, w, h, alpha);
  (void)update;
  return 0;
}

int
SurfacePVR::draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update)
{
  ensure_pvr();
  PVRRenderer::draw_texture_batch(static_cast<pvr_ptr_t>(pvr_texture),
                                  pvr_texture_width, pvr_texture_height,
                                  0, 0, w, h, positions, count, w, h, alpha);
  (void)update;
  return 0;
}

int
SurfacePVR::draw_part_batch(float sx, float sy, float width, float height,
                            const float* positions, unsigned int count,
                            Uint8 alpha, bool update)
{
  ensure_pvr();
  PVRRenderer::draw_texture_batch(static_cast<pvr_ptr_t>(pvr_texture),
                                  pvr_texture_width, pvr_texture_height,
                                  sx, sy, width, height, positions, count,
                                  width, height, alpha);
  (void)update;
  return 0;
}

int
SurfacePVR::draw_bg(Uint8 alpha, bool update)
{
  ensure_pvr();
  PVRRenderer::draw_texture(static_cast<pvr_ptr_t>(pvr_texture),
                            pvr_texture_width, pvr_texture_height,
                            0, 0, w, h, 0, 0, screen->w, screen->h, alpha);
  (void)update;
  return 0;
}

int
SurfacePVR::draw_part(float sx, float sy, float x, float y,
                      float width, float height, Uint8 alpha, bool update)
{
  ensure_pvr();
  PVRRenderer::draw_texture(static_cast<pvr_ptr_t>(pvr_texture),
                            pvr_texture_width, pvr_texture_height,
                            sx, sy, width, height, x, y, width, height, alpha);
  (void)update;
  return 0;
}

int
SurfacePVR::draw_stretched(float x, float y, int width, int height, Uint8 alpha, bool update)
{
  ensure_pvr();
  PVRRenderer::draw_texture(static_cast<pvr_ptr_t>(pvr_texture),
                            pvr_texture_width, pvr_texture_height,
                            0, 0, w, h, x, y, width, height, alpha);
  (void)update;
  return 0;
}
#endif

#if defined(__DREAMCAST__) && !defined(PVR_RENDERER)
void
Surface::print_memory_stats(const char* label)
{
  GLint free_texture_memory = 0;
  GLint contiguous_texture_memory = 0;
  struct mallinfo heap = mallinfo();
  glGetIntegerv(GL_FREE_TEXTURE_MEMORY_KOS, &free_texture_memory);
  glGetIntegerv(GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS, &contiguous_texture_memory);
  printf("Memory %s: gldc_free=%ld gldc_contiguous=%ld "
         "heap_used=%d heap_free=%d surfaces=%lu\n",
         label, (long)free_texture_memory, (long)contiguous_texture_memory,
         heap.uordblks, heap.fordblks, (unsigned long)surfaces.size());
}
#endif

SurfaceSDL::SurfaceSDL(SDL_Surface* surf, int use_alpha)
{
  sdl_surface = sdl_surface_from_sdl_surface(surf, use_alpha);
  w = sdl_surface->w;
  h = sdl_surface->h;
}

SurfaceSDL::SurfaceSDL(const std::string& file, int use_alpha)
{
  sdl_surface = sdl_surface_from_file(file, use_alpha);
  w = sdl_surface->w;
  h = sdl_surface->h;
}

SurfaceSDL::SurfaceSDL(const std::string& file, int x, int y, int w, int h,  int use_alpha)
{
  sdl_surface = sdl_surface_part_from_file(file, x, y, w, h, use_alpha);
  w = sdl_surface->w;
  h = sdl_surface->h;
}

int
SurfaceSDL::draw(float x, float y, Uint8 alpha, bool update)
{
  SDL_Rect dest;

  dest.x = (int)x;
  dest.y = (int)y;
  dest.w = w;
  dest.h = h;

  if(alpha != 255)
    {
    /* Create a Surface, make it using colorkey, blit surface into temp, apply alpha
      to temp sur, blit the temp into the screen */
    /* Note: this has to be done, since SDL doesn't allow to set alpha to surfaces that
      already have an alpha mask yet... */

    SDL_Surface* sdl_surface_copy = SDL_CreateRGBSurface (sdl_surface->flags,
                                    sdl_surface->w, sdl_surface->h, sdl_surface->format->BitsPerPixel,
                                    sdl_surface->format->Rmask, sdl_surface->format->Gmask,
                                    sdl_surface->format->Bmask,
                                    0);
    int colorkey = SDL_MapRGB(sdl_surface_copy->format, 255, 0, 255);
    SDL_FillRect(sdl_surface_copy, NULL, colorkey);
    SDL_SetColorKey(sdl_surface_copy, SDL_SRCCOLORKEY, colorkey);


    SDL_BlitSurface(sdl_surface, NULL, sdl_surface_copy, NULL);
    SDL_SetAlpha(sdl_surface_copy ,SDL_SRCALPHA,alpha);

    int ret = SDL_BlitSurface(sdl_surface_copy, NULL, screen, &dest);

    if (update == UPDATE)
      SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);

    SDL_FreeSurface (sdl_surface_copy);
    return ret;
    }

  int ret = SDL_BlitSurface(sdl_surface, NULL, screen, &dest);

  if (update == UPDATE)
    SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);

  return ret;
}

int
SurfaceSDL::draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update)
{
  for(unsigned int i = 0; i < count; ++i)
    draw(positions[i * 2], positions[i * 2 + 1], alpha, update);
  return 0;
}

int
SurfaceSDL::draw_part_batch(float sx, float sy, float w, float h,
                            const float* positions, unsigned int count,
                            Uint8 alpha, bool update)
{
  for(unsigned int i = 0; i < count; ++i)
    draw_part(sx, sy, positions[i * 2], positions[i * 2 + 1], w, h, alpha, update);
  return 0;
}

int
SurfaceSDL::draw_bg(Uint8 alpha, bool update)
{
  SDL_Rect dest;

  dest.x = 0;
  dest.y = 0;
  dest.w = screen->w;
  dest.h = screen->h;

  if(alpha != 255)
    {
    /* Create a Surface, make it using colorkey, blit surface into temp, apply alpha
      to temp sur, blit the temp into the screen */
    /* Note: this has to be done, since SDL doesn't allow to set alpha to surfaces that
      already have an alpha mask yet... */

    SDL_Surface* sdl_surface_copy = SDL_CreateRGBSurface (sdl_surface->flags,
                                    sdl_surface->w, sdl_surface->h, sdl_surface->format->BitsPerPixel,
                                    sdl_surface->format->Rmask, sdl_surface->format->Gmask,
                                    sdl_surface->format->Bmask,
                                    0);
    int colorkey = SDL_MapRGB(sdl_surface_copy->format, 255, 0, 255);
    SDL_FillRect(sdl_surface_copy, NULL, colorkey);
    SDL_SetColorKey(sdl_surface_copy, SDL_SRCCOLORKEY, colorkey);


    SDL_BlitSurface(sdl_surface, NULL, sdl_surface_copy, NULL);
    SDL_SetAlpha(sdl_surface_copy ,SDL_SRCALPHA,alpha);

    int ret = SDL_BlitSurface(sdl_surface_copy, NULL, screen, &dest);

    if (update == UPDATE)
      SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);

    SDL_FreeSurface (sdl_surface_copy);
    return ret;
    }

  int ret = SDL_SoftStretch(sdl_surface, NULL, screen, &dest);

  if (update == UPDATE)
    SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);

  return ret;
}

int
SurfaceSDL::draw_part(float sx, float sy, float x, float y, float w, float h, Uint8 alpha, bool update)
{
  SDL_Rect src, dest;

  src.x = (int)sx;
  src.y = (int)sy;
  src.w = (int)w;
  src.h = (int)h;

  dest.x = (int)x;
  dest.y = (int)y;
  dest.w = (int)w;
  dest.h = (int)h;

  if(alpha != 255)
    {
    /* Create a Surface, make it using colorkey, blit surface into temp, apply alpha
      to temp sur, blit the temp into the screen */
    /* Note: this has to be done, since SDL doesn't allow to set alpha to surfaces that
      already have an alpha mask yet... */

    SDL_Surface* sdl_surface_copy = SDL_CreateRGBSurface (sdl_surface->flags,
                                    sdl_surface->w, sdl_surface->h, sdl_surface->format->BitsPerPixel,
                                    sdl_surface->format->Rmask, sdl_surface->format->Gmask,
                                    sdl_surface->format->Bmask,
                                    0);
    int colorkey = SDL_MapRGB(sdl_surface_copy->format, 255, 0, 255);
    SDL_FillRect(sdl_surface_copy, NULL, colorkey);
    SDL_SetColorKey(sdl_surface_copy, SDL_SRCCOLORKEY, colorkey);


    SDL_BlitSurface(sdl_surface, NULL, sdl_surface_copy, NULL);
    SDL_SetAlpha(sdl_surface_copy ,SDL_SRCALPHA,alpha);

    int ret = SDL_BlitSurface(sdl_surface_copy, NULL, screen, &dest);

    if (update == UPDATE)
      SDL_UpdateRect(screen, dest.x, dest.y, dest.w, dest.h);

    SDL_FreeSurface (sdl_surface_copy);
    return ret;
    }

  int ret = SDL_BlitSurface(sdl_surface, &src, screen, &dest);

  if (update == UPDATE)
    update_rect(screen, dest.x, dest.y, dest.w, dest.h);

  return ret;
}

int
SurfaceSDL::draw_stretched(float x, float y, int sw, int sh, Uint8 alpha, bool update)
{
  SDL_Rect dest;

  dest.x = (int)x;
  dest.y = (int)y;
  dest.w = (int)sw;
  dest.h = (int)sh;

  if(alpha != 255)
    SDL_SetAlpha(sdl_surface ,SDL_SRCALPHA,alpha);


  SDL_Surface* sdl_surface_copy = SDL_CreateRGBSurface (sdl_surface->flags,
                                  sw, sh, sdl_surface->format->BitsPerPixel,
                                  sdl_surface->format->Rmask, sdl_surface->format->Gmask,
                                  sdl_surface->format->Bmask,
                                  0);

  SDL_BlitSurface(sdl_surface, NULL, sdl_surface_copy, NULL);
  SDL_SoftStretch(sdl_surface_copy, NULL, sdl_surface_copy, &dest);

  int ret = SDL_BlitSurface(sdl_surface_copy,NULL,screen,&dest);
  SDL_FreeSurface(sdl_surface_copy);

  if (update == UPDATE)
    update_rect(screen, dest.x, dest.y, dest.w, dest.h);

  return ret;
}

SurfaceSDL::~SurfaceSDL()
{}

/* EOF */
