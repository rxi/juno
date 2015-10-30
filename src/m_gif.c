/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "lib/jo_gif.c"
#include "util.h"
#include "luax.h"
#include "m_buffer.h"

#define CLASS_NAME "Gif"

typedef struct {
  int state;
  jo_gif_t gif;
  int w, h;
  unsigned char *buf;
} Gif;

enum {
  STATE_INIT,
  STATE_ACTIVE,
  STATE_CLOSED,
};

static int l_gif_new(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  int w = luaL_checknumber(L, 2);
  int h = luaL_checknumber(L, 3);
  int ncolors = luaL_optnumber(L, 4, 63);
  Gif *self = lua_newuserdata(L, sizeof(*self));
  luaL_setmetatable(L, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  self->state = STATE_INIT;
  self->w = w;
  self->h = h;
  self->buf = malloc(w * h * 4);
  ASSERT(self->buf);
  memset(self->buf, 0, w * h * 4);
  /* Activate gif */
  self->gif = jo_gif_start(filename, self->w, self->h, 0, ncolors);
  self->state = STATE_ACTIVE;
  return 1;
}


static int l_gif_gc(lua_State *L) {
  Gif *self = luaL_checkudata(L, 1, CLASS_NAME);
  free(self->buf);
  /* Not closed? close now */
  if (self->state == STATE_ACTIVE) {
    jo_gif_end(&self->gif);
  }
  return 0;
}


static int l_gif_update(lua_State *L) {
  Gif *self = luaL_checkudata(L, 1, CLASS_NAME);
  Buffer *buf = luaL_checkudata(L, 2, BUFFER_CLASS_NAME);
  int delay = luaL_checknumber(L, 3);
  /* Already closed? Error */
  if (self->state == STATE_CLOSED) {
    luaL_error(L, "can't update closed gif");
  }
  /* Buffer dimensions are okay? */
  if (buf->buffer->w != self->w || buf->buffer->h != self->h) {
    luaL_error(L, "bad buffer dimensions for gif object, expected %dx%d",
               self->w, self->h);
  }
  /* Copy pixels to buffer -- jo_gif expects a specific channel byte-order
   * which may differ from what sera is using -- alpha channel isn't copied
   * since jo_gif doesn't use this */
  int i, n;
  int len = self->w * self->h;
  sr_Pixel *p = buf->buffer->pixels;
  for (i = 0; i < len; i++) {
    n = i * 4;
    self->buf[n    ] = p[i].rgba.r;
    self->buf[n + 1] = p[i].rgba.g;
    self->buf[n + 2] = p[i].rgba.b;
  }
  /* Update */
  jo_gif_frame(&self->gif, self->buf, delay, 0);
  return 0;
}


static int l_gif_close(lua_State *L) {
  Gif *self = luaL_checkudata(L, 1, CLASS_NAME);
  if (self->state == STATE_CLOSED) {
    luaL_error(L, "state already closed");
  }
  self->state = STATE_CLOSED;
  jo_gif_end(&self->gif);
  return 0;
}


int luaopen_gif(lua_State *L) {
  luaL_Reg reg[] = {
    { "__gc",   l_gif_gc     },
    { "new",    l_gif_new    },
    { "update", l_gif_update },
    { "close",  l_gif_close  },
    { NULL, NULL }
  };
  ASSERT( luaL_newmetatable(L, CLASS_NAME) );
  luaL_setfuncs(L, reg, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  return 1;
}
