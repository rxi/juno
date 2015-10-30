/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"
#include "lib/sera/sera.h"
#include "m_buffer.h"
#include "util.h"
#include "fs.h"

#define CLASS_NAME BUFFER_CLASS_NAME

Buffer *buffer_new(lua_State *L) {
  Buffer *self = lua_newuserdata(L, sizeof(*self));
  luaL_setmetatable(L, CLASS_NAME);
  memset(self, 0, sizeof(*self));
  return self;
}


static sr_Pixel getColorArgs(lua_State *L, int first, int defzero) {
  float n = defzero ? 0. : 1.;
  int r = luaL_optnumber(L, first + 0, n) * 256;
  int g = luaL_optnumber(L, first + 1, n) * 256;
  int b = luaL_optnumber(L, first + 2, n) * 256;
  int a = luaL_optnumber(L, first + 3, n) * 256;
  return sr_pixel(r, g, b, a);
}


static sr_Rect getRectArg(lua_State *L, int idx) {
  if (lua_type(L, idx) != LUA_TTABLE) {
    luaL_argerror(L, idx, "expected table");
  }
  idx = lua_absindex(L, idx);
  lua_getfield(L, idx, "x"); int x = lua_tonumber(L, -1);
  lua_getfield(L, idx, "y"); int y = lua_tonumber(L, -1);
  lua_getfield(L, idx, "w"); int w = lua_tonumber(L, -1);
  lua_getfield(L, idx, "h"); int h = lua_tonumber(L, -1);
  lua_pop(L, 4);
  return sr_rect(x, y, w, h);
}


static void checkSubRect(lua_State *L, int idx, sr_Buffer *b, sr_Rect *r) {
  if (r->x < 0 || r->y < 0 || r->x + r->w > b->w || r->y + r->h > b->h) {
    luaL_argerror(L, idx, "sub rectangle out of bounds");
  }
}


static int loadBufferFromMemory(Buffer *self, const void *data, int len) {
  int w, h;
  void *pixels = stbi_load_from_memory(
    data, len, &w, &h, NULL, STBI_rgb_alpha);
  if (!pixels) {
    return -1;
  }
  self->buffer = sr_newBuffer(w, h);
  if (!self->buffer) {
    free(pixels);
    return -1;
  }
  sr_loadPixels(self->buffer, pixels, SR_FMT_RGBA);
  free(pixels);
  return 0;
}


static int l_buffer_fromFile(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  Buffer *self = buffer_new(L);
  size_t len;
  void *data = fs_read(filename, &len);
  if (!data) {
    luaL_error(L, "could not open file '%s'", filename);
  }
  int err = loadBufferFromMemory(self, data, len);
  free(data);
  if (err) {
    luaL_error(L, "could not load buffer");
  }
  return 1;
}


static int l_buffer_fromString(lua_State *L) {
  size_t len;
  const char *str = luaL_checklstring(L, 1, &len);
  Buffer *self = buffer_new(L);
  int err = loadBufferFromMemory(self, str, len);
  if (err) {
    luaL_error(L, "could not load buffer");
  }
  return 1;
}


static int l_buffer_fromBlank(lua_State *L) {
  int w = luaL_checknumber(L, 1);
  int h = luaL_checknumber(L, 2);
  if (w <= 0) luaL_argerror(L, 1, "expected width greater than 0");
  if (h <= 0) luaL_argerror(L, 2, "expected height greater than 0");
  Buffer *self = buffer_new(L);
  self->buffer = sr_newBuffer(w, h);
  sr_clear(self->buffer, sr_pixel(0, 0, 0, 0));
  if (!self->buffer) {
    luaL_error(L, "could not create buffer");
  }
  return 1;
}


static int l_buffer_clone(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  Buffer *b = buffer_new(L);
  b->buffer = sr_cloneBuffer(self->buffer);
  if (!b->buffer) {
    luaL_error(L, "could not clone buffer");
  }
  return 1;
}


static int l_buffer_gc(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  if (self->buffer) {               /* self->buffer may be NULL if  */
    sr_destroyBuffer(self->buffer); /* an error was raised in the   */
  }                                 /* constructor                  */
  return 0;
}


static int l_buffer_getWidth(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  lua_pushnumber(L, self->buffer->w);
  return 1;
}

static int l_buffer_getHeight(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  lua_pushnumber(L, self->buffer->h);
  return 1;
}


static int l_buffer_setAlpha(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  sr_setAlpha(self->buffer, luaL_optnumber(L, 2, 1.) * 0xff);
  return 0;
}


static int l_buffer_setBlend(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  const char *str = luaL_optstring(L, 2, "alpha");
  int mode = 0;
  if      (!strcmp(str, "alpha"     )) mode = SR_BLEND_ALPHA;
  else if (!strcmp(str, "color"     )) mode = SR_BLEND_COLOR;
  else if (!strcmp(str, "add"       )) mode = SR_BLEND_ADD;
  else if (!strcmp(str, "subtract"  )) mode = SR_BLEND_SUBTRACT;
  else if (!strcmp(str, "multiply"  )) mode = SR_BLEND_MULTIPLY;
  else if (!strcmp(str, "lighten"   )) mode = SR_BLEND_LIGHTEN;
  else if (!strcmp(str, "darken"    )) mode = SR_BLEND_DARKEN;
  else if (!strcmp(str, "screen"    )) mode = SR_BLEND_SCREEN;
  else if (!strcmp(str, "difference")) mode = SR_BLEND_DIFFERENCE;
  else luaL_argerror(L, 2, "bad blend mode");
  sr_setBlend(self->buffer, mode);
  return 0;
}


static int l_buffer_setColor(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  sr_setColor(self->buffer, getColorArgs(L, 2, 0));
  return 0;
}


static int l_buffer_setClip(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checkinteger(L, 2);
  int y = luaL_checkinteger(L, 3);
  int w = luaL_checkinteger(L, 4);
  int h = luaL_checkinteger(L, 5);
  sr_setClip(self->buffer, sr_rect(x, y, w, h));
  return 0;
}


static int l_buffer_reset(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  sr_reset(self->buffer);
  return 0;
}


static int l_buffer_clear(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  sr_clear(self->buffer, getColorArgs(L, 2, 1));
  return 0;
}


static int l_buffer_getPixel(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  sr_Pixel px = sr_getPixel(self->buffer, x, y);
  lua_pushnumber(L, px.rgba.r * 0.00390625); /* div 256. */
  lua_pushnumber(L, px.rgba.g * 0.00390625);
  lua_pushnumber(L, px.rgba.b * 0.00390625);
  lua_pushnumber(L, px.rgba.a * 0.00390625);
  return 4;
}


static int l_buffer_setPixel(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  sr_setPixel(self->buffer, getColorArgs(L, 4, 0), x, y);
  return 0;
}


static int l_buffer_copyPixels(lua_State *L) {
  sr_Rect sub;
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  Buffer *src  = luaL_checkudata(L, 2, CLASS_NAME);
  int x = luaL_optnumber(L, 3, 0);
  int y = luaL_optnumber(L, 4, 0);
  int hasSub = 0;
  if (!lua_isnoneornil(L, 5)) {
    hasSub = 1;
    sub = getRectArg(L, 5);
    checkSubRect(L, 5, src->buffer, &sub);
  }
  float sx = luaL_optnumber(L, 6, 1.);
  float sy = luaL_optnumber(L, 7, sx);
  sr_copyPixels(self->buffer, src->buffer, x, y,
                 hasSub ? &sub : NULL, sx, sy);
  return 0;
}


static int l_buffer_noise(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int seed = luaL_optnumber(L, 2, rand());
  int low  = luaL_optnumber(L, 3, 0) * 256;
  int high = luaL_optnumber(L, 4, 1) * 256;
  int grey = luax_optboolean(L, 5, 0);
  sr_noise(self->buffer, seed, low, high, grey);
  return 0;
}


static int l_buffer_floodFill(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  sr_Pixel px = getColorArgs(L, 4, 0);
  sr_floodFill(self->buffer, px, x, y);
  return 0;
}


static int l_buffer_drawPixel(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  sr_Pixel px = getColorArgs(L, 4, 0);
  sr_drawPixel(self->buffer, px, x, y);
  return 0;
}


static int l_buffer_drawLine(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x1 = luaL_checknumber(L, 2);
  int y1 = luaL_checknumber(L, 3);
  int x2 = luaL_checknumber(L, 4);
  int y2 = luaL_checknumber(L, 5);
  sr_Pixel px = getColorArgs(L, 6, 0);
  sr_drawLine(self->buffer, px, x1, y1, x2, y2);
  return 0;
}

static int l_buffer_drawRect(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  int w = luaL_checknumber(L, 4);
  int h = luaL_checknumber(L, 5);
  sr_Pixel px = getColorArgs(L, 6, 0);
  sr_drawRect(self->buffer, px, x, y, w, h);
  return 0;
}


static int l_buffer_drawBox(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  int w = luaL_checknumber(L, 4);
  int h = luaL_checknumber(L, 5);
  sr_Pixel px = getColorArgs(L, 6, 0);
  sr_drawBox(self->buffer, px, x, y, w, h);
  return 0;
}


static int l_buffer_drawTriangle(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x1 = luaL_checknumber(L, 2);
  int y1 = luaL_checknumber(L, 3);
  int x2 = luaL_checknumber(L, 4);
  int y2 = luaL_checknumber(L, 5);
  int x3 = luaL_checknumber(L, 6);
  int y3 = luaL_checknumber(L, 7);
  sr_Pixel px = getColorArgs(L, 8, 0);
  sr_drawTriangle(self->buffer, px, x1, y1, x2, y2, x3, y3);
  return 0;
}


static int l_buffer_drawCircle(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  int x = luaL_checknumber(L, 2);
  int y = luaL_checknumber(L, 3);
  int r = luaL_checknumber(L, 4);
  sr_Pixel px = getColorArgs(L, 5, 0);
  sr_drawCircle(self->buffer, px, x, y, r);
  return 0;
}


static int l_buffer_drawBuffer(lua_State *L) {
  int hasSub = 0;
  sr_Rect sub;
  sr_Transform t;
  Buffer *self = luaL_checkudata(L, 1, CLASS_NAME);
  Buffer *src  = luaL_checkudata(L, 2, CLASS_NAME);
  int x = luaL_optnumber(L, 3, 0);
  int y = luaL_optnumber(L, 4, 0);
  if (!lua_isnoneornil(L, 5)) {
    hasSub = 1;
    sub = getRectArg(L, 5);
    checkSubRect(L, 5, src->buffer, &sub);
  }
  t.r  = luaL_optnumber(L, 6, 0);
  t.sx = luaL_optnumber(L, 7, 1);
  t.sy = luaL_optnumber(L, 8, t.sx);
  t.ox = luaL_optnumber(L, 9, 0);
  t.oy = luaL_optnumber(L, 10, 0);
  sr_drawBuffer(self->buffer, src->buffer, x, y, hasSub ? &sub : NULL, &t);
  return 0;
}


int luaopen_buffer(lua_State *L) {
  luaL_Reg reg[] = {
    { "__gc",           l_buffer_gc             },
    { "fromFile",       l_buffer_fromFile       },
    { "fromString",     l_buffer_fromString     },
    { "fromBlank",      l_buffer_fromBlank      },
    { "clone",          l_buffer_clone          },
    { "getWidth",       l_buffer_getWidth       },
    { "getHeight",      l_buffer_getHeight      },
    { "setAlpha",       l_buffer_setAlpha       },
    { "setBlend",       l_buffer_setBlend       },
    { "setColor",       l_buffer_setColor       },
    { "setClip",        l_buffer_setClip        },
    { "reset",          l_buffer_reset          },
    { "clear",          l_buffer_clear          },
    { "getPixel",       l_buffer_getPixel       },
    { "setPixel",       l_buffer_setPixel       },
    { "copyPixels",     l_buffer_copyPixels     },
    { "noise",          l_buffer_noise          },
    { "floodFill",      l_buffer_floodFill      },
    { "drawPixel",      l_buffer_drawPixel      },
    { "drawLine",       l_buffer_drawLine       },
    { "drawRect",       l_buffer_drawRect       },
    { "drawBox",        l_buffer_drawBox        },
    { "drawTriangle",   l_buffer_drawTriangle   },
    { "drawCircle",     l_buffer_drawCircle     },
    { "drawBuffer",     l_buffer_drawBuffer     },
    { "draw",           l_buffer_drawBuffer     },
    { NULL, NULL }
  };
  ASSERT( luaL_newmetatable(L, CLASS_NAME) );
  luaL_setfuncs(L, reg, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  return 1;
}

