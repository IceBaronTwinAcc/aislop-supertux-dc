//  $Id: texture.h 1053 2004-05-09 18:08:02Z tobgle $
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

#ifndef SUPERTUX_TEXTURE_H
#define SUPERTUX_TEXTURE_H

#include <SDL.h>
#include <string>
#ifndef NOOPENGL
#ifdef __DREAMCAST__
#include <GL/gl.h>
#else
#include <SDL_opengl.h>
#endif
#endif

#include <list>
#include "screen.h"

SDL_Surface* sdl_surface_from_sdl_surface(SDL_Surface* sdl_surf, int use_alpha);

class SurfaceImpl;
class SurfaceSDL;
class SurfaceOpenGL;
#ifdef PVR_RENDERER
class SurfacePVR;
#endif

/** This class holds all the data necessary to construct a surface */
class SurfaceData 
{
public:
  enum ConstructorType { LOAD, LOAD_PART, SURFACE };
  ConstructorType type;
  SDL_Surface* surface;
  std::string file;
  int use_alpha;
  int x;
  int y;
  int w;
  int h;

  SurfaceData(SDL_Surface* surf, int use_alpha_);
  SurfaceData(const std::string& file_, int use_alpha_);
  SurfaceData(const std::string& file_, int x_, int y_, int w_, int h_, int use_alpha_);
  ~SurfaceData();

  SurfaceSDL* create_SurfaceSDL();
  SurfaceOpenGL* create_SurfaceOpenGL();
#ifdef PVR_RENDERER
  SurfacePVR* create_SurfacePVR();
#endif
  SurfaceImpl* create();
};

/** Container class that holds a surface, necessary so that we can
    switch Surface implementations (OpenGL, SDL) on the fly */
class Surface
{
public:
  SurfaceData data;
  SurfaceImpl* impl;
  int w; 
  int h;
  
  typedef std::list<Surface*> Surfaces;
  static Surfaces surfaces;
public:
  static void reload_all();
  static void unprepare_all();
  static void clear_file_cache();
  static void debug_check();
  static void begin_draw_batch();
  static void end_draw_batch();
  static void begin_synchronized_texture_release();
  static void begin_texture_release();
  static void end_texture_release();
#ifdef __DREAMCAST__
  static void print_memory_stats(const char* label);
#endif

  Surface(SDL_Surface* surf, int use_alpha);  
  Surface(const std::string& file, int use_alpha);  
  Surface(const std::string& file, int x, int y, int w, int h, int use_alpha);
  ~Surface();
  
  /** Captures the screen and returns it as Surface*, the user is expected to call the destructor. */
  static Surface* CaptureScreen();
  
  /** Reload the surface, which is necesarry in case of a mode swich */
  void reload();

  void draw(float x, float y, Uint8 alpha = 255, bool update = false);
  void draw_batch(const float* positions, unsigned int count, Uint8 alpha = 255, bool update = false);
  void draw_part_batch(float sx, float sy, float w, float h, const float* positions,
                       unsigned int count, Uint8 alpha = 255, bool update = false);
  void draw_bg(Uint8 alpha = 255, bool update = false);
  void draw_part(float sx, float sy, float x, float y, float w, float h,  Uint8 alpha = 255, bool update = false);
  void draw_stretched(float x, float y, int w, int h, Uint8 alpha, bool update = false);
  void resize(int w_, int h_);
  void prepare();
  void unprepare();
};

/** Surface implementation, all implementation have to inherit from
    this class */
class SurfaceImpl
{
protected:
  SDL_Surface* sdl_surface;

public:
  int w;
  int h;

public:
  SurfaceImpl();
  virtual ~SurfaceImpl();
  
  /** Return 0 on success, -2 if surface needs to be reloaded */
  virtual int draw(float x, float y, Uint8 alpha, bool update) = 0;
  virtual int draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update) = 0;
  virtual int draw_part_batch(float sx, float sy, float w, float h, const float* positions,
                              unsigned int count, Uint8 alpha, bool update) = 0;
  virtual int draw_bg(Uint8 alpha, bool update) = 0;
  virtual int draw_part(float sx, float sy, float x, float y, float w, float h,  Uint8 alpha, bool update) = 0;
  virtual int draw_stretched(float x, float y, int w, int h, Uint8 alpha, bool update) = 0;
  int resize(int w_, int h_);
  virtual void prepare() = 0;
  virtual void unprepare() = 0;

  SDL_Surface* get_sdl_surface() const; // @evil@ try to avoid this function
};

class SurfaceSDL : public SurfaceImpl
{
public:
  SurfaceSDL(SDL_Surface* surf, int use_alpha);
  SurfaceSDL(const std::string& file, int use_alpha);  
  SurfaceSDL(const std::string& file, int x, int y, int w, int h, int use_alpha);
  virtual ~SurfaceSDL();

  int draw(float x, float y, Uint8 alpha, bool update);
  int draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update);
  int draw_part_batch(float sx, float sy, float w, float h, const float* positions,
                      unsigned int count, Uint8 alpha, bool update);
  int draw_bg(Uint8 alpha, bool update);
  int draw_part(float sx, float sy, float x, float y, float w, float h,  Uint8 alpha, bool update);
  int draw_stretched(float x, float y, int w, int h, Uint8 alpha, bool update);
  void prepare() {}
  void unprepare() {}
};

#ifdef PVR_RENDERER
class SurfacePVR : public SurfaceImpl
{
public:
  SurfacePVR(SDL_Surface* surf, int use_alpha);
  SurfacePVR(const std::string& file, int use_alpha);
  SurfacePVR(const std::string& file, int x, int y, int w, int h, int use_alpha);
  virtual ~SurfacePVR();

  int draw(float x, float y, Uint8 alpha, bool update);
  int draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update);
  int draw_part_batch(float sx, float sy, float w, float h, const float* positions,
                      unsigned int count, Uint8 alpha, bool update);
  int draw_bg(Uint8 alpha, bool update);
  int draw_part(float sx, float sy, float x, float y, float w, float h, Uint8 alpha, bool update);
  int draw_stretched(float x, float y, int w, int h, Uint8 alpha, bool update);
  void prepare();
  void unprepare();

private:
  void ensure_pvr();
  void* pvr_texture;
  std::string pvr_texture_cache_key;
  int pvr_texture_width;
  int pvr_texture_height;
};
#endif

#ifndef NOOPENGL
class SurfaceOpenGL : public SurfaceImpl
{
public:
  unsigned gl_texture;

public:
  SurfaceOpenGL(SDL_Surface* surf, int use_alpha);
  SurfaceOpenGL(const std::string& file, int use_alpha);  
  SurfaceOpenGL(const std::string& file, int x, int y, int w, int h, int use_alpha);
  virtual ~SurfaceOpenGL();

  int draw(float x, float y, Uint8 alpha, bool update);
  int draw_batch(const float* positions, unsigned int count, Uint8 alpha, bool update);
  int draw_part_batch(float sx, float sy, float w, float h, const float* positions,
                      unsigned int count, Uint8 alpha, bool update);
  int draw_bg(Uint8 alpha, bool update);
  int draw_part(float sx, float sy, float x, float y, float w, float h,  Uint8 alpha, bool update);
  int draw_stretched(float x, float y, int w, int h, Uint8 alpha, bool update);
  void prepare() { ensure_gl(); }
  void unprepare();

private:
  void ensure_gl();
  void create_gl(SDL_Surface * surf, GLuint * tex);
  void set_alpha_state(Uint8 alpha);
  std::string gl_texture_cache_key;
  int gl_texture_width;
  int gl_texture_height;
  bool has_alpha;
};
#endif 

#endif /*SUPERTUX_TEXTURE_H*/

/* Local Variables: */
/* mode: c++ */
/* End: */
