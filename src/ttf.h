/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef TTF_H
#define TTF_H

#include "lib/stb_truetype.h"

typedef struct {
  stbtt_fontinfo font;
  void *fontData;
  float ptsize;
  float scale;
  int baseline;
} ttf_Font;


ttf_Font *ttf_new(const void *data, int len);
void ttf_destroy(ttf_Font *self);
void ttf_ptsize(ttf_Font *self, float ptsize);
int ttf_height(ttf_Font *self);
int ttf_width(ttf_Font *self, const char *str);
void *ttf_render(ttf_Font *self, const char *str, int *w, int *h);

#endif
