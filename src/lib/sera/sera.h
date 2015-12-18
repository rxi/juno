/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef SERA_H
#define SERA_H

#if SR_MODE_RGBA
  #define SR_CHANNELS r, g, b, a
  #define SR_RGB_MASK 0xffffff
#elif SR_MODE_ARGB
  #define SR_CHANNELS a, r, g, b
  #define SR_RGB_MASK 0xffffff00
#elif SR_MODE_ABGR
  #define SR_CHANNELS a, b, g, r
  #define SR_RGB_MASK 0xffffff00
#else
  #define SR_CHANNELS b, g, r, a
  #define SR_RGB_MASK 0xffffff
#endif

typedef union {
  unsigned int word;
  struct { unsigned char SR_CHANNELS; } rgba;
} sr_Pixel;

typedef struct {
  int x, y, w, h;
} sr_Rect;

typedef struct {
  sr_Pixel color;
  unsigned char alpha, blend;
} sr_DrawMode;

typedef struct {
  float ox, oy, r, sx, sy;
} sr_Transform;

typedef struct {
  sr_DrawMode mode;
  sr_Rect clip;
  sr_Pixel *pixels;
  int w, h;
  char flags;
} sr_Buffer;

#define SR_BUFFER_SHARED (1 << 0)

enum {
  SR_FMT_BGRA,
  SR_FMT_RGBA,
  SR_FMT_ARGB,
  SR_FMT_ABGR
};

enum {
  SR_BLEND_ALPHA,
  SR_BLEND_COLOR,
  SR_BLEND_ADD,
  SR_BLEND_SUBTRACT,
  SR_BLEND_MULTIPLY,
  SR_BLEND_LIGHTEN,
  SR_BLEND_DARKEN,
  SR_BLEND_SCREEN,
  SR_BLEND_DIFFERENCE
};


sr_Pixel sr_pixel(int r, int g, int b, int a);
sr_Pixel sr_color(int r, int g, int b);
sr_Transform sr_transform(void);
sr_Rect sr_rect(int x, int y, int w, int h);

sr_Buffer *sr_newBuffer(int w, int h);
sr_Buffer *sr_newBufferShared(void *pixels, int w, int h);
sr_Buffer *sr_cloneBuffer(sr_Buffer *src);
void sr_destroyBuffer(sr_Buffer* b);

void sr_loadPixels(sr_Buffer *b, void *src, int fmt);
void sr_loadPixels8(sr_Buffer *b, unsigned char *src, sr_Pixel *pal);

void sr_setAlpha(sr_Buffer* b, int alpha);
void sr_setBlend(sr_Buffer* b, int blend);
void sr_setColor(sr_Buffer* b, sr_Pixel c);
void sr_setClip(sr_Buffer *b, sr_Rect r);
void sr_reset(sr_Buffer *b);

void sr_clear(sr_Buffer *b, sr_Pixel c);
sr_Pixel sr_getPixel(sr_Buffer *b, int x, int y);
void sr_setPixel(sr_Buffer *b, sr_Pixel c, int x, int y);
void sr_copyPixels(sr_Buffer *b, sr_Buffer *src, int x, int y,
                   sr_Rect *sub, float sx, float sy);
void sr_noise(sr_Buffer *b, unsigned seed, int low, int high, int grey);
void sr_floodFill(sr_Buffer *b, sr_Pixel c, int x, int y);

void sr_drawPixel(sr_Buffer *b, sr_Pixel c, int x, int y);
void sr_drawLine(sr_Buffer *b, sr_Pixel c, int x0, int y0, int x1, int y1);
void sr_drawRect(sr_Buffer *b, sr_Pixel c, int x, int y, int w, int h);
void sr_drawBox(sr_Buffer *b, sr_Pixel c, int x, int y, int w, int h);
void sr_drawCircle(sr_Buffer *b, sr_Pixel c, int x, int y, int r);
void sr_drawRing(sr_Buffer *b, sr_Pixel c, int x, int y, int r);
void sr_drawBuffer(sr_Buffer *b, sr_Buffer *src, int x, int y,
                   sr_Rect *sub, sr_Transform *t);

#endif
