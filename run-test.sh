#!/bin/sh

if [ ! -d "build" ]; then
  mkdir build
fi

cd build

../sokol/shdc/linux/sokol-shdc --input ../shaders.glsl --output shaders.glsl.h --slang glsl330
gcc -xc -fsanitize=address -Wall -Wextra -Werror -pedantic -g ../test_main.c -lX11 -lXi -lXcursor -lGL -lasound -ldl -lm -o test
cd ../
./build/test
rm ./build/test
