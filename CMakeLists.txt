cmake_minimum_required(VERSION 3.24)
project(lmfilament VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
option(LMFILAMENT_FULL_STATIC "Link the complete executable statically" ON)
# Filament is built by scripts/build-filament.sh with clang + libc++ (install prefix + build tree).
set(LMFILAMENT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/filament-install" CACHE PATH "Filament install prefix")
set(LMFILAMENT_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/build/filament-build" CACHE PATH "Filament build tree (static archives not installed)")
set(LMFILAMENT_FILAMENT_VERSION "1.78.1" CACHE STRING "Pinned Filament version")
set(LMFILAMENT_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/schema/lmfilament.dsl.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LMFILAMENT_SCHEMA}")
file(READ "${LMFILAMENT_SCHEMA}" SCHEMA_HEX HEX)
string(LENGTH "${SCHEMA_HEX}" SCHEMA_HEX_LENGTH)
math(EXPR SCHEMA_LAST_BYTE "${SCHEMA_HEX_LENGTH} - 2")
set(SCHEMA_INITIALIZER "")
foreach(offset RANGE 0 ${SCHEMA_LAST_BYTE} 2)
  string(SUBSTRING "${SCHEMA_HEX}" ${offset} 2 byte)
  string(APPEND SCHEMA_INITIALIZER "0x${byte},")
endforeach()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/schema_embedded.hpp"
  "#pragma once\n#include <cstddef>\nnamespace lmfilament { inline constexpr unsigned char embedded_schema[] = {${SCHEMA_INITIALIZER}}; inline constexpr std::size_t embedded_schema_size = sizeof(embedded_schema); }\n")

function(fil_lib out name)
  find_library(${out} NAMES ${name} PATHS "${LMFILAMENT_BUILD}" PATH_SUFFIXES
    libs/filamat libs/filament-matp libs/filabridge libs/filaflat libs/math libs/utils libs/geometry
    filament filament/backend shaders tools/matc
    third_party/smol-v/tnt third_party/zstd/tnt third_party/meshoptimizer/tnt third_party/mikktspace
    third_party/glslang/tnt/SPIRV third_party/glslang/tnt/glslang third_party/glslang/tnt/glslang/OSDependent/Unix
    third_party/spirv-tools/source third_party/spirv-tools/source/opt third_party/spirv-cross/tnt
    NO_DEFAULT_PATH REQUIRED)
endfunction()
set(_names matlang matp filamat shaders smol-v zstd glslang SPIRV OSDependent SPIRV-Tools-opt SPIRV-Tools
    spirv-cross-glsl spirv-cross-msl spirv-cross-core filament backend filabridge filaflat geometry meshoptimizer mikktspace math utils)
set(FIL_LIBS "")
foreach(n ${_names})
  string(MAKE_C_IDENTIFIER "FILLIB_${n}" var)
  fil_lib(${var} ${n})
  list(APPEND FIL_LIBS "${${var}}")
endforeach()

add_executable(lmfilament cpp/main.cpp cpp/mesh.cpp)
target_compile_definitions(lmfilament PRIVATE LMFILAMENT_VERSION="${PROJECT_VERSION}" LMFILAMENT_FILAMENT_VERSION="${LMFILAMENT_FILAMENT_VERSION}")
target_include_directories(lmfilament PRIVATE cpp "${CMAKE_CURRENT_BINARY_DIR}/generated")
# The install prefix omits headers (utils/JobSystem.h, filament-matp, ...): use the pinned source tree.
set(FIL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/filament")
target_include_directories(lmfilament SYSTEM PRIVATE
  "${FIL_SRC}/filament/include" "${LMFILAMENT_BUILD}/filament" "${FIL_SRC}/filament/backend/include"
  "${FIL_SRC}/libs/math/include" "${FIL_SRC}/libs/utils/include" "${FIL_SRC}/libs/filabridge/include"
  "${FIL_SRC}/libs/filamat/include" "${FIL_SRC}/libs/filament-matp/include" "${FIL_SRC}/libs/geometry/include"
  "${FIL_SRC}/third_party/robin-map/include" "${LMFILAMENT_BUILD}/libs/filabridge/generated")
target_compile_options(lmfilament PRIVATE -Wall -Wextra)
if(APPLE)
  # ld64 rescans archives; no -static / --gc-sections; libc++ is the system one.
  target_link_libraries(lmfilament PRIVATE ${FIL_LIBS})
  target_link_options(lmfilament PRIVATE -Wl,-dead_strip)
else()
  target_link_libraries(lmfilament PRIVATE -Wl,--start-group ${FIL_LIBS} -Wl,--end-group c++ c++abi pthread dl m)
  if(LMFILAMENT_FULL_STATIC)
    target_link_options(lmfilament PRIVATE -static)
  endif()
  target_link_options(lmfilament PRIVATE -Wl,--gc-sections)
endif()
