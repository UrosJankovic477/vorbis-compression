cmake_minimum_required(VERSION 3.14...4.0)

project(
    vorbis-compression
    VERSION 1.0
    LANGUAGES CXX C
)

set(INCLUDE_INTTYPES_H 0)
set(INCLUDE_STDINT_H 1)
set(INCLUDE_SYS_TYPES_H 0)

set(SIZE16 int16_t)
set(USIZE16 uint16_t)
set(SIZE32 int32_t)
set(USIZE32 uint32_t)
set(SIZE64 int64_t)
set(USIZE64 uint64_t)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/ogg/config_types.h.in ${CMAKE_SOURCE_DIR}/include/ogg/config_types.h 

)

include(FetchContent)
FetchContent_Declare(
  Ogg
  GIT_REPOSITORY https://github.com/xiph/ogg.git
  GIT_TAG master
)

FetchContent_MakeAvailable(Ogg)

FetchContent_Declare(
  Vorbis
  GIT_REPOSITORY https://github.com/xiph/vorbis.git
  GIT_TAG master
)
  
FetchContent_MakeAvailable(Vorbis)

set(SOUNDIO_BUILD_STATIC ON  CACHE BOOL "" FORCE)
set(SOUNDIO_BUILD_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  soundio
  GIT_REPOSITORY https://github.com/andrewrk/libsoundio.git
  GIT_TAG master
)

FetchContent_MakeAvailable(soundio)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

file(
  GLOB_RECURSE SOURCE_FILES
  ${CMAKE_SOURCE_DIR}/src/*.c
  ${CMAKE_SOURCE_DIR}/src/*.cpp
)

file(
  GLOB_RECURSE HEADER_FILES
  ${CMAKE_SOURCE_DIR}/src/*.h
  ${CMAKE_SOURCE_DIR}/src/*.hpp
  ${CMAKE_SOURCE_DIR}/src/*.inl
)

include_directories(${CMAKE_SOURCE_DIR}/include ${GTK4_INCLUDE_DIRS})
link_directories(${CMAKE_SOURCE_DIR}/src/*)
add_compile_definitions(_USE_MATH_DEFINES)
add_compile_options(-Wall -O3 ${GTK4_CFLAGS_OTHER} -ggdb)
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE soundio ogg vorbis vorbisfile vorbisenc ${GTK4_LIBRARIES})