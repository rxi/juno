#!/usr/bin/python2.7
import os, sys, random, re

def fmt(fmt, dic):
  for k in dic:
    fmt = fmt.replace("{%s}" % k, str(dic[k]))
  return fmt


def makeArray(data):
  i = [0]
  def fn(x):
    x = str(ord(x)) + ","
    if i[0] + len(x) > 78:
      i[0] = len(x)
      x = '\n' + x
    else:
      i[0] += len(x)
    return x
  return '{' + "".join(map(fn, data)).rstrip(",") + '}'


def safename(filename):
  return re.sub("[^a-z0-9]", "_", os.path.basename(filename).lower())


def process(filenames):
  if type(filenames) is str:
    filenames = [filenames]

  strings = []

  for filename in filenames:
    data = open(filename, "rb").read()
    strings.append(
      fmt("/* {filename} */\n" +\
          "static const char {name}[] = \n{array};",
          {
           "filename" : os.path.basename(filename),
           "name"     : safename(filename),
           "array"    : makeArray(data),
          }))

  return "/* Automatically generated; do not edit */\n\n" +\
         "\n\n".join(strings)


def main():
  if len(sys.argv) < 2:
    print "usage: embed FILENAMES"
    sys.exit(1)

  print process(sys.argv[1:])


if __name__ == "__main__":
  main() 
