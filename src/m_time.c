/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/time.h>
#endif
#include "luax.h"
#include <SDL/SDL.h>


static int l_time_getNow(lua_State *L) {
  double t;
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  t = (ft.dwHighDateTime * 4294967296.0 / 1e7) + ft.dwLowDateTime / 1e7;
  t -= 11644473600.0;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  t = tv.tv_sec + tv.tv_usec / 1e6;
#endif
  lua_pushnumber(L, t);
  return 1;
}


static int l_time_getTime(lua_State *L) {
  lua_pushnumber(L, SDL_GetTicks() / 1000.);
  return 1;
}


static int l_time_sleep(lua_State *L) {
  SDL_Delay(luaL_checknumber(L, 1) * 1000.);
  return 0;
}


int luaopen_time(lua_State *L) {
  luaL_Reg reg[] = {
    { "getNow",   l_time_getNow   },
    { "getTime",  l_time_getTime  },
    { "sleep",    l_time_sleep    },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);
  return 1;
}
