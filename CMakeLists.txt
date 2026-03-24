cmake_minimum_required(VERSION 3.16)
project(Verslang VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform detection
if(WIN32)
    add_definitions(-DVERSLANG_WINDOWS -D_USE_MATH_DEFINES -DNOMINMAX)
elseif(APPLE)
    add_definitions(-DVERSLANG_MACOS)
else()
    add_definitions(-DVERSLANG_LINUX)
endif()

# Source files
set(VERSLANG_SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/codegen/x86_64.cpp
    src/compiler/compiler.cpp
)

# Main executable
add_executable(verslang ${VERSLANG_SOURCES})
target_include_directories(verslang PRIVATE src)

# Install
install(TARGETS verslang DESTINATION bin)
install(DIRECTORY examples/ DESTINATION share/verslang/examples)
install(DIRECTORY stdlib/ DESTINATION share/verslang/stdlib)
