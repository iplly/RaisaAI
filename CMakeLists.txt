cmake_minimum_required(VERSION 3.10)

# ============================ Ускорение сборки ============================

# 1) ccache — кэш компиляции (важно настроить до project()).
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# 2) Ninja вместо Make — быстрее (аналог make -jN), параллелит сам.
if(NOT DEFINED CMAKE_GENERATOR)
    find_program(NINJA_BIN ninja)
    if(NINJA_BIN)
        set(CMAKE_GENERATOR "Ninja" CACHE STRING "" FORCE)
    endif()
endif()

# 3) Быстрый линкер lld (в несколько раз быстрее GNU ld).
find_program(LLD_PROGRAM ld.lld)
if(LLD_PROGRAM)
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld ${CMAKE_EXE_LINKER_FLAGS}")
endif()

# 4) -pipe — не писать временные файлы на диск, компиляция быстрее.
set(CMAKE_CXX_FLAGS "-pipe ${CMAKE_CXX_FLAGS}")

# 5) Явно количество потоков по умолчанию = число ядер.
if(NOT CMAKE_BUILD_PARALLEL_LEVEL)
    cmake_host_system_information(RESULT NCPU QUERY NUMBER_OF_LOGICAL_CORES)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${NCPU})
endif()

# ============================ Проект ============================

project(Raisa)

# Тип сборки по умолчанию
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Тип сборки: Debug | Release" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Флаги для каждого типа сборки
set(CMAKE_CXX_FLAGS_DEBUG
    "-g -O0 -Wall -Wextra" CACHE STRING "Флаги Debug" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -DNDEBUG" CACHE STRING "Флаги Release" FORCE)

find_package(PkgConfig REQUIRED)

# Vosk
find_path(VOSK_INCLUDE_DIR NAMES vosk_api.h)
find_library(VOSK_LIBRARY NAMES vosk libvosk PATHS /usr/lib /usr/local/lib)

# FFmpeg
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavdevice libavformat libavcodec libavutil libswresample)

# libcurl
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

# Автопоиск исходников (main.cpp в корне + всё в src/)
file(GLOB_RECURSE RAISA_SOURCES CONFIGURE_DEPENDS
    main.cpp
    src/*.cpp
)

add_executable(Raisa ${RAISA_SOURCES})

target_include_directories(Raisa PRIVATE
    src
    ${VOSK_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Заголовки установленного FFmpeg не оборачивают объявления в extern "C",
# из-за чего C++ манглирует символы и линковка падает. Оборачиваем их
# принудительно через генерируемый хедер.
set(FFMPEG_C_WRAPPER ${CMAKE_BINARY_DIR}/ffmpeg_c_wrapper.h)
file(WRITE ${FFMPEG_C_WRAPPER}
"#ifdef __cplusplus\nextern \"C\" {\n#endif\n"
"#include <libavdevice/avdevice.h>\n"
"#include <libavformat/avformat.h>\n"
"#include <libavutil/dict.h>\n"
"#include <libavutil/error.h>\n"
"#ifdef __cplusplus\n}\n#endif\n"
)

target_compile_options(Raisa PRIVATE -include ${FFMPEG_C_WRAPPER})

target_link_libraries(Raisa
    PkgConfig::FFMPEG
    PkgConfig::CURL
    ${VOSK_LIBRARY}
)