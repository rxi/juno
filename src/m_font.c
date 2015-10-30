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
#include "lib/sera/sera.h"
#include "util.h"
#include "luax.h"
#include "fs.h"
#include "ttf.h"
#include "m_buffer.h"

#define CLASS_NAME "Font"
#define DEFAULT_FONTSIZE 14

typedef struct {
  ttf_Font *font;
} Font;


static Font *newFont(lua_State *L) {
  Font *self = lua_newuserdata(L, sizeof(*self));
  luaL_setmetatable(L, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  return self;
}

static const char *loadFontFromMemory(
  Font *self, const void *data, int len, int ptsize
) {
  self->font = ttf_new(data, len);
  if (!self->font) {
    return "could not load font";
  }
  ttf_ptsize(self->font, ptsize);
  return NULL;
}


static int l_font_fromFile(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  int fontsize = luaL_optint(L, 2, DEFAULT_FONTSIZE);
  Font *self = newFont(L);
  size_t len;
  void *data = fs_read(filename, &len);
  /* Load new font */
  if (!data) {
    luaL_error(L, "could not open file '%s'", filename);
  }
  const char *err = loadFontFromMemory(self, data, len, fontsize);
  free(data);
  if (err) luaL_error(L, "%s", err);
  return 1;
}


static int l_font_fromString(lua_State *L) {
  size_t len;
  const char *data = luaL_checklstring(L, 1, &len);
  int fontsize = luaL_optint(L, 2, DEFAULT_FONTSIZE);
  Font *self = newFont(L);
  const char *err = loadFontFromMemory(self, data, len, fontsize);
  if (err) luaL_error(L, "%s", err);
  return 1;
}


static int l_font_fromEmbedded(lua_State *L) {
  #include "font_ttf.h"
  int fontsize = luaL_optint(L, 1, DEFAULT_FONTSIZE);
  Font *self = newFont(L);
  const char *err = loadFontFromMemory(self, font_ttf, sizeof(font_ttf), 
                                       fontsize);
  if (err) luaL_error(L, "%s", err);
  return 1;
}


static int l_font_gc(lua_State *L) {
  Font *self = luaL_checkudata(L, 1, CLASS_NAME);
  if (self->font) {
    ttf_destroy(self->font);
  }
  return 0;
}


static int l_font_render(lua_State *L) {
  int w, h;
  Font *self = luaL_checkudata(L, 1, CLASS_NAME);
  const char *str = lua_tostring(L, 2);
  if (!str || *str == '\0') str = " ";
  Buffer *b = buffer_new(L);
  void *data = ttf_render(self->font, str, &w, &h);
  if (!data) {
    luaL_error(L, "could not render text");
  }
  /* Load bitmap and free intermediate 8bit bitmap */
  b->buffer = sr_newBuffer(w, h);
  if (!b->buffer) {
    free(data);
    luaL_error(L, "could not create buffer");
  }
  sr_loadPixels8(b->buffer, data, NULL);
  free(data);
  return 1;
}


static int l_font_getWidth(lua_State *L) {
  Font *self = luaL_checkudata(L, 1, CLASS_NAME);
  const char *str = luaL_checkstring(L, 2);
  lua_pushnumber(L, ttf_width(self->font, str));
  return 1;
}


static int l_font_getHeight(lua_State *L) {
  Font *self = luaL_checkudata(L, 1, CLASS_NAME);
  lua_pushnumber(L, ttf_height(self->font));
  return 1;
}


int luaopen_font(lua_State *L) {
  luaL_Reg reg[] = {
    { "__gc",         l_font_gc           },
    { "fromFile",     l_font_fromFile     },
    { "fromString",   l_font_fromString   },
    { "fromEmbedded", l_font_fromEmbedded },
    { "render",       l_font_render       },
    { "getWidth",     l_font_getWidth     },
    { "getHeight",    l_font_getHeight    },
    { NULL, NULL }
  };
  ASSERT( luaL_newmetatable(L, CLASS_NAME) );
  luaL_setfuncs(L, reg, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  return 1;
}
