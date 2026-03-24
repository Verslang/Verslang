@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
cd /d "d:\01. Leonard\Downloads\Verslang"
cl /std:c++20 /EHsc /O2 /I "src" /D_USE_MATH_DEFINES /DNOMINMAX /DVERSLANG_WINDOWS src/main.cpp src/lexer/lexer.cpp src/parser/parser.cpp src/codegen/x86_64.cpp src/compiler/compiler.cpp /Fe:"build/verslang.exe" /Fo:"build/"
