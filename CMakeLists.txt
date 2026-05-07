cmake_minimum_required(VERSION 3.10)
project(VideoPlayer)

set(CMAKE_C_STANDARD 99)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale)

add_executable(videoplayer videoplayer.c)

target_include_directories(videoplayer PRIVATE ${SDL2_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(videoplayer ${SDL2_LIBRARIES} ${FFMPEG_LIBRARIES} m)