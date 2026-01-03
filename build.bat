@echo off

set LIBS=-luser32 -lgdi32 -lXinput

if exist build\debug rmdir /s /q build\debug
mkdir build\debug
pushd build\debug

set SOURCES=
for %%f in (..\..\*.cc) do call set SOURCES=%%SOURCES%% %%f

clang -std=gnu++20 -g -fno-exceptions -o HandmadeHero.exe %SOURCES% %LIBS%

popd
