/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <SDL/SDL.h>
#include "luax.h"


static int l_mouse_setVisible(lua_State *L) {
  SDL_ShowCursor(lua_toboolean(L, 1));
  return 0;
}


static int l_mouse_setPosition(lua_State *L) {
  SDL_WarpMouse(luaL_checknumber(L, 1), luaL_checknumber(L, 2));
  return 0;
}


int luaopen_mouse(lua_State *L) {
  luaL_Reg reg[] = {
    { "setVisible",   l_mouse_setVisible  },
    { "setPosition",  l_mouse_setPosition },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);
  return 1;
}
