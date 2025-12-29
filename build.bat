@echo off

if exist build\debug rmdir /s /q build\debug
mkdir build\debug
pushd build\debug

clang -std=gnu++20 -g -o win64HandmadeHero.exe ../../win64HandmadeHero.cc  -luser32 -lgdi32

popd
