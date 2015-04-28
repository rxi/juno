/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef BUFFER_H
#define BUFFER_H

#include "luax.h"
#include "lib/sera/sera.h"

#define BUFFER_CLASS_NAME "Buffer"

typedef struct {
  sr_Buffer *buffer;
} Buffer;

Buffer *buffer_new(lua_State *L);

#endif
