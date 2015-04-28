/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef LUAX_H
#define LUAX_H

//#include <lua5.1/lua.h>
//#include <lua5.1/lualib.h>
//#include <lua5.1/lauxlib.h>

#include "lib/lua51/lua.h"
#include "lib/lua51/lualib.h"
#include "lib/lua51/lauxlib.h"
#include "lib/lextlib_lua52.h"

#define luax_setfield_(T, L, k, v)\
   do { lua_push##T(L, v); lua_setfield(L, -2, k); } while (0)

#define luax_setfield_number(L, k, v) luax_setfield_(number, L, k, v)
#define luax_setfield_string(L, k, v) luax_setfield_(string, L, k, v)
#define luax_setfield_bool(L, k, v)   luax_setfield_(boolean, L, k, v)
#define luax_setfield_udata(L, k, v)  luax_setfield_(lightuserdata, L, k, v)
#define luax_setfield_cfunc(L, k, v)  luax_setfield_(cfunction, L, k, v)
#define luax_setfield_fstring(L, k, ...)\
  do { lua_pushfstring(L, __VA_ARGS__); lua_setfield(L, -2, k); } while (0)


#define luax_optboolean(L, i, x)\
  (!lua_isnoneornil(L, i) ? lua_toboolean(L, i) : (x))

#define luax_optudata(L, i, name, x)\
  (!lua_isnoneornil(L, i) ? luaL_checkudata(L, i, name) : (x))

#endif
