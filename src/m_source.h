/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef M_SOURCE_H
#define M_SOURCE_H

#include "luax.h"
#include "wav.h"
#include "m_data.h"

#define STB_VORBIS_HEADER_ONLY
#include "lib/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY


#define SOURCE_CLASS_NAME "Source"

#define SOURCE_BUFFER_MAX  4096
#define SOURCE_BUFFER_MASK (SOURCE_BUFFER_MAX - 1)

struct Source;
struct SourceEvent;

typedef void (*SourceEventHandler)(struct Source*, struct SourceEvent*); 

typedef struct Source {
  int rawBufLeft[SOURCE_BUFFER_MAX];
  int rawBufRight[SOURCE_BUFFER_MAX];
  int buf[SOURCE_BUFFER_MAX];
  int dataRef, destRef, callbackRef, tableRef;
  Data *data;
  struct Source *dest;
  SourceEventHandler onEvent;
  int samplerate;
  int state;
  int flags;
  int length;
  int rate;
  long long position;
  int end;
  int bufEnd;
  int lgain, rgain;
  double gain, pan;
  /* Type-specific fields */
  union {
    /* .wav */
    struct { wav_t wav; int wavIdx; };
    /* .ogg */
    struct { stb_vorbis *oggStream; };
  };
} Source;

typedef struct SourceEvent {
  int type;
  lua_State *luaState;
  int offset;
  int len;
} SourceEvent;

#define SOURCE_FLOOP    (1 << 0)
#define SOURCE_FREPLACE (1 << 1)

enum {
  SOURCE_STATE_STOPPED,
  SOURCE_STATE_PLAYING,
  SOURCE_STATE_PAUSED,
};

enum {
  SOURCE_EVENT_NULL,
  SOURCE_EVENT_INIT,
  SOURCE_EVENT_DEINIT,
  SOURCE_EVENT_REWIND,
  SOURCE_EVENT_PROCESS,
};


Source *source_getMaster(int *ref);
void source_setLuaMutex(SDL_mutex *m);
void source_setSamplerate(int sr);
void source_processCommands(void);
void source_process(Source *s, int len);
void source_processAllSources(int len);

#endif
