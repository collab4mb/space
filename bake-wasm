#!/bin/sh

if [ ! -d "build" ]; then
  mkdir build
fi

cd build

../sokol/shdc/linux/sokol-shdc --input ../shaders.glsl --output shaders.glsl.h --slang glsl100

emcc -g --shell-file=../shell.html -s ASYNCIFY=1 -s ALLOW_MEMORY_GROWTH=1 -lc -lm ../main.c -o output.html
