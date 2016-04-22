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
#include "lib/sera/sera.h"
#include "m_buffer.h"

#define MIN(a, b)           ((b) < (a) ? (b) : (a))
#define MAX(a, b)           ((b) > (a) ? (b) : (a))
#define CLAMP(x, a, b)      (MAX(a, MIN(x, b)))
#define LERP(bits, a, b, p) ((a) + ((((b) - (a)) * (p)) >> (bits)))

#define FX_BITS 10
#define FX_UNIT (1 << FX_BITS)
#define FX_MASK (FX_UNIT - 1)

static int tablesInited = 0;
static int tableSin[FX_UNIT];

static void initTables(void) {
  if (tablesInited) return;
  /* Make sin table */
  int i;
  for (i = 0; i < FX_UNIT; i++) {
    tableSin[i] = sin((i / (float) FX_UNIT) * 6.283185) * FX_UNIT;
  }
  /* Done */
  tablesInited = 1;
}

static int fxsin(int n) {
  return tableSin[n & FX_MASK];
}

static sr_Pixel getColorFromTable(lua_State *L, int idx) {
  if (lua_isnoneornil(L, idx) || lua_type(L, idx) != LUA_TTABLE) {
    luaL_error(L, "expected table");
  }
  sr_Pixel px;
  int i;
  float v;
  for (i = 0; i < 4; i++) {
    lua_rawgeti(L, idx, i + 1);
    v = lua_tonumber(L, -1);
    v = CLAMP(v, 0, 1) * 255;
    switch (i) {
      case 0 : px.rgba.r = v; break;
      case 1 : px.rgba.g = v; break;
      case 2 : px.rgba.b = v; break;
      case 3 : px.rgba.a = v; break;
    }
    lua_pop(L, 1);
  }
  return px;
}

static void checkBufferSizesMatch(lua_State *L, Buffer *a, Buffer *b) {
  if (a->buffer->w != b->buffer->w || a->buffer->h != b->buffer->h) {
    luaL_error(L, "expected buffer sizes to match");
  }
}


static int l_bufferfx_desaturate(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  int amount = luaL_optnumber(L, 2, 1.) * 0xff;
  amount = CLAMP(amount, 0, 0xff);
  int i = self->buffer->w * self->buffer->h;
  sr_Pixel *p = self->buffer->pixels;
  if (amount >= 0xfe) {
    /* Full amount? Don't bother with lerping, just write pixel avg */
    while (i--) {
      p->rgba.r = p->rgba.g = p->rgba.b =
        ((p->rgba.r + p->rgba.g + p->rgba.b) * 341) >> 10;
      p++;
    }
  } else {
    while (i--) {
      int avg = ((p->rgba.r + p->rgba.g + p->rgba.b) * 341) >> 10;
      p->rgba.r = LERP(8, p->rgba.r, avg, amount);
      p->rgba.g = LERP(8, p->rgba.g, avg, amount);
      p->rgba.b = LERP(8, p->rgba.b, avg, amount);
      p++;
    }

  }
  return 0;
}


static int l_bufferfx_mask(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  Buffer *mask = luaL_checkudata(L, 2, BUFFER_CLASS_NAME);
  const char *channel = luaL_optstring(L, 3, "a");
  checkBufferSizesMatch(L, self, mask);
  if (!strchr("rgba", *channel)) {
    luaL_error(L, "expected channel to be 'r', 'g', 'b' or 'a'");
  }
  int i = self->buffer->w * self->buffer->h;
  sr_Pixel *d = self->buffer->pixels;
  sr_Pixel *s = mask->buffer->pixels;
  while (i--) {
    switch (*channel) {
      case 'r' : d->rgba.a = (d->rgba.a * s->rgba.r) >> 8; break;
      case 'g' : d->rgba.a = (d->rgba.a * s->rgba.g) >> 8; break;
      case 'b' : d->rgba.a = (d->rgba.a * s->rgba.b) >> 8; break;
      case 'a' : d->rgba.a = (d->rgba.a * s->rgba.a) >> 8; break;
    }
    d++;
    s++;
  }
  return 0;
}


static int l_bufferfx_palette(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  if (lua_isnoneornil(L, 2) || lua_type(L, 2) != LUA_TTABLE) {
    luaL_argerror(L, 2, "expected table");
  }
  /* Load palette from table */
  sr_Pixel pal[256];
  int ncolors = lua_rawlen(L, 2);
  if (ncolors == 0) {
    luaL_argerror(L, 2, "expected non-empty table");
  }
  int i;
  for (i = 0; i < 256; i++) {
    lua_rawgeti(L, 2, ((i * ncolors) >> 8) + 1);
    pal[i] = getColorFromTable(L, -1);
    lua_pop(L, 1);
  }
  /* Convert each pixel to palette color based on its brightest channel */
  i = self->buffer->w * self->buffer->h;
  sr_Pixel *p = self->buffer->pixels;
  while (i--) {
    int idx = MAX(MAX(p->rgba.r, p->rgba.b), p->rgba.g);
    p->rgba.r = pal[idx].rgba.r;
    p->rgba.g = pal[idx].rgba.g;
    p->rgba.b = pal[idx].rgba.b;
    p++;
  }
  return 0;
}


static unsigned long long xorshift64star(unsigned long long *x) {
  *x ^= *x >> 12;
  *x ^= *x << 25;
  *x ^= *x >> 27;
  return *x * 2685821657736338717ULL;
}

static int l_bufferfx_dissolve(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  unsigned long long s = 1ULL << 32;
  unsigned amount;
  amount = luaL_checknumber(L, 2) * 256;
  s |= (unsigned) (luaL_optnumber(L, 3, 0));
  amount = CLAMP(amount, 0, 0xff);
  int i = self->buffer->w * self->buffer->h;
  sr_Pixel *p = self->buffer->pixels;
  while (i--) {
    if ((xorshift64star(&s) & 0xff) < amount) {
      p->rgba.a = 0;
    }
    p++;
  }
  return 0;
}


static int l_bufferfx_wave(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  Buffer *src = luaL_checkudata(L, 2, BUFFER_CLASS_NAME);
  checkBufferSizesMatch(L, self, src);
  int amountX = luaL_checknumber(L, 3);
  int amountY = luaL_checknumber(L, 4);
  int scaleX  = luaL_checknumber(L, 5)  * FX_UNIT;
  int scaleY  = luaL_checknumber(L, 6)  * FX_UNIT;
  int offsetX = luaL_optnumber(L, 7, 0) * FX_UNIT;
  int offsetY = luaL_optnumber(L, 8, 0) * FX_UNIT;
  int x, y;
  for (y = 0; y < self->buffer->h; y++) {
    sr_Pixel *d = self->buffer->pixels + y * self->buffer->w;
    int ox = (fxsin(offsetX + ((y * scaleX) >> FX_BITS)) * amountX)
             >> FX_BITS;
    for (x = 0; x < self->buffer->w; x++) {
      int oy = (fxsin(offsetY + ((x * scaleY) >> FX_BITS)) * amountY)
               >> FX_BITS;
      *d = sr_getPixel(src->buffer, x + ox, y + oy);
      d++;
    }
  }
  return 0;
}


static int getChannel(sr_Pixel px, char channel) {
  switch (channel) {
    case 'r' : return px.rgba.r;
    case 'g' : return px.rgba.g;
    case 'b' : return px.rgba.b;
    case 'a' : return px.rgba.a;
  }
  return 0;
}

static int l_bufferfx_displace(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  Buffer *src = luaL_checkudata(L, 2, BUFFER_CLASS_NAME);
  Buffer *map = luaL_checkudata(L, 3, BUFFER_CLASS_NAME);
  const char *channelX = luaL_checkstring(L, 4);
  const char *channelY = luaL_checkstring(L, 5);
  int scaleX = luaL_checknumber(L, 6) * (1 << 7);
  int scaleY = luaL_checknumber(L, 7) * (1 << 7);
  int x, y;
  checkBufferSizesMatch(L, self, src);
  checkBufferSizesMatch(L, self, map);
  if (!strchr("rgba", *channelX)) luaL_argerror(L, 4, "bad channel");
  if (!strchr("rgba", *channelY)) luaL_argerror(L, 5, "bad channel");
  for (y = 0; y < self->buffer->h; y++) {
    sr_Pixel *d = self->buffer->pixels + y * self->buffer->w;
    sr_Pixel *m = map->buffer->pixels + y * self->buffer->w;
    for (x = 0; x < self->buffer->w; x++) {
      int cx = ((getChannel(*m, *channelX) - (1 << 7)) * scaleX) >> 14;
      int cy = ((getChannel(*m, *channelY) - (1 << 7)) * scaleY) >> 14;
      *d = sr_getPixel(src->buffer, x + cx, y + cy);
      d++;
      m++;
    }
  }
  return 0;
}


static int l_bufferfx_blur(lua_State *L) {
  Buffer *self = luaL_checkudata(L, 1, BUFFER_CLASS_NAME);
  Buffer *src = luaL_checkudata(L, 2, BUFFER_CLASS_NAME);
  int radiusx = luaL_checknumber(L, 3);
  int radiusy = luaL_checknumber(L, 4);
  int y, x, ky, kx;
  int r, g, b, r2, g2, b2;
  sr_Pixel p2;
  int w = src->buffer->w;
  int h = src->buffer->h;
  int dx = 256 / (radiusx * 2 + 1);
  int dy = 256 / (radiusy * 2 + 1);
  sr_Rect bounds = sr_rect(radiusx, radiusy, w - radiusx, h - radiusy);
  sr_Pixel *p = self->buffer->pixels;
  checkBufferSizesMatch(L, self, src);
  /* do blur */
  for (y = 0; y < h; y++) {
    int inBoundsY = y >= bounds.y && y < bounds.h;
    for (x = 0; x < w; x++) {
      /* are the pixels that will be used in bounds? */
      int inBounds = inBoundsY && x >= bounds.x && x < bounds.w;
      /* blur pixel */
      #define GET_PIXEL_FAST(b, x, y) ((b)->pixels[(x) + (y) * w])
      #define BLUR_PIXEL(GET_PIXEL)                      \
        r = 0, g = 0, b = 0;                             \
        for (ky = -radiusy; ky <= radiusy; ky++) {       \
          r2 = 0, g2 = 0, b2 = 0;                        \
          for (kx = -radiusx; kx <= radiusx; kx++) {     \
            p2 = GET_PIXEL(src->buffer, x + kx, y + ky); \
            r2 += p2.rgba.r;                             \
            g2 += p2.rgba.g;                             \
            b2 += p2.rgba.b;                             \
          }                                              \
          r += (r2 * dx) >> 8;                           \
          g += (g2 * dx) >> 8;                           \
          b += (b2 * dx) >> 8;                           \
        }
      if (inBounds) {
        BLUR_PIXEL(GET_PIXEL_FAST)
      } else {
        BLUR_PIXEL(sr_getPixel)
      }
      /* set pixel */
      p->rgba.r = (r * dy) >> 8;
      p->rgba.g = (g * dy) >> 8;
      p->rgba.b = (b * dy) >> 8;
      p->rgba.a = 0xff;
      p++;
    }
  }
  return 0;
}


int luaopen_bufferfx(lua_State *L) {
  luaL_Reg reg[] = {
    { "desaturate", l_bufferfx_desaturate },
    { "palette",    l_bufferfx_palette    },
    { "dissolve",   l_bufferfx_dissolve   },
    { "mask",       l_bufferfx_mask       },
    { "wave",       l_bufferfx_wave       },
    { "displace",   l_bufferfx_displace   },
    { "blur",       l_bufferfx_blur       },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);
  initTables();
  return 1;
}
