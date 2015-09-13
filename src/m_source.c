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
#include "luax.h"
#include "fs.h"
#include "util.h"
#include "lib/vec/vec.h"
#include "m_source.h"

#define CLASS_NAME SOURCE_CLASS_NAME

#define FX_BITS (12)
#define FX_UNIT (1 << FX_BITS)
#define FX_MASK (FX_UNIT - 1)
#define FX_LERP(a, b, p) ((a) + ((((b) - (a)) * (p)) >> (FX_BITS)))

static int samplerate = 44100;
static Source *master;
static int masterRef = LUA_NOREF;
static vec_t(Source*) sources;
static SDL_mutex *luaMutex;
static lua_State *luaState;


/* Sources can have assigned different streams, each stream has its own onEvent
 * function to handle the initing, deiniting, processing and rewinding of a
 * stream. A source works with a stream through the use of a stereo ring buffer
 * (Source.rawBufLeft, Source.rawBufRight).  When the source requires a sample
 * which the stream has not created yet it emits the STREAM_EVENT_PROCESS with
 * an offset and length into the ring buffer, the stream fills this buffer with
 * stereo 16bit audio for the given length and offset. The raw PCM is then
 * linearly interpolated and written to the Source's main output buffer
 * (Source.buf) at the Source's given playback rate (Source.rate). The Lua
 * callback is run (if it exists), gains are applied, then the Source writes
 * its output to its destination.
 *
 * In a STREAM_EVENT_PROCESS event, when a stream reaches the end of its data
 * before it has filled the requested amount of the raw PCM buffer, it loops
 * back around to the beginning of the file and continue filling the buffer.
 * This allows the Source to continue playing beyond the length of the stream's
 * data (Source.length) if it wants to loop, in the case that it doesn't it
 * will simply stop once it reaches the end (Source.end).
 *
 * `Source` structures are not directly stored as a lua udata, but rather a
 * pointer to a Source is stored as a udata so that a Source can continue to
 * exist after being GCed until it can be safely destroyed in the audio thread.
 *
 *  Raw PCM ring buffer
 *  -------------------
 *
 *                                  +-- Current playhead position
 *                                  |              (Source.position)
 *                          +-------|----------+
 *                          | ......>ooooooooo |
 *                          | . +----------+ o |
 *         Past samples ----- . |          | o --- Future samples 
 *                          | . +----------+ o |
 *                          | ............oooo |
 *                          +------------|-----+
 *                               |       |
 *        Raw PCM ring buffers --+       +-- End of future samples
 *                (Source.rawBufLeft,                 (Source.bufEnd)
 *                 Source.rawBufRight)
 *
 *
 *  Process chain
 *  -------------
 *
 *    +------------------------+  +----------+  +-------+  +-------------+
 *    | Write interpolated raw |->| Do lua   |->| Apply |->| Write to    |
 *    | PCM to Source.buf      |  | callback |  | gains |  | destination |
 *    +------------------------+  +----------+  +-------+  +-------------+
 *          |            ^
 *          v            |
 *     +----------------------+
 *     | Process samples from |
 *     | stream to raw buffer |
 *     +----------------------+
 *
 *
 *  Commands
 *  --------
 *
 *  All commands issued by lua are pushed to the command vector (`commands`).
 *  
 *  In the audio thread's callback each command is processed via
 *  `source_processCommand()` which also clears the vector. Each Source's audio
 *  is then processed via `source_ProcessAllSources()` (see [Process Chain]
 *  above)
 *
 */


typedef struct {
  int type;
  Source *source;
  int i;
  double f;
  void *p;
} Command;

enum {
  COMMAND_NULL,
  COMMAND_ADD,
  COMMAND_DESTROY,
  COMMAND_PLAY,
  COMMAND_PAUSE,
  COMMAND_STOP,
  COMMAND_SET_CALLBACK,
  COMMAND_SET_DESTINATION,
  COMMAND_SET_GAIN,
  COMMAND_SET_PAN,
  COMMAND_SET_RATE,
  COMMAND_SET_LOOP
};

static vec_t(Command) commands;
static SDL_mutex *commandMutex;

static Command command(int type, Source *source) {
  Command c;
  memset(&c, 0, sizeof(c));
  c.type = type;
  c.source = source;
  return c;
}

static void pushCommand(Command *c) {
  ASSERT(SDL_LockMutex(commandMutex) == 0);
  int err = vec_push(&commands, *c);
  ASSERT(err == 0);
  ASSERT(SDL_UnlockMutex(commandMutex) == 0);
}

static void lockLua(void) {
  ASSERT(luaMutex);
  ASSERT(SDL_LockMutex(luaMutex) == 0);
}

static void unlockLua(void) {
  ASSERT(luaMutex);
  ASSERT(SDL_UnlockMutex(luaMutex) == 0);
}

static SourceEvent event(int type) {
  SourceEvent e;
  memset(&e, 0, sizeof(e));
  e.type = type;
  return e;
}

static void emitEvent(Source *s, SourceEvent *e) {
  if (s->onEvent) {
    s->onEvent(s, e);
  }
}

static void recalcGains(Source *self) {
  double left, right;
  double pan = CLAMP(self->pan, -1., 1.);
  double gain = MAX(self->gain, 0.);
  /* Get linear gains */
  left  = ((pan < 0.) ? 1. : (1. - pan)) * gain;
  right = ((pan > 0.) ? 1. : (1. + pan)) * gain; 
  /* Apply curve */
  left = left * left;
  right = right * right;
  /* Set fixedpoint gains */
  self->lgain = left * FX_UNIT;
  self->rgain = right * FX_UNIT;
}

static Source *checkSource(lua_State *L, int idx) {
  Source **p = luaL_checkudata(L, idx, CLASS_NAME);
  return *p;
}

static Source *newSource(lua_State *L) {
  Source *self = malloc(sizeof(*self));
  ASSERT(self);
  memset(self, 0, sizeof(*self));
  self->dataRef = LUA_NOREF;
  self->destRef = LUA_NOREF;
  self->callbackRef = LUA_NOREF;
  self->tableRef = LUA_NOREF;
  self->gain = 1.;
  self->pan = 0.;
  recalcGains(self);
  /* Init lua pointer to the actual Source struct */
  Source **p = lua_newuserdata(L, sizeof(self));
  luaL_setmetatable(L, CLASS_NAME);
  *p = self;
  return self;
}


static void destroySource(Source *self) {
  /* Note: All the lua references of the source should be unreferenced before
   * the source is destroyed. The source should also be removed from the
   * `sources` vector -- this is done in `source_processCommands()`, the only
   * place this function should ever be called. */
  SourceEvent e = event(SOURCE_EVENT_DEINIT);
  emitEvent(self, &e);
  free(self);
}

static double getBaseRate(Source *self) {
  return (double) self->samplerate / (double) samplerate;
}

static void rewindStream(Source *self, long long position) {
  /* Rewind stream */
  SourceEvent e = event(SOURCE_EVENT_REWIND);
  emitEvent(self, &e);
  /* Process first chunk and reset */
  e = event(SOURCE_EVENT_PROCESS);
  e.offset = 0;
  e.len = SOURCE_BUFFER_MAX / 2;
  emitEvent(self, &e);
  self->end = self->length;
  self->bufEnd = e.len;
  self->position = position;
}


static void onEventWav(Source *s, SourceEvent *e) {
  switch (e->type) {

    case SOURCE_EVENT_INIT: {
      int err = wav_read(&s->wav, s->data->data, s->data->len);
      if (err != WAV_ESUCCESS) {
        luaL_error(e->luaState,
                  "could not init wav stream: %s", wav_strerror(err));
      }
      if (s->wav.bitdepth != 16) {
        luaL_error(e->luaState,
                  "could not init wav stream, expected 16bit wave");
      }
      if (s->wav.channels != 1 && s->wav.channels != 2) {
        luaL_error(e->luaState,
                  "could not init wav stream, expected mono/stereo wave");
      }
      s->length = s->wav.length;
      s->samplerate = s->wav.samplerate;
      break;
    }

    case SOURCE_EVENT_REWIND:
      s->wavIdx = 0;
      break;

    case SOURCE_EVENT_PROCESS: {
      int i, x;
      for (i = 0; i < e->len; i++) {
        /* Hit the end? Rewind and continue */
        if (s->wavIdx >= s->length) {
          s->wavIdx = 0;
        }
        /* Process */
        int idx = (e->offset + i) & SOURCE_BUFFER_MASK;
        if (s->wav.channels == 2) {
          /* Process stereo */
          x = s->wavIdx << 1;
          s->rawBufLeft[idx]  = ((short*) s->wav.data)[x];
          s->rawBufRight[idx] = ((short*) s->wav.data)[x + 1];
        } else {
          /* Process mono */
          s->rawBufLeft[idx] =
          s->rawBufRight[idx] = ((short*) s->wav.data)[s->wavIdx];
        }
        s->wavIdx++;
      }
      break;
    }
  }
}


static void onEventOgg(Source *s, SourceEvent *e) {
  switch (e->type) {

    case SOURCE_EVENT_INIT: {
      int err;
      s->oggStream = stb_vorbis_open_memory(s->data->data, s->data->len,
                                            &err, NULL);
      if (!s->oggStream) {
        luaL_error(e->luaState, "could not init ogg stream; bad data?");
      }
      stb_vorbis_info info = stb_vorbis_get_info(s->oggStream);
      s->samplerate = info.sample_rate;
      s->length = stb_vorbis_stream_length_in_samples(s->oggStream);
      break;
    }

    case SOURCE_EVENT_DEINIT:
      if (s->oggStream) {
        stb_vorbis_close(s->oggStream);
      }
      break;

    case SOURCE_EVENT_REWIND:
      stb_vorbis_seek_start(s->oggStream);
      break;
      
    case SOURCE_EVENT_PROCESS: {
      int i, n;
      short buf[SOURCE_BUFFER_MAX];
      int len = e->len * 2;
      int z = e->offset;
fill:
      n = stb_vorbis_get_samples_short_interleaved(s->oggStream, 2, buf, len);
      n *= 2;
      for (i = 0; i < n; i += 2) {
        int idx = z++ & SOURCE_BUFFER_MASK;
        s->rawBufLeft [idx] = buf[i];
        s->rawBufRight[idx] = buf[i + 1];
      }
      /* Reached end of stream before the end of the buffer? rewind and fill
       * remaining buffer */
      if (len != n) {
        stb_vorbis_seek_start(s->oggStream);
        len -= n;
        goto fill;
      }
      break;
    }

  }
}


Source *source_getMaster(int *ref) {
  if (ref) {
    *ref = masterRef;
  }
  return master;
}


void source_setLuaMutex(SDL_mutex *m) {
  luaMutex = m;
}


void source_setSamplerate(int sr) {
  samplerate = sr;
}


void source_processCommands(void) {
  int i;
  Command *c;
  vec_t(int) oldRefs;
  vec_init(&oldRefs);
  /* Handle commands */
  ASSERT(SDL_LockMutex(commandMutex) == 0);
  vec_foreach_ptr(&commands, c, i) {
    switch (c->type) {
      case COMMAND_ADD:
        vec_push(&sources, c->source);
        break;

      case COMMAND_DESTROY:
        vec_push(&oldRefs, c->source->dataRef);
        vec_push(&oldRefs, c->source->destRef);
        vec_push(&oldRefs, c->source->callbackRef);
        vec_push(&oldRefs, c->source->tableRef);
        vec_remove(&sources, c->source);
        destroySource(c->source);
        break;

      case COMMAND_PLAY:
        if (c->i || c->source->state == SOURCE_STATE_STOPPED) {
          rewindStream(c->source, 0);
        }
        c->source->state = SOURCE_STATE_PLAYING;
        break;

      case COMMAND_PAUSE:
        if (c->source->state == SOURCE_STATE_PLAYING) {
          c->source->state = SOURCE_STATE_PAUSED;
        }
        break;

      case COMMAND_STOP:
        c->source->state = SOURCE_STATE_STOPPED;
        break;

      case COMMAND_SET_CALLBACK:
        vec_push(&oldRefs, c->source->callbackRef);
        c->source->callbackRef = c->i;
        break;

      case COMMAND_SET_DESTINATION:
        vec_push(&oldRefs, c->source->destRef);
        c->source->destRef = c->i;
        c->source->dest = c->p;
        break;

      case COMMAND_SET_GAIN:
        c->source->gain = c->f;
        recalcGains(c->source);
        break;

      case COMMAND_SET_PAN:
        c->source->pan = c->f;
        recalcGains(c->source);
        break;

      case COMMAND_SET_RATE:
        c->source->rate = getBaseRate(c->source) * c->f * FX_UNIT;
        break;

      case COMMAND_SET_LOOP:
        if (c->i) {
          c->source->flags |= SOURCE_FLOOP;
        } else {
          c->source->flags &= ~SOURCE_FLOOP;
        }
        break;
    }
  }
  /* Clear command vector */
  vec_clear(&commands);
  ASSERT(SDL_UnlockMutex(commandMutex) == 0);
  /* Remove old Lua references */
  if (oldRefs.length > 0) {
    int i, ref;
    lockLua();
    vec_foreach(&oldRefs, ref, i) {
      luaL_unref(luaState, LUA_REGISTRYINDEX, ref);
    }
    unlockLua();
  }
  vec_deinit(&oldRefs);
}


void source_process(Source *self, int len) {
  int i;
  /* Replace flag still set? Zeroset the buffer */
  if (self->flags & SOURCE_FREPLACE) {
    memset(self->buf, 0, sizeof(*self->buf) * len);
  }
  /* Process audio stream and add to our buffer */
  if (self->state == SOURCE_STATE_PLAYING && self->onEvent) {
    for (i = 0; i < len; i += 2) {
      int idx = (self->position >> FX_BITS);
      /* Process the stream and fill the raw buffer if the next index requires
       * samples we don't yet have */
      if (idx + 1 >= self->bufEnd) {
        SourceEvent e = event(SOURCE_EVENT_PROCESS);
        e.offset = (self->bufEnd) & SOURCE_BUFFER_MASK;
        e.len = SOURCE_BUFFER_MAX / 2;
        emitEvent(self, &e);
        self->bufEnd += e.len;
      }
      /* Have we reached the end? */
      if (idx >= self->end) {
        /* Not set to loop? Stop and stop processing */
        if (~self->flags & SOURCE_FLOOP) {
          self->state = SOURCE_STATE_STOPPED;
          break;
        }
        /* Set to loop: Streams always fill the raw buffer in a loop, so we
         * just increment the end index by the stream's length so that it
         * continues for another iteration of the sound file */
        self->end = idx + self->length;
      }
      /* Write interpolated frame to buffer */
      int p = self->position & FX_MASK;
      /* Left */
      int la = self->rawBufLeft[idx       & SOURCE_BUFFER_MASK];
      int lb = self->rawBufLeft[(idx + 1) & SOURCE_BUFFER_MASK];
      self->buf[i] += FX_LERP(la, lb, p);
      /* Right */
      int ra = self->rawBufRight[idx       & SOURCE_BUFFER_MASK];
      int rb = self->rawBufRight[(idx + 1) & SOURCE_BUFFER_MASK];
      self->buf[i + 1] += FX_LERP(ra, rb, p);
      /* Increment position */
      self->position += self->rate;
    }
  }
  /* Do lua callback */
  if (self->callbackRef != LUA_NOREF) {
    lockLua();
    lua_State *L = luaState;
    /* Get _pcall function */
    lua_getglobal(L, "juno");
    if (!lua_isnil(L, -1)) {
      lua_getfield(L, -1, "_pcall");
      if (!lua_isnil(L, -1)) {
        int i, n;
        /* Get callback function */
        lua_rawgeti(L, LUA_REGISTRYINDEX, self->callbackRef);
        /* Create a table for our PCM if we don't already have one */
        if (self->tableRef == LUA_NOREF) {
          lua_newtable(L);
          self->tableRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        /* Push our table and copy buffer PCM (int16 -> double) */
        lua_rawgeti(L, LUA_REGISTRYINDEX, self->tableRef);
        for (i = 0; i < len; i++) {
          lua_pushnumber(L, ((double) self->buf[i]) * 3.0517578125e-05);
          lua_rawseti(L, -2, i + 1);
        }
        /* If the table is longer than `len` the last call must have had a
         * larger `len`; we nil the additional elements to remove them */
        n = lua_rawlen(L, -1);
        for (i = n; i < len; i++) {
          lua_pushnil(L);
          lua_rawseti(L, -2, i + 1);
        }
        /* Copy and insert the table to before the two function arguments so we
         * can access it to copy the PCM data back to Source.buf below */
        lua_pushvalue(L, -1);
        lua_insert(L, -4);
        /* Call function: juno._pcall(callback, table) -> 1 return */
        lua_call(L, 2, 1);
        /* Check return value -- if it is false then an error occured, in this
         * case we unset the callback function so it doesn't keep getting
         * called */
        if (!lua_toboolean(L, -1)) {
          luaL_unref(L, LUA_REGISTRYINDEX, self->callbackRef);
          self->callbackRef = LUA_NOREF;
        }
        lua_pop(L, 1); /* Pop function return value */
        /* Copy contents of table back to buffer (double -> int16) */
        n = lua_rawlen(L, -1);
        for (i = 0; i < n; i++) {
          lua_rawgeti(L, -1, i + 1);
          self->buf[i] = lua_tonumber(L, -1) * 32768.;
          lua_pop(L, 1);
        }
        /* Pop PCM table */
        lua_pop(L, 1);
      }
      /* Pop `juno` table */
      lua_pop(L, 1);
    }
    unlockLua();
  }
  /* Apply gains */
  for (i = 0; i < len; i += 2) {
    self->buf[i    ] = (self->buf[i    ] * self->lgain) >> FX_BITS;
    self->buf[i + 1] = (self->buf[i + 1] * self->rgain) >> FX_BITS;
  }
  /* Write to destination */
  if (self->dest) {
    if (self->dest->flags & SOURCE_FREPLACE) {
      memcpy(self->dest->buf, self->buf, sizeof(*self->buf) * len);
      self->dest->flags &= ~SOURCE_FREPLACE;
    } else {
      for (i = 0; i < len; i++) {
        self->dest->buf[i] += self->buf[i];
      }
    }
  }
  /* Reset our flag as to replace the buffer's content */
  self->flags |= SOURCE_FREPLACE;
}


void source_processAllSources(int len) {
  int i;
  Source *s;
  /* Sources are processed in reverse (newer Sources are processed first) --
   * this assures the master is processed last */
  vec_foreach_rev(&sources, s, i) {
    source_process(s, len);
  }
}


static int l_source_fromData(lua_State *L) {
  Data *data = luaL_checkudata(L, 1, DATA_CLASS_NAME);
  Source *self = newSource(L);
  /* Init data reference */
  self->data = data;
  lua_pushvalue(L, 1);
  self->dataRef = luaL_ref(L, LUA_REGISTRYINDEX);
  /* Detect format and set appropriate event handler */
  /* Is .wav? */
  if (data->len > 12 && !memcmp(((char*) data->data) + 8, "WAVE", 4)) {
    self->onEvent = onEventWav;
    goto init;
  }
  /* Is .ogg? */
  if (data->len > 4 && !memcmp(data->data, "OggS", 4)) {
    self->onEvent = onEventOgg;
    goto init;
  }
  /* Made it here? Error out because we couldn't detect the format */
  luaL_error(L, "could not init Source; bad Data format?");
  /* Init stream */
init:;
  SourceEvent e = event(SOURCE_EVENT_INIT);
  e.luaState = L;
  emitEvent(self, &e);
  /* Init */
  self->rate = getBaseRate(self) * FX_UNIT;
  self->dest = master;
  /* Issue "add" command to push to `sources` vector */
  Command c = command(COMMAND_ADD, self);
  pushCommand(&c);
  return 1;
}


static int l_source_fromBlank(lua_State *L) {
  Source *self = newSource(L);
  /* Init */
  self->dest = master;
  /* Issue "add" command to push to `sources` vector */
  Command c = command(COMMAND_ADD, self);
  pushCommand(&c);
  return 1;
}


static int l_source_gc(lua_State *L) {
  Source *self = checkSource(L, 1);
  Command c = command(COMMAND_DESTROY, self);
  pushCommand(&c);
  return 0;
}


static int l_source_getLength(lua_State *L) {
  Source *self = checkSource(L, 1);
  lua_pushnumber(L, getBaseRate(self) * self->length / self->samplerate);
  return 1;
}


static int l_source_getState(lua_State *L) {
  Source *self = checkSource(L, 1);
  switch (self->state) {
    case SOURCE_STATE_PLAYING : lua_pushstring(L, "playing"); break;
    case SOURCE_STATE_PAUSED  : lua_pushstring(L, "paused");  break;
    case SOURCE_STATE_STOPPED : lua_pushstring(L, "stopped"); break;
    default                   : lua_pushstring(L, "?");       break;
  }
  return 1;
}


static int l_source_setCallback(lua_State *L) {
  Source *self = checkSource(L, 1);
  if (!lua_isnoneornil(L, 2) && lua_type(L, 2) != LUA_TFUNCTION) {
    luaL_argerror(L, 2, "expected function");
  }
  Command c = command(COMMAND_SET_CALLBACK, self);
  if (!lua_isnoneornil(L, 2)) {
    lua_pushvalue(L, 2);
    c.i = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    c.i = LUA_NOREF;
  }
  pushCommand(&c);
  return 0;
}


static int l_source_setDestination(lua_State *L) {
  Source *self = checkSource(L, 1);
  Source *dest = master;
  if (!lua_isnoneornil(L, 2)) {
    dest = checkSource(L, 2);
  }
  /* Master? Disallow */
  if (self == master) {
    luaL_argerror(L, 1, "master cannot be rerouted");
  }
  /* Check for feedback loop */
  Source *s = dest;
  while (s) {
    if (s == self) {
      luaL_error(L, "routing results in a feedback loop");
    }
    s = s->dest;
  }
  /* Do routing */
  Command c = command(COMMAND_SET_DESTINATION, self);
  if (!lua_isnoneornil(L, 2)) {
    lua_pushvalue(L, 2);
  } else {
    lua_rawgeti(L, LUA_REGISTRYINDEX, masterRef);
  }
  c.i = luaL_ref(L, LUA_REGISTRYINDEX);
  c.p = dest;
  pushCommand(&c);
  return 0;
}


static int l_source_setGain(lua_State *L) {
  Source *self = checkSource(L, 1);
  double gain = luaL_optnumber(L, 2, 1.);
  Command c = command(COMMAND_SET_GAIN, self);
  c.f = gain;
  pushCommand(&c);
  return 0;
}


static int l_source_setPan(lua_State *L) {
  Source *self = checkSource(L, 1);
  double pan = luaL_optnumber(L, 2, 0.);
  Command c = command(COMMAND_SET_PAN, self);
  c.f = pan;
  pushCommand(&c);
  return 0;
}


static int l_source_setRate(lua_State *L) {
  Source *self = checkSource(L, 1);
  double rate = luaL_optnumber(L, 2, 1.);
  if (rate < 0) {
    luaL_argerror(L, 2, "expected value of zero or greater");
  }
  if (rate > 16) {
    luaL_argerror(L, 2, "value is too large");
  }
  Command c = command(COMMAND_SET_RATE, self);
  c.f = rate;
  pushCommand(&c);
  return 0;
}


static int l_source_setLoop(lua_State *L) {
  Source *self = checkSource(L, 1);
  int loop = luax_optboolean(L, 2, 0);
  Command c = command(COMMAND_SET_LOOP, self);
  c.i = loop;
  pushCommand(&c);
  return 0;
}


static int l_source_play(lua_State *L) {
  Source *self = checkSource(L, 1);
  int reset = luax_optboolean(L, 2, 0);
  Command c = command(COMMAND_PLAY, self);
  c.i = reset;
  pushCommand(&c);
  return 0;
}


static int l_source_pause(lua_State *L) {
  Source *self = checkSource(L, 1);
  Command c = command(COMMAND_PAUSE, self);
  pushCommand(&c);
  return 0;
}


static int l_source_stop(lua_State *L) {
  Source *self = checkSource(L, 1);
  Command c = command(COMMAND_STOP, self);
  pushCommand(&c);
  return 0;
}


int luaopen_source(lua_State *L) {
  luaL_Reg reg[] = {
    { "__gc",           l_source_gc             },
    { "fromData",       l_source_fromData       },
    { "fromBlank",      l_source_fromBlank      },
    { "getLength",      l_source_getLength      },
    { "getState",       l_source_getState       },
    { "setCallback",    l_source_setCallback    },
    { "setDestination", l_source_setDestination },
    { "setGain",        l_source_setGain        },
    { "setPan",         l_source_setPan         },
    { "setRate",        l_source_setRate        },
    { "setLoop",        l_source_setLoop        },
    { "play",           l_source_play           },
    { "pause",          l_source_pause          },
    { "stop",           l_source_stop           },
    { NULL, NULL }
  };
  ASSERT( luaL_newmetatable(L, CLASS_NAME) );
  luaL_setfuncs(L, reg, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  /* Init command mutex */
  commandMutex = SDL_CreateMutex();
  ASSERT(commandMutex);
  /* Set lua state */
  luaState = L;
  /* Init master */
  master = newSource(L);
  masterRef = luaL_ref(L, LUA_REGISTRYINDEX);
  Command c = command(COMMAND_ADD, master);
  pushCommand(&c);
  return 1;
}

