/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include "luax.h"
#include <SDL/SDL.h>

#define JUNO_VERSION "0.0.0"


static int l_juno_getVersion(lua_State *L) {
  lua_pushstring(L, JUNO_VERSION);
  return 1;
}


int luaopen_system(lua_State *L);
int luaopen_fs(lua_State *L);
int luaopen_time(lua_State *L);
int luaopen_graphics(lua_State *L);
int luaopen_audio(lua_State *L);
int luaopen_mouse(lua_State *L);
int luaopen_buffer(lua_State *L);
int luaopen_bufferfx(lua_State *L);
int luaopen_font(lua_State *L);
int luaopen_source(lua_State *L);
int luaopen_data(lua_State *L);
int luaopen_gif(lua_State *L);

int luaopen_juno(lua_State *L) {
  luaL_Reg reg[] = {
    { "getVersion",  l_juno_getVersion  },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);

  /* Init submodules */
  int i;
  struct { char *name; int (*fn)(lua_State *L); } mods[] = {
    /* Objects */
    { "Font",     luaopen_font      },
    { "Buffer",   luaopen_buffer    },
    { "Source",   luaopen_source    },
    { "Data",     luaopen_data      },
    { "Gif",      luaopen_gif       },
    /* Modules */
    { "system",   luaopen_system    },
    { "fs",       luaopen_fs        },
    { "time",     luaopen_time      },
    { "graphics", luaopen_graphics  },
    { "audio",    luaopen_audio     },
    { "mouse",    luaopen_mouse     },
    { "bufferfx", luaopen_bufferfx  },
    { NULL, NULL },
  };
  for (i = 0; mods[i].name; i++) {
    mods[i].fn(L);
    lua_setfield(L, -2, mods[i].name);
  }

  return 1;
}
