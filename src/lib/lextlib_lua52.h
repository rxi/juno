#ifndef LEXTLIB_LUA52_H
#define LEXTLIB_LUA52_H

/** 
 * The original version of this file is located at
 * https://github.com/devurandom/lextlib/blob/master/lextlib_lua52.h
 *
 * Its license is as follows:
 *
 * Copyright (c) 2012 Dennis Schridde
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include <assert.h>

//#include <lua.h>
//#include <lauxlib.h>


#if LUA_VERSION_NUM < 502
/* Error codes for lua_load: */
#	define LUA_OK 0
/* WARNING: This error does not exist in Lua 5.1 */
#	define LUA_ERRGCMM (-1)

/* Comparison types for lua_compare: */
#	define LUA_OPEQ 0
#	define LUA_OPLT 1
#	define LUA_OPLE 2

/* WARNING: Not entirely correct, but should work anyway */
#	define lua_rawlen lua_objlen

#	define lua_absindex(L, i) (((i) > 0 || (i) < LUA_REGISTRYINDEX) ? (i) : lua_gettop(L)+(i)+1)

/* WARNING: Something very different, but it might get your job done */
#	define lua_getuservalue lua_getfenv
#	define lua_setuservalue lua_setfenv

/* WARNING: Probably slower than Lua 5.2's implementation */
#	define lua_compare luaX52_lua_compare

#	define lua_tonumberx(L,i,b) (lua_isnumber(L,(i)) ? (*(b)=1, lua_tonumber(L,(i))) : (*(b)=0, 0))
#	define lua_tointegerx(L,i,b) (lua_isnumber(L,(i)) ? (*(b)=1, lua_tointeger(L,(i))) : (*(b)=0, 0))

#	define luaL_getsubtable luaX52_luaL_getsubtable

#	define luaL_newlib(L,l) (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#	define luaL_newlibtable(L,l) (lua_createtable(L,0,sizeof(l)))

# define luaL_requiref(L,l,f,g) luaX52_luaL_requiref(L,(l),(f),(g))

#	define luaL_setfuncs luaX52_luaL_setfuncs

#	define luaL_setmetatable(L,t) (luaL_getmetatable(L,t), lua_setmetatable(L,-2))

# define luaL_testudata(L,i,t) luaX52_luaL_testudata(L,(i),(t))

static inline void luaX52_luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		for (int i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -nup);
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);  /* remove upvalues */
}

static inline int luaX52_lua_compare(lua_State *L, int index1, int index2, int op) {
	assert(op == LUA_OPEQ || op == LUA_OPLT || op == LUA_OPLE);
	switch (op) {
	case LUA_OPEQ:
		return lua_equal(L, index1, index2);
	case LUA_OPLT:
		return lua_lessthan(L, index1, index2);
	case LUA_OPLE:
		return lua_lessthan(L, index1, index2) || lua_equal(L, index1, index2);
	default:
		return luaL_error(L, "Call to lua_compare with unsupported operator %d", op);
	}
}

static inline int luaX52_luaL_getsubtable(lua_State *L, int idx, const char *fname) {
	lua_getfield(L, idx, fname);
	if (lua_istable(L, -1)) return 1;  /* table already there */
	else {
		lua_pop(L, 1);  /* remove previous result */
		idx = lua_absindex(L, idx);
		lua_newtable(L);
		lua_pushvalue(L, -1);  /* copy to be left at top */
		lua_setfield(L, idx, fname);  /* assign new table to field */
		return 0;  /* false, because did not find table there */
	}
}

static inline void luaX52_luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb) {
	lua_pushcfunction(L, openf);
	lua_pushstring(L, modname);  /* argument to open function */
	lua_call(L, 1, 1);  /* open module */
	luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_pushvalue(L, -2);  /* make copy of module (call result) */
	lua_setfield(L, -2, modname);  /* _LOADED[modname] = module */
	lua_pop(L, 1);  /* remove _LOADED table */
	if (glb) {
		lua_pushvalue(L, -1);  /* copy of 'mod' */
		lua_setglobal(L, modname);  /* _G[modname] = module */
	}
}

static inline void *luaX52_luaL_testudata(lua_State *L, int ud, const char *tname) {
	void *p = lua_touserdata(L, ud);
	if (p != NULL) {  /* value is a userdata? */
		if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
			luaL_getmetatable(L, tname);  /* get correct metatable */
			if (!lua_rawequal(L, -1, -2))  /* not the same? */
				p = NULL;  /* value is a userdata with wrong metatable */
			lua_pop(L, 2);  /* remove both metatables */
			return p;
		}
	}
	return NULL;  /* value is not a userdata with a metatable */
}

#endif


#endif
