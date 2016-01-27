/**
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "sera.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIN(a, b)           ((b) < (a) ? (b) : (a))
#define MAX(a, b)           ((b) > (a) ? (b) : (a))
#define CLAMP(x, a, b)      (MAX(a, MIN(x, b)))
#define LERP(bits, a, b, p) ((a) + ((((b) - (a)) * (p)) >> (bits)))

#define SWAP(T, a, b)\
  do {\
    T tmp__ = a;\
    a = b;\
    b = tmp__;\
  } while (0)

#define PI  (3.14159265359)
#define PI2 (6.28318530718)

#define FX_BITS (12)
#define FX_UNIT (1 << FX_BITS)
#define FX_MASK (FX_UNIT - 1)


typedef struct { int x, y; } sr_Point;
typedef struct { unsigned x, y, z, w; } sr_RandState;


static int inited = 0;
static unsigned char div8Table[256][256];

static void init(void) {
  int a, b;
  if (inited) return;
  /* Init 8bit divide lookup table */
  for (b = 1; b < 256; b++) {
    for (a = 0; a < 256; a++) {
      div8Table[a][b] = (a << 8) / b;
    }
  }
  /* Inited */
  inited = 1;
}


static void check(int cond, const char *fname, const char *msg) {
  if (!cond) {
    fprintf(stderr, "(error) %s() %s\n", fname, msg);
    exit(EXIT_FAILURE);
  }
}


static int xdiv(int n, int x) {
  if (x == 0) return n;
  return n / x;
}


static sr_RandState rand128init(unsigned seed) {
  sr_RandState s;
  s.x = (seed & 0xff000000) | 1;
  s.y = seed & 0xff0000;
  s.z = seed & 0xff00;
  s.w = seed & 0xff;
  return s;
}


static unsigned rand128(sr_RandState *s) {
  unsigned t = s->x ^ (s->x << 11);
  s->x = s->y;
  s->y = s->z;
  s->z = s->w;
  return s->w = s->w ^ (s->w >> 19) ^ t ^ (t >> 8);
}


sr_Pixel sr_pixel(int r, int g, int b, int a) {
  sr_Pixel p;
  p.rgba.r = CLAMP(r, 0, 0xff);
  p.rgba.g = CLAMP(g, 0, 0xff);
  p.rgba.b = CLAMP(b, 0, 0xff);
  p.rgba.a = CLAMP(a, 0, 0xff);
  return p;
}


sr_Pixel sr_color(int r, int g, int b) {
  return sr_pixel(r, g, b, 0xff);
}


sr_Transform sr_transform(void) {
  sr_Transform t;
  t.ox = t.oy = 0;
  t.sx = t.sy = 1;
  t.r = 0;
  return t;
}


sr_Rect sr_rect(int x, int y, int w, int h) {
  sr_Rect r;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  return r;
}


static void clipRect(sr_Rect *r, sr_Rect *to) {
  int x1 = MAX(r->x, to->x);
  int y1 = MAX(r->y, to->y);
  int x2 = MIN(r->x + r->w, to->x + to->w);
  int y2 = MIN(r->y + r->h, to->y + to->h);
  r->x = x1;
  r->y = y1;
  r->w = MAX(x2 - x1, 0);
  r->h = MAX(y2 - y1, 0);
}


static void clipRectAndOffset(sr_Rect *r, int *x, int *y, sr_Rect *to) {
  int d;
  if ((d = (to->x - *x)) > 0) { *x += d; r->w -= d; r->x += d; }
  if ((d = (to->y - *y)) > 0) { *y += d; r->h -= d; r->y += d; }
  if ((d = (*x + r->w) - (to->x + to->w)) > 0) { r->w -= d; }
  if ((d = (*y + r->h) - (to->y + to->h)) > 0) { r->h -= d; }
}


static void initBuffer(sr_Buffer *b, void *pixels, int w, int h) {
  /* Init lookup tables if not inited */
  init();
  /* Init buffer */
  b->pixels = pixels;
  b->w = w;
  b->h = h;
  sr_reset(b);
}


sr_Buffer *sr_newBuffer(int w, int h) {
  sr_Buffer *b = calloc(1, sizeof(*b));
  if (!b) return NULL;
  check(w > 0, "sr_newBuffer", "expected width of 1 or greater");
  check(h > 0, "sr_newBuffer", "expected height of 1 or greater");
  b->pixels = malloc(w * h * sizeof(*b->pixels));
  if (!b->pixels) {
    free(b);
    return NULL;
  }
  initBuffer(b, b->pixels, w, h);
  return b;
}


sr_Buffer *sr_newBufferShared(void *pixels, int w, int h) {
  sr_Buffer *b = calloc(1, sizeof(*b));
  if (!b) return NULL;
  initBuffer(b, pixels, w, h);
  b->flags |= SR_BUFFER_SHARED;
  return b;
}


sr_Buffer *sr_cloneBuffer(sr_Buffer *src) {
  sr_Pixel *pixels;
  sr_Buffer *b = sr_newBuffer(src->w, src->h);
  if (!b) return NULL;
  pixels = b->pixels;
  memcpy(pixels, src->pixels, b->w * b->h * sizeof(*b->pixels));
  memcpy(b, src, sizeof(*b));
  b->pixels = pixels;
  return b;
}


void sr_destroyBuffer(sr_Buffer *b) {
  if (~b->flags & SR_BUFFER_SHARED) {
    free(b->pixels);
  }
  free(b);
}


void sr_loadPixels(sr_Buffer *b, void *src, int fmt) {
  int sr, sg, sb, sa;
  int i = b->w * b->h;
  unsigned *s = src;
  switch (fmt) {
    case SR_FMT_BGRA : sr = 16, sg =  8, sb =  0, sa = 24; break;
    case SR_FMT_RGBA : sr =  0, sg =  8, sb = 16, sa = 24; break;
    case SR_FMT_ARGB : sr =  8, sg = 16, sb = 24, sa =  0; break;
    case SR_FMT_ABGR : sr = 24, sg = 16, sb =  8, sa =  0; break;
    default: check(0, "sr_loadPixels", "bad fmt");
  }
  while (i--) {
    b->pixels[i].rgba.r = (s[i] >> sr) & 0xff;
    b->pixels[i].rgba.g = (s[i] >> sg) & 0xff;
    b->pixels[i].rgba.b = (s[i] >> sb) & 0xff;
    b->pixels[i].rgba.a = (s[i] >> sa) & 0xff;
  }
}


void sr_loadPixels8(sr_Buffer *b, unsigned char *src, sr_Pixel *pal) {
  int i = b->w * b->h;
  while (i--) {
    if (pal) {
      b->pixels[i] = pal[src[i]];
    } else {
      b->pixels[i] = sr_pixel(0xff, 0xff, 0xff, src[i]);
    }
  }
}


void sr_setBlend(sr_Buffer *b, int blend) {
  b->mode.blend = blend;
}


void sr_setAlpha(sr_Buffer *b, int alpha) {
  b->mode.alpha = CLAMP(alpha, 0, 0xff);
}


void sr_setColor(sr_Buffer *b, sr_Pixel c) {
  b->mode.color.word = c.word & SR_RGB_MASK;
}


void sr_setClip(sr_Buffer *b, sr_Rect r) {
  b->clip = r;
  r = sr_rect(0, 0, b->w, b->h);
  clipRect(&b->clip, &r);
}


void sr_reset(sr_Buffer *b) {
  sr_setBlend(b, SR_BLEND_ALPHA);
  sr_setAlpha(b, 0xff);
  sr_setColor(b, sr_color(0xff, 0xff, 0xff));
  sr_setClip(b, sr_rect(0, 0, b->w, b->h));
}


void sr_clear(sr_Buffer *b, sr_Pixel c) {
  int i = b->w * b->h;
  while (i--) {
    b->pixels[i] = c;
  }
}


sr_Pixel sr_getPixel(sr_Buffer *b, int x, int y) {
  sr_Pixel p;
  if (x >= 0 && y >= 0 && x < b->w && y < b->h) {
    return b->pixels[x + y * b->w];
  }
  p.word = 0;
  return p;
}


void sr_setPixel(sr_Buffer *b, sr_Pixel c, int x, int y) {
  if (x >= 0 && y >= 0 && x < b->w && y < b->h) {
    b->pixels[x + y * b->w] = c;
  }
}


static void copyPixelsBasic(
  sr_Buffer *b, sr_Buffer *src, int x, int y, sr_Rect s
) {
  int i;
  /* Clip to destination buffer */
  clipRectAndOffset(&s, &x, &y, &b->clip);
  /* Clipped off screen? */
  if (s.w <= 0 || s.h <= 0) return;
  /* Copy pixels */
  for (i = 0; i < s.h; i++) {
    memcpy(b->pixels + x + (y + i) * b->w,
           src->pixels + s.x + (s.y + i) * src->w,
           s.w * sizeof(*b->pixels));
  }
}


static void copyPixelsScaled(
  sr_Buffer *b, sr_Buffer *src, int x, int y, sr_Rect s,
  float scalex, float scaley
) {
  int d, dx, dy,  edx, sx, sy, inx, iny;
  sr_Pixel *p;
  int w = s.w * scalex;
  int h = s.h * scaley;
  inx = FX_UNIT / scalex;
  iny = FX_UNIT / scaley;
  /* Clip to destination buffer */
  if ((d = (b->clip.x - x)) > 0) { x += d; s.x += d / scalex; w -= d; }
  if ((d = (b->clip.y - y)) > 0) { y += d; s.y += d / scaley; h -= d; }
  if ((d = ((x + w) - (b->clip.x + b->clip.w))) > 0) { w -= d; }
  if ((d = ((y + h) - (b->clip.y + b->clip.h))) > 0) { h -= d; }
  /* Clipped offscreen? */
  if (w == 0 || h == 0) return;
  /* Draw */
  sy = s.y << FX_BITS;
  for (dy = y; dy < y + h; dy++) {
    p = src->pixels + (s.x >> FX_BITS) + src->w * (sy >> FX_BITS);
    sx = 0;
    dx = x + b->w * dy;
    edx = dx + w;
    while (dx < edx) {
      b->pixels[dx++] = p[sx >> FX_BITS];
      sx += inx;
    }
    sy += iny;
  }
}


void sr_copyPixels(
  sr_Buffer *b, sr_Buffer *src, int x, int y, sr_Rect *sub,
  float sx, float sy
) {
  sr_Rect s;
  sx = fabs(sx);
  sy = fabs(sy);
  if (sx == 0 || sy == 0) return;
  /* Check sub rectangle */
  if (sub) {
    if (sub->w <= 0 || sub->h <= 0) return;
    s = *sub;
    check(s.x >= 0 && s.y >= 0 && s.x + s.w <= src->w && s.y + s.h <= src->h,
          "sr_copyPixels", "sub rectangle out of bounds");
  } else {
    s = sr_rect(0, 0, src->w, src->h);
  }
  /* Dispatch */
  if (sx == 1 && sy == 1) {
    /* Basic un-scaled copy */
    copyPixelsBasic(b, src, x, y, s);
  } else {
    /* Scaled copy */
    copyPixelsScaled(b, src, x, y, s, sx, sy);
  }
}


void sr_noise(sr_Buffer *b, unsigned seed, int low, int high, int grey) {
  sr_RandState s = rand128init(seed);
  int i;
  low = CLAMP(low, 0, 0xfe);
  high = CLAMP(high, low + 1, 0xff);
  i = b->w * b->h;
  if (grey) {
    while (i--) {
      b->pixels[i].rgba.r = low + rand128(&s) % (high - low);
      b->pixels[i].rgba.g = b->pixels[i].rgba.b = b->pixels[i].rgba.r;
      b->pixels[i].rgba.a = 0xff;
    }
  } else {
    while (i--) {
      b->pixels[i].word = rand128(&s) | ~SR_RGB_MASK;
      b->pixels[i].rgba.r = low + b->pixels[i].rgba.r % (high - low);
      b->pixels[i].rgba.g = low + b->pixels[i].rgba.g % (high - low);
      b->pixels[i].rgba.b = low + b->pixels[i].rgba.b % (high - low);
    }
  }
}


static void floodFill(sr_Buffer *b, sr_Pixel c, sr_Pixel o, int x, int y) {
  int ir, il;
  if (
    y < 0 || y >= b->h || x < 0 || x >= b->w ||
    b->pixels[x + y * b->w].word != o.word
  ) {
    return;
  }
  /* Fill left */
  il = x;
  while (il >= 0 && b->pixels[il + y * b->w].word == o.word) {
    b->pixels[il + y * b->w] = c;
    il--;
  }
  /* Fill right */
  ir = (x < b->w - 1) ? (x + 1) : x;
  while (ir < b->w && b->pixels[ir + y * b->w].word == o.word) {
    b->pixels[ir + y * b->w] = c;
    ir++;
  }
  /* Fill up and down */
  while (il <= ir) {
    floodFill(b, c, o, il, y - 1);
    floodFill(b, c, o, il, y + 1);
    il++;
  }
}


void sr_floodFill(sr_Buffer *b, sr_Pixel c, int x, int y) {
  floodFill(b, c, sr_getPixel(b, x, y), x, y);
}


static void blendPixel(sr_DrawMode *m, sr_Pixel *d, sr_Pixel s) {
  int alpha = (s.rgba.a * m->alpha) >> 8;
  if (alpha <= 1) return;
  /* Color */
  if (m->color.word != SR_RGB_MASK) {
    s.rgba.r = (s.rgba.r * m->color.rgba.r) >> 8;
    s.rgba.g = (s.rgba.g * m->color.rgba.g) >> 8;
    s.rgba.b = (s.rgba.b * m->color.rgba.b) >> 8;
  }
  /* Blend */
  switch (m->blend) {
    default:
    case SR_BLEND_ALPHA:
      break;
    case SR_BLEND_COLOR:
      s = m->color;
      break;
    case SR_BLEND_ADD:
      s.rgba.r = MIN(d->rgba.r + s.rgba.r, 0xff);
      s.rgba.g = MIN(d->rgba.g + s.rgba.g, 0xff);
      s.rgba.b = MIN(d->rgba.b + s.rgba.b, 0xff);
      break;
    case SR_BLEND_SUBTRACT:
      s.rgba.r = MIN(d->rgba.r - s.rgba.r, 0);
      s.rgba.g = MIN(d->rgba.g - s.rgba.g, 0);
      s.rgba.b = MIN(d->rgba.b - s.rgba.b, 0);
      break;
    case SR_BLEND_MULTIPLY:
      s.rgba.r = (s.rgba.r * d->rgba.r) >> 8;
      s.rgba.g = (s.rgba.g * d->rgba.g) >> 8;
      s.rgba.b = (s.rgba.b * d->rgba.b) >> 8;
      break;
    case SR_BLEND_LIGHTEN:
      s = (s.rgba.r + s.rgba.g + s.rgba.b >
           d->rgba.r + d->rgba.g + d->rgba.b) ? s : *d;
      break;
    case SR_BLEND_DARKEN:
      s = (s.rgba.r + s.rgba.g + s.rgba.b <
           d->rgba.r + d->rgba.g + d->rgba.b) ? s : *d;
      break;
    case SR_BLEND_SCREEN:
      s.rgba.r = 0xff - (((0xff - d->rgba.r) * (0xff - s.rgba.r)) >> 8);
      s.rgba.g = 0xff - (((0xff - d->rgba.g) * (0xff - s.rgba.g)) >> 8);
      s.rgba.b = 0xff - (((0xff - d->rgba.b) * (0xff - s.rgba.b)) >> 8);
      break;
    case SR_BLEND_DIFFERENCE:
      s.rgba.r = abs(s.rgba.r - d->rgba.r);
      s.rgba.g = abs(s.rgba.g - d->rgba.g);
      s.rgba.b = abs(s.rgba.b - d->rgba.b);
      break;
  }
  /* Write */
  if (alpha >= 254) {
    *d = s;
  } else if (d->rgba.a >= 254) {
    d->rgba.r = LERP(8, d->rgba.r, s.rgba.r, alpha);
    d->rgba.g = LERP(8, d->rgba.g, s.rgba.g, alpha);
    d->rgba.b = LERP(8, d->rgba.b, s.rgba.b, alpha);
  } else {
    int a = 0xff - (((0xff - d->rgba.a) * (0xff - alpha)) >> 8);
    int z = (d->rgba.a * (0xff - alpha)) >> 8;
    d->rgba.r = div8Table[((d->rgba.r * z) >>8) + ((s.rgba.r * alpha) >>8)][a];
    d->rgba.g = div8Table[((d->rgba.g * z) >>8) + ((s.rgba.g * alpha) >>8)][a];
    d->rgba.b = div8Table[((d->rgba.b * z) >>8) + ((s.rgba.b * alpha) >>8)][a];
    d->rgba.a = a;
  }
}


void sr_drawPixel(sr_Buffer *b, sr_Pixel c, int x, int y) {
  if (
    x >= b->clip.x && x < b->clip.x + b->clip.w &&
    y >= b->clip.y && y < b->clip.y + b->clip.h
  ) {
    blendPixel(&b->mode, b->pixels + x + y * b->w, c);
  }
}


void sr_drawLine(sr_Buffer *b, sr_Pixel c, int x0, int y0, int x1, int y1) {
  int x, y;
  int deltax, deltay;
  int error;
  int ystep;
  int steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    SWAP(int, x0, y0);
    SWAP(int, x1, y1);
  }
  if (x0 > x1) {
    SWAP(int, x0, x1);
    SWAP(int, y0, y1);
  }
  deltax = x1 - x0;
  deltay = abs(y1 - y0);
  error = deltax / 2;
  ystep = (y0 < y1) ? 1 : -1;
  y = y0;
  for (x = x0; x <= x1; x++) {
    if (steep) {
      sr_drawPixel(b, c, y, x);
    } else {
      sr_drawPixel(b, c, x, y);
    }
    error -= deltay;
    if (error < 0) {
      y += ystep;
      error += deltax;
    }
  }
}


void sr_drawRect(sr_Buffer *b, sr_Pixel c, int x, int y, int w, int h) {
  sr_Pixel *p;
  sr_Rect r = sr_rect(x, y, w, h);
  clipRect(&r, &b->clip);
  y = r.h;
  while (y--) {
    x = r.w;
    p = b->pixels + r.x + (r.y + y) * b->w;
    while (x--) {
      blendPixel(&b->mode, p++, c);
    }
  }
}


void sr_drawBox(sr_Buffer *b, sr_Pixel c, int x, int y, int w, int h) {
  sr_drawRect(b, c, x + 1, y, w - 1, 1);
  sr_drawRect(b, c, x, y + h - 1, w - 1, 1);
  sr_drawRect(b, c, x, y, 1, h - 1);
  sr_drawRect(b, c, x + w - 1, y + 1, 1, h - 1);
}


#define DRAW_ROW(x, y, len)\
  do {\
    int y__ = (y);\
    if (y__ >= 0 && ~rows[y__ >> 5] & (1 << (y__ & 31))) {\
      sr_drawRect(b, c, x, y__, len, 1);\
      rows[y__ >> 5] |= 1 << (y__ & 31);\
    }\
  } while (0)

void sr_drawCircle(sr_Buffer *b, sr_Pixel c, int x, int y, int r) {
  int dx = abs(r);
  int dy = 0;
  int radiusError = 1 - dx;
  unsigned rows[512];
  /* Clipped completely off-screen? */
  if (x + dx < b->clip.x || x - dx > b->clip.x + b->clip.w ||
      y + dx < b->clip.y || y - dx > b->clip.y + b->clip.h) return;
  /* zeroset bit array of drawn rows -- we keep track of which rows have been
   * drawn so that we can avoid overdraw */
  memset(rows, 0, sizeof(rows));
  while (dx >= dy) {
    DRAW_ROW(x - dx, y + dy, dx << 1);
    DRAW_ROW(x - dx, y - dy, dx << 1);
    DRAW_ROW(x - dy, y + dx, dy << 1);
    DRAW_ROW(x - dy, y - dx, dy << 1);
    dy++;
    if (radiusError < 0) {
      radiusError += 2 * dy + 1;
    } else {
      dx--;
      radiusError += 2 * (dy - dx + 1);
    }
  }
}

#undef DRAW_ROW


void sr_drawRing(sr_Buffer *b, sr_Pixel c, int x, int y, int r) {
  /* TODO : Prevent against overdraw? */
  int dx = abs(r);
  int dy = 0;
  int radiusError = 1 - dx;
  /* Clipped completely off-screen? */
  if (x + dx < b->clip.x || x - dx > b->clip.x + b->clip.w ||
      y + dx < b->clip.y || y - dx > b->clip.y + b->clip.h) return;
  /* Draw */
  while (dx >= dy) {
    sr_drawPixel(b, c,  dx + x,  dy + y);
    sr_drawPixel(b, c,  dy + x,  dx + y);
    sr_drawPixel(b, c, -dx + x,  dy + y);
    sr_drawPixel(b, c, -dy + x,  dx + y);
    sr_drawPixel(b, c, -dx + x, -dy + y);
    sr_drawPixel(b, c, -dy + x, -dx + y);
    sr_drawPixel(b, c,  dx + x, -dy + y);
    sr_drawPixel(b, c,  dy + x, -dx + y);
    dy++;
    if (radiusError < 0) {
      radiusError += 2 * dy + 1;
    } else {
      dx--;
      radiusError += 2 * (dy - dx + 1);
    }
  }
}


static void drawBufferBasic(
  sr_Buffer *b, sr_Buffer *src, int x, int y, sr_Rect s
) {
  int ix, iy;
  sr_Pixel *pd, *ps;
  /* Clip to destination buffer */
  clipRectAndOffset(&s, &x, &y, &b->clip);
  /* Clipped off screen? */
  if (s.w <= 0 || s.h <= 0) return;
  /* Draw */
  for (iy = 0; iy < s.h; iy++) {
    pd = b->pixels + x + (y + iy) * b->w;
    ps = src->pixels + s.x + (s.y + iy) * src->w;
    ix = s.w;
    while (ix--) {
      blendPixel(&b->mode, pd++, *ps++);
    }
  }
}


static void drawBufferScaled(
  sr_Buffer *b, sr_Buffer *src, int x, int y, sr_Rect s, sr_Transform a
) {
  float absSx = (a.sx < 0) ? -a.sx : a.sx;
  float absSy = (a.sy < 0) ? -a.sy : a.sy;
  int w = floor(s.w * absSx + .5);
  int h = floor(s.h * absSy + .5);
  int osx = (a.sx < 0) ? (s.w << FX_BITS) - 1 : 0;
  int osy = (a.sy < 0) ? (s.h << FX_BITS) - 1 : 0;
  int ix = (s.w << FX_BITS) / a.sx / s.w;
  int iy = (s.h << FX_BITS) / a.sy / s.h;
  int odx, dx, dy, sx, sy;
  int d;
  /* Adjust x/y depending on origin */
  x = x - ((a.sx < 0) ? w : 0) - (a.sx < 0 ? -1 : 1) * a.ox * absSx;
  y = y - ((a.sy < 0) ? h : 0) - (a.sy < 0 ? -1 : 1) * a.oy * absSy;
  /* Clipped completely offscreen horizontally? */
  if (x + w < b->clip.x || x > b->clip.x + b->clip.w) return;
  /* Adjust for clipping */
  dy = 0;
  odx = 0;
  if ((d = (b->clip.y - y)) > 0) { dy = d;  s.y += d / a.sy; }
  if ((d = (b->clip.x - x)) > 0) { odx = d; s.x += d / a.sx; }
  if ((d = ((y + h) - (b->clip.y + b->clip.h))) > 0) { h -= d; }
  if ((d = ((x + w) - (b->clip.x + b->clip.w))) > 0) { w -= d; }
  /* Draw */
  sy = osy;
  while (dy < h) {
    dx = odx;
    sx = osx;
    while (dx < w) {
      blendPixel(&b->mode, b->pixels + (x + dx) + (y + dy) * b->w,
                 src->pixels[(s.x + (sx >> FX_BITS)) +
                             (s.y + (sy >> FX_BITS)) * src->w]);
      sx += ix;
      dx++;
    }
    sy += iy;
    dy++;
  }
}


static void drawScanline(
  sr_Buffer *b, sr_Buffer *src, sr_Rect *s, int left, int right,
  int dy, int sx, int sy, int sxIncr, int syIncr
) {
  int d, dx;
  int x, y;
  /* Adjust for clipping */
  if (dy < b->clip.y || dy >= b->clip.y + b->clip.h) return;
  if ((d = b->clip.x - left) > 0) {
    left += d;
    sx += d * sxIncr;
    sy += d * syIncr;
  }
  if ((d = right - (b->clip.x + b->clip.w)) > 0) {
    right -= d;
  }
  /* Does the scaline length go out of bounds of our `s` rect? If so we
   * should adjust the scan line and the source coordinates accordingly */
checkSourceLeft:
  x = sx >> FX_BITS;
  y = sy >> FX_BITS;
  if (x < s->x || y < s->y || x >= s->x + s->w || y >= s->y + s->h) {
    left++;
    sx += sxIncr;
    sy += syIncr;
    if (left >= right) return;
    goto checkSourceLeft;
  }
checkSourceRight:
  x = (sx + sxIncr * (right - left)) >> FX_BITS;
  y = (sy + syIncr * (right - left)) >> FX_BITS;
  if (x < s->x || y < s->y || x >= s->x + s->w || y >= s->y + s->h) {
    right--;
    if (left >= right) return;
    goto checkSourceRight;
  }
  /* Draw */
  dx = left;
  while (dx < right) {
    blendPixel(&b->mode, b->pixels + dx + dy * b->w,
               src->pixels[(sx >> FX_BITS) +
                           (sy >> FX_BITS) * src->w]);
#if 0
    /* DEBUG : Out of bounds? draw pink pixels */
    /* TODO : Remove this */
    if ((sx >> FX_BITS) < 0 || (sx >> FX_BITS) >= src->w ||
        (sy >> FX_BITS) < 0 || (sy >> FX_BITS) >= src->h
    ) {
      sr_drawPixel(b, sr_color(255, 0, 255), dx, dy);
    }
#endif
    sx += sxIncr;
    sy += syIncr;
    dx++;
  }
}


static void drawBufferRotatedScaled(
  sr_Buffer *b, sr_Buffer *src, int x, int y, sr_Rect s, sr_Transform a
) {
  sr_Point p[4], top, bottom, left, right;
  int dy, xl, xr, il, ir;
  int sx, sy, sxi, syi, sxoi, syoi;
  int tsx, tsy, tsxi, tsyi;
  float cosr = cos(a.r);
  float sinr = sin(a.r);
  float absSx = (a.sx < 0) ? -a.sx : a.sx;
  float absSy = (a.sy < 0) ? -a.sy : a.sy;
  int invX = a.sx < 0;
  int invY = a.sy < 0;
  int w = s.w * absSx;
  int h = s.h * absSy;
  int q = a.r * 4 / PI2;
  float cosq = cos(q * PI2 / 4);
  float sinq = sin(q * PI2 / 4);
  float ox = (invX ? s.w - a.ox : a.ox) * absSx;
  float oy = (invY ? s.h - a.oy : a.oy) * absSy;
  /* Store rotated corners as points */
  p[0].x = x + cosr * (-ox    ) - sinr * (-oy    );
  p[0].y = y + sinr * (-ox    ) + cosr * (-oy    );
  p[1].x = x + cosr * (-ox + w) - sinr * (-oy    );
  p[1].y = y + sinr * (-ox + w) + cosr * (-oy    );
  p[2].x = x + cosr * (-ox + w) - sinr * (-oy + h);
  p[2].y = y + sinr * (-ox + w) + cosr * (-oy + h);
  p[3].x = x + cosr * (-ox    ) - sinr * (-oy + h);
  p[3].y = y + sinr * (-ox    ) + cosr * (-oy + h);
  /* Set named points based on rotation */
  top    = p[(-q + 0) & 3];
  right  = p[(-q + 1) & 3];
  bottom = p[(-q + 2) & 3];
  left   = p[(-q + 3) & 3];
  /* Clipped completely off screen? */
  if (bottom.y < b->clip.y || top.y  >= b->clip.y + b->clip.h) return;
  if (right.x  < b->clip.x || left.x >= b->clip.x + b->clip.w) return;
  /* Destination */
  xl = xr = top.x << FX_BITS;
  il = xdiv((left.x - top.x) << FX_BITS, left.y - top.y);
  ir = xdiv((right.x - top.x) << FX_BITS, right.y - top.y);
  /* Source */
  sxi  = xdiv(s.w << FX_BITS, w) * cos(-a.r);
  syi  = xdiv(s.h << FX_BITS, h) * sin(-a.r);
  sxoi = xdiv(s.w << FX_BITS, left.y - top.y) * sinq;
  syoi = xdiv(s.h << FX_BITS, left.y - top.y) * cosq;
  switch (q) {
    default:
    case 0:
      sx = s.x << FX_BITS;
      sy = s.y << FX_BITS;
      break;
    case 1:
      sx = s.x << FX_BITS;
      sy = ((s.y + s.h) << FX_BITS) - 1;
      break;
    case 2:
      sx = ((s.x + s.w) << FX_BITS) - 1;
      sy = ((s.y + s.h) << FX_BITS) - 1;
      break;
    case 3:
      sx = ((s.x + s.w) << FX_BITS) - 1;
      sy = s.y << FX_BITS;
      break;
  }
  /* Draw */
  if (left.y == top.y || right.y == top.y) {
    /* Adjust for right-angled rotation */
    dy = top.y - 1;
  } else {
    dy = top.y;
  }
  while (dy <= bottom.y) {
    /* Invert source iterators & increments if we are scaled negatively */
    if (invX) {
      tsx = ((s.x * 2 + s.w) << FX_BITS) - sx - 1;
      tsxi = -sxi;
    } else {
      tsx = sx;
      tsxi = sxi;
    }
    if (invY) {
      tsy = ((s.y * 2 + s.h) << FX_BITS) - sy - 1;
      tsyi = -syi;
    } else {
      tsy = sy;
      tsyi = syi;
    }
    /* Draw row */
    drawScanline(b, src, &s, xl >> FX_BITS, xr >> FX_BITS, dy,
                 tsx, tsy, tsxi, tsyi);
    sx += sxoi;
    sy += syoi;
    xl += il;
    xr += ir;
    dy++;
    /* Modify increments if we've reached the left or right corner */
    if (dy == left.y) {
      il = xdiv((bottom.x - left.x) << FX_BITS, bottom.y - left.y);
      sxoi = xdiv(s.w << FX_BITS, bottom.y - left.y) *  cosq;
      syoi = xdiv(s.h << FX_BITS, bottom.y - left.y) * -sinq;
    }
    if (dy == right.y) {
      ir = xdiv((bottom.x - right.x) << FX_BITS, bottom.y - right.y);
    }
  }
}


void sr_drawBuffer(
  sr_Buffer *b, sr_Buffer *src, int x, int y,
  sr_Rect *sub, sr_Transform *t
) {
  sr_Rect s;
  /* Init sub rect */
  if (sub) {
    if (sub->w <= 0 || sub->h <= 0) return;
    s = *sub;
    check(s.x >= 0 && s.y >= 0 && s.x + s.w <= src->w && s.y + s.h <= src->h,
          "sr_drawBuffer", "sub rectangle out of bounds");
  } else {
    s = sr_rect(0, 0, src->w, src->h);
  }
  /* Draw */
  if (!t) {
    drawBufferBasic(b, src, x, y, s);
  } else {
    sr_Transform a = *t;
    /* Move rotation value into 0..PI2 range */
    a.r = fmod(fmod(a.r, PI2) + PI2, PI2);
    /* Not rotated or scaled? apply offset and draw basic */
    if (a.r == 0 && a.sx == 1 && a.sy == 1) {
      x -= a.ox;
      y -= a.oy;
      drawBufferBasic(b, src, x, y, s);
    } else if (a.r == 0) {
      drawBufferScaled(b, src, x, y, s, a);
    } else {
      drawBufferRotatedScaled(b, src, x, y, s, a);
    }
  }
}
