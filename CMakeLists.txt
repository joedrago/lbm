cmake_minimum_required(VERSION 2.6)

project(lbm)

if(UNIX)
    add_definitions(-g)
endif()

if(WIN32)
    add_definitions(/wd4996)
endif()

set(DYN_LIBS_ONLY true)
add_subdirectory(ext/dyn)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/dyn/src)

set(PCRE_MINIMAL_DEFAULT "OFF")
add_subdirectory(ext/pcre)
add_subdirectory(ext/genHeader)
add_subdirectory(ext/lua)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/lua/src)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SOURCES
    src/lbmRenderer.c
    src/lbmRenderer.h
    src/lbmVariant.c
    src/lbmVariant.h
    src/main.c
)
genHeader(SOURCES src/lbmBase.lua ${CMAKE_CURRENT_BINARY_DIR}/lbmBaseLua.h lbmBaseLua)
add_executable(lbm ${SOURCES})
target_link_libraries(lbm dyn lua pcre)

if(WIN32)
    target_link_libraries(lbm opengl32)
endif()
if(UNIX)
    target_link_libraries(lbm m)
endif()
