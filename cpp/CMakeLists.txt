cmake_minimum_required(VERSION 3.15)
project(ShogiAlgo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# cshogi ライブラリ (静的ライブラリとしてビルド)
add_library(cshogi_lib STATIC
    cshogi/src/bitboard.cpp
    cshogi/src/book.cpp
    cshogi/src/common.cpp
    cshogi/src/dfpn.cpp
    cshogi/src/generateMoves.cpp
    cshogi/src/hand.cpp
    cshogi/src/init.cpp
    cshogi/src/mate.cpp
    cshogi/src/move.cpp
    cshogi/src/mt64bit.cpp
    cshogi/src/position.cpp
    cshogi/src/search.cpp
    cshogi/src/square.cpp
    cshogi/src/usi.cpp
)

target_include_directories(cshogi_lib PUBLIC cshogi/src)
target_compile_definitions(cshogi_lib PRIVATE NDEBUG)

if(MSVC)
    target_compile_options(cshogi_lib PRIVATE /O2 /W3)
else()
    target_compile_options(cshogi_lib PRIVATE -O3 -Wall)
endif()

# ShogiAlgo エンジン本体
add_executable(ShogiAlgo main.cpp)
target_link_libraries(ShogiAlgo PRIVATE cshogi_lib)
target_include_directories(ShogiAlgo PRIVATE cshogi/src)
target_compile_definitions(ShogiAlgo PRIVATE NDEBUG)

if(MSVC)
    target_compile_options(ShogiAlgo PRIVATE /O2 /W3)
else()
    target_compile_options(ShogiAlgo PRIVATE -O3 -Wall)
endif()
