/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdlib.h>
#include "luax.h"
#include "fs.h"


static void checkError(lua_State *L, int err, const char *str) {
  if (!err) return;
  if (err == FS_ENOWRITEPATH || !str) {
    luaL_error(L, "%s", fs_errorStr(err));
  }
  luaL_error(L, "%s '%s'", fs_errorStr(err), str);
}


static int l_fs_mount(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int res = fs_mount(path);
  if (res != FS_ESUCCESS) {
    lua_pushnil(L);
    lua_pushfstring(L, "%s '%s'", fs_errorStr(res), path);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


static int l_fs_unmount(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  fs_unmount(path);
  return 0;
}


static int l_fs_setWritePath(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int res = fs_setWritePath(path);
  checkError(L, res, path);
  return 0;
}


static int l_fs_exists(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  lua_pushboolean(L, fs_exists(filename));
  return 1;
}


static int l_fs_getSize(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  size_t sz;
  int res = fs_size(filename, &sz);
  checkError(L, res, filename);
  lua_pushnumber(L, sz);
  return 1;
}


static int l_fs_getModified(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  unsigned t;
  int res = fs_modified(filename, &t);
  checkError(L, res, filename);
  lua_pushnumber(L, t);
  return 1;
}


static int l_fs_read(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  size_t len;
  char *data = fs_read(filename, &len);
  if (!data) {
    luaL_error(L, "could not read file '%s'", filename);
  }
  lua_pushlstring(L, data, len);
  free(data);
  return 1;
}


static int l_fs_isDir(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  lua_pushboolean(L, fs_isDir(filename));
  return 1;
}


static int l_fs_listDir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  fs_FileListNode *list = fs_listDir(path);
  lua_newtable(L);
  int i = 1;
  fs_FileListNode *n = list;
  while (n) {
    lua_pushstring(L, n->name);
    lua_rawseti(L, -2, i);
    i++;
    n = n->next;
  }
  fs_freeFileList(list);
  return 1;
}


static int l_fs_write(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  size_t len;
  const char *data = luaL_checklstring(L, 2, &len);
  int res = fs_write(filename, data, len);
  checkError(L, res, filename);
  return 0;
}


static int l_fs_append(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  size_t len;
  const char *data = luaL_checklstring(L, 2, &len);
  int res = fs_append(filename, data, len);
  checkError(L, res, filename);
  return 0;
}


static int l_fs_delete(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  int res = fs_delete(filename);
  if (res != FS_ESUCCESS) {
    lua_pushnil(L);
    lua_pushfstring(L, "%s", fs_errorStr(res));
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}


static int l_fs_makeDirs(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  int res = fs_makeDirs(path);
  if (res != FS_ESUCCESS) {
    luaL_error(L, "%s '%s'", fs_errorStr(res), path);
  }
  return 0;
}


int luaopen_fs(lua_State *L) {
  luaL_Reg reg[] = {
    { "mount",        l_fs_mount        },
    { "unmount",      l_fs_unmount      },
    { "setWritePath", l_fs_setWritePath },
    { "exists",       l_fs_exists       },
    { "getSize",      l_fs_getSize      },
    { "getModified",  l_fs_getModified  },
    { "read",         l_fs_read         },
    { "isDir",        l_fs_isDir        },
    { "listDir",      l_fs_listDir      },
    { "write",        l_fs_write        },
    { "append",       l_fs_append       },
    { "delete",       l_fs_delete       },
    { "makeDirs",     l_fs_makeDirs     },
    { NULL, NULL }
  };
  luaL_newlib(L, reg);
  atexit(fs_deinit);
  return 1;
}
