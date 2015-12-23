#!/usr/bin/python2.7
import os, sys, shutil, platform, time
import cembed

OUTPUT = "bin/juno"
EMBED_DIR = "src/embed"
TEMPSRC_DIR = ".tempsrc"
COMPILER = "gcc"
INCLUDE = [ TEMPSRC_DIR ]
SOURCE = [
  "src/*.c",
  "src/lib/sera/*.c",
  "src/lib/vec/*.c",
  "src/lib/stb_vorbis.c"
]
FLAGS = [ "-Wall", "-Wextra", "--std=gnu99", "-fno-strict-aliasing" ]
LINK = [ "m" ]
DEFINE = [ ]
EXTRA = ""

if platform.system() == "Windows":
  OUTPUT += ".exe"
  LINK += [ "mingw32", "lua51", "SDLmain", "SDL" ]
  FLAGS += [ "-mwindows" ]

if platform.system() == "Linux":
  LINK += [ "luajit-5.1", "SDLmain", "SDL" ]

if platform.system() == "Darwin":
  LINK += [ "luajit-5.1" ]
  FLAGS += [ "-pagezero_size 10000", "-image_base 100000000" ]
  FLAGS += [ os.popen("sdl-config --cflags").read().strip() ]
  EXTRA += os.popen("sdl-config --libs").read().strip()
  DEFINE += [ "SR_MODE_ARGB" ]


def fmt(fmt, dic):
  for k in dic:
    fmt = fmt.replace("{" + k + "}", str(dic[k]))
  return fmt


def clearup():
  if os.path.exists(TEMPSRC_DIR):
    shutil.rmtree(TEMPSRC_DIR)


def main():
  global FLAGS, SOURCE, LINK

  print "initing..."
  starttime = time.time()

  # Handle args
  build = "debug" if "debug" in sys.argv else "release"
  verbose = "verbose" in sys.argv
  
  # Handle build type
  if build == "debug":
    FLAGS += [ "-g" ]
  else:
    FLAGS += [ "-O3" ]

  # Handle "nojit" option -- compile with normal embedded Lua instead
  if "nojit" in sys.argv:
    LINK = filter(lambda x:"lua" not in x, LINK)
    SOURCE += ["src/lib/lua51/*.c"]

  print "building (" + build + ")..."
  
  # Make sure there arn't any temp files left over from a previous build
  clearup()

  # Create directories
  os.makedirs(TEMPSRC_DIR)
  outdir = os.path.dirname(OUTPUT)
  if not os.path.exists(outdir):
    os.makedirs(outdir)

  # Create embedded-file header files
  for filename in os.listdir(EMBED_DIR):
    fullname = EMBED_DIR + "/" + filename
    res = cembed.process(fullname)
    open(TEMPSRC_DIR + "/" + cembed.safename(fullname) + ".h", "wb").write(res)

  # Build
  cmd = fmt(
    "{compiler} -o {output} {flags} {source} {include} {link} {define} " + 
    "{extra}",
    {
      "compiler"  : COMPILER,
      "output"    : OUTPUT,
      "source"    : " ".join(SOURCE),
      "include"   : " ".join(map(lambda x:"-I" + x, INCLUDE)),
      "link"      : " ".join(map(lambda x:"-l" + x, LINK)),
      "define"    : " ".join(map(lambda x:"-D" + x, DEFINE)),
      "flags"     : " ".join(FLAGS),
      "extra"     : EXTRA,
    })
 
  if verbose:
    print cmd

  print "compiling..."
  res = os.system(cmd)

  if build == "release":
    print "stripping..."
    os.system("strip %s" % OUTPUT)

  print "clearing up..."
  clearup()

  if res == 0:
    print "done (%.2fs)" % (time.time() - starttime)
  else:
    print "done with errors"
  sys.exit(res)


if __name__ == "__main__":
  main() 

