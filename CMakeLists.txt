cmake_minimum_required(VERSION 3.10)
project(FFmpegPlayer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 pkg-config
find_package(PkgConfig REQUIRED)

# 查找 FFmpeg 库
pkg_check_modules(LIBAV REQUIRED
    libavformat
    libavcodec  
    libavutil
    libswscale
    libswresample
)

# 查找 SDL2
pkg_check_modules(SDL2 REQUIRED sdl2)

# 添加可执行文件（源文件在 src 目录下）
add_executable(myplayer src/videoplayermain.cpp)

# 包含头文件目录
target_include_directories(myplayer PRIVATE
    ${LIBAV_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(myplayer
    ${LIBAV_LIBRARIES}
    ${SDL2_LIBRARIES}
    m
    pthread
)

# 设置编译选项
target_compile_options(myplayer PRIVATE
    ${LIBAV_CFLAGS_OTHER}
    ${SDL2_CFLAGS_OTHER}
    -Wall
    -O2
)

# 输出调试信息
message(STATUS "FFmpeg include dirs: ${LIBAV_INCLUDE_DIRS}")
message(STATUS "FFmpeg libraries: ${LIBAV_LIBRARIES}")
message(STATUS "SDL2 include dirs: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 libraries: ${SDL2_LIBRARIES}")