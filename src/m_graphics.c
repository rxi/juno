/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <SDL/SDL.h>
#include "lib/sera/sera.h"
#include "util.h"
#include "luax.h"
#include "m_buffer.h"

double m_graphics_maxFps = 60.;

static int inited = 0;
static int screenWidth = 0;
static int screenHeight = 0;
static int screenRef = 0;
static int fullscreen = 0;
static int resizable = 0;
Buffer *screen;


static void resetVideoMode(lua_State *L) {
  /* Reset video mode */
  int flags = (fullscreen ? SDL_FULLSCREEN : 0) |
              (resizable  ? SDL_RESIZABLE : 0);
  if (SDL_SetVideoMode(screenWidth, screenHeight, 32, flags) == NULL) {
    luaL_error(L, "could not set video mode");
  }
  /* Reset screen buffer */
  if (screen) {
    sr_Buffer *b = screen->buffer;
    b->pixels = (void*) SDL_GetVideoSurface()->pixels;
    b->w = screenWidth;
    b->h = screenHeight;
    sr_setClip(b, sr_rect(0, 0, b->w, b->h));
  }
}


static int l_graphics_init(lua_State *L) {
  screenWidth = luaL_checkint(L, 1);
  screenHeight = luaL_checkint(L, 2);
  const char *title = luaL_optstring(L, 3, "Juno");
  fullscreen = luax_optboolean(L, 4, 0);
  resizable = luax_optboolean(L, 5, 0);
  if (inited) {
    luaL_error(L, "graphics are already inited");
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    luaL_error(L, "could not init video");
  }
  /* Init SDL video */
  resetVideoMode(L);
  /* Required to get the associated character when a key is pressed. This has
   * to be enabled *after* SDL video is set up */
  SDL_EnableUNICODE(1);
  /* Init window title */
  SDL_WM_SetCaption(title, title);
  /* Create, store in registry and return main screen buffer */
  screen = buffer_new(L);
  screen->buffer = sr_newBufferShared(
    SDL_GetVideoSurface()->pixels, screenWidth, screenHeight);
  lua_pushvalue(L, -1);
  screenRef = lua_ref(L, LUA_REGISTRYINDEX);
  /* Set state */
  inited = 1;
  return 1;
}


static int l_graphics_setFullscreen(lua_State *L) {
  fullscreen = luax_optboolean(L, 1, 0);
  resetVideoMode(L);
  return 0;
}


static int l_graphics_setMaxFps(lua_State *L) {
  m_graphics_maxFps = luaL_optnumber(L, 1, 60);
  return 0;
}


int luaopen_graphics(lua_State *L) {
  luaL_Reg reg[] = {
    { "init",           l_graphics_init           },
    { "setFullscreen",  l_graphics_setFullscreen  },
    { "setMaxFps",      l_graphics_setMaxFps      },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);
  return 1;
}
