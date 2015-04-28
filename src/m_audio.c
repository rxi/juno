/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <SDL/SDL.h>
#include "luax.h"
#include "util.h"
#include "m_source.h"


static int inited = 0;
static double samplerate = 0;


static void audioCallback(void *udata, Uint8 *stream, int size) {
  lua_State *L = udata;
  int16_t *buffer = (void*) stream;
  int len = size >> 1;
  int i;
  UNUSED(L);
  /* Process source commands */
  source_processCommands();
  /* Process sources audio */
  source_processAllSources(len);
  /* Copy master to buffer */
  Source *master = source_getMaster(NULL);
  for (i = 0; i < len; i++) {
    int x = master->buf[i];
    buffer[i] = CLAMP(x, -32768, 32767);
  }
}


static int l_audio_init(lua_State *L) {
  int rate = luaL_optint(L, 1, 44100);
  int bufferSize = luaL_optint(L, 2, 44100);
  if (inited) {
    luaL_error(L, "audio is already inited");
  }
  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    luaL_error(L, "could not init audio");
  }
  /* Init format, open and start */
  SDL_AudioSpec fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.freq      = rate;
  fmt.format    = AUDIO_S16;
  fmt.channels  = 2;
  fmt.callback  = audioCallback;
  fmt.samples   = bufferSize;
  fmt.userdata  = L;
  if (SDL_OpenAudio(&fmt, 0) < 0) {
    luaL_error(L, "could not open audio");
  }
  /* Set state */
  samplerate = rate;
  inited = 1;
  /* Init Source state */
  source_setSamplerate(samplerate);
  /* Start audio */
  SDL_PauseAudio(0);
  return 0;
}


int luaopen_audio(lua_State *L) {
  luaL_Reg reg[] = {
    { "init",   l_audio_init   },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);
  /* Add .master Source field */
  int ref;
  source_getMaster(&ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  lua_setfield(L, -2, "master");
  return 1;
}
