@echo off
pushd %~pd0
mkdir build 2> nul

:: Generate the shaders
::sokol\shdc\win32\sokol-shdc.exe --input shaders.glsl --output build\shaders.glsl.h --slang hlsl5
sokol\shdc\win32\sokol-shdc.exe --input shaders.glsl --output build\shaders.glsl.h --slang glsl330

:: Compile the game
pushd build
cl /nologo /Z7 /FC ..\main.c /link user32.lib gdi32.lib
popd

popd
