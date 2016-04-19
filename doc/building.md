# Building Juno

## Requirements
* `gcc` or `clang` is used to compile
* `SDL1.2` is linked to dynamically
* `LuaJIT` is linked to dynamically, although this is optional as non-JIT Lua can be used in its place
* `Python2.7` is used by the build script

These dependencies must be met before building.

## Building
Juno can be built on Linux, Windows and OS X. To build you should first clone the repo and cd into it
```bash
git clone https://github.com/rxi/juno.git
cd juno
```

The build script should then be executed:
```bash
./build.py
```

On windows:
```bash
build.py
```

When the build is finished an executable named `juno` (or `juno.exe` on windows) should exist in the `bin/` directory.

You can test it works by executing one of the example projects:
```bash
./bin/juno example/particles
```

On windows:
```bash
bin/juno example/particles
```


## Build options
The following arguments can be passed to the `build.py` script:
* `debug` - compiles unoptimized and doesn't strip debug symbols
* `nojit` - uses embedded Lua instead of linking to LuaJIT
