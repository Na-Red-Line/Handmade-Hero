@echo off

if exist build\debug rmdir /s /q build\debug
mkdir build\debug

set DBG=
set WARN=
for %%a in (%*) do (
    if "%%a"=="-debug" set DBG=-g -gcodeview -O0

    if "%%a"=="-warn" set WARN=-Weverything -pedantic ^
                               -isystem "D:/Windows Kits/10/Include/10.0.26100.0/um/" ^
                               -isystem "D:/Windows Kits/10/Include/10.0.26100.0/shared/" ^
                               -Wno-c++98-compat ^
															 -Wno-c++98-compat-pedantic ^
															 -Wno-gnu^
                               -Wno-gnu-anonymous-struct ^
                               -Wno-nested-anon-types ^
															 -Wno-gnu-offsetof-extensions ^
															 -Wno-zero-as-null-pointer-constant ^
															 -Wno-cast-qual ^
															 -Wno-cast-align ^
                               -Wno-old-style-cast ^
															 -Wno-cast-function-type-strict ^
															 -Wno-missing-prototypes ^
															 -Wno-global-constructors ^
															 -Wno-unsafe-buffer-usage ^
															 -Wno-unsafe-buffer-usage-in-libc-call ^
															 -Wno-nonportable-system-include-path
)


set CXXFLAGS=-std=gnu++20 -fno-exceptions -fno-rtti

set LIBS=-luser32 -lgdi32 -lXinput

set MACRO=-DHANDMADE_INTERNAL

set XLINKER=-Wl,/SUBSYSTEM:WINDOWS,/OPT:REF,/OPT:ICF

powershell -Command "ls '*.cc' -Recurse | %% { '\"' + ($_.FullName -replace '\\', '/') + '\"' }" > build/debug/sources.rsp

pushd build\debug

clang++ %CXXFLAGS% %MACRO% %LIBS% %XLINKER% @sources.rsp -o HandmadeHero.exe %DBG% %WARN%

popd
