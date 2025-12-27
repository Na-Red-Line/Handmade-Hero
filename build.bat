@echo off

if exist build\debug rmdir /s /q build\debug
mkdir build\debug
pushd build\debug

clang -std=gnu17 -g -o main.exe ../../main.c  -luser32 -lgdi32

popd
