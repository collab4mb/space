#!/bin/sh

if [ ! -d "build" ]; then
  mkdir build
fi

cd build

../sokol/shdc/linux/sokol-shdc --input ../shaders.glsl --output shaders.glsl.h --slang glsl310

gcc -g ../main.c -lX11 -lXi -lXcursor -lGL -lasound -ldl -lm
