cmake_minimum_required(VERSION 3.10...3.31)
project(oxide_cheat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(HOOK_DIR ${CMAKE_SOURCE_DIR}/../include/And64InlineHook)

add_library(oxide_cheat SHARED
    src/main.cpp
    ${HOOK_DIR}/And64InlineHook.cpp
)

target_include_directories(oxide_cheat PRIVATE ${HOOK_DIR})
target_link_libraries(oxide_cheat log EGL GLESv3 android)
target_compile_options(oxide_cheat PRIVATE -O2 -fvisibility=hidden)
