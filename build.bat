@echo off

if exist build\debug rmdir /s /q build\debug
mkdir build\debug

set DBG=
if "%1"=="debug" set DBG=-g -gcodeview -O0

set CXXFLAGS=-std=gnu++20 -fno-exceptions

set LIBS=-luser32 -lgdi32 -lXinput

powershell -Command "ls '*.cc' -Recurse | %% { '\"' + ($_.FullName -replace '\\', '/') + '\"' }" > build/debug/sources.rsp

pushd build\debug

clang %CXXFLAGS% %DBG% -o HandmadeHero.exe @sources.rsp %LIBS%

popd
