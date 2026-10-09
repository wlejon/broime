cmake_minimum_required(VERSION 3.24)

project(broime VERSION 0.1.0 LANGUAGES C CXX)

# Dependencies (cmake/bro_deps.cmake): an outer project's target wins (under bro
# these are all bro's), else the ../<name> working tree beside the top-level
# project, else the pinned commit, fetched at configure.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)

if(MSVC)
    # The DLL CRT, so the tests share one heap with bronze_runtime_shared.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_broime_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_broime_top_level ON)
endif()

option(BROIME_BUILD_TESTS "Build broime tests" ${_broime_top_level})

if(_broime_top_level AND CMAKE_GENERATOR MATCHES "Ninja")
    if(NOT DEFINED BRO_COMPILE_JOBS)
        set(BRO_COMPILE_JOBS 4)
    endif()
    if(NOT DEFINED BRO_LINK_JOBS)
        set(BRO_LINK_JOBS 2)
    endif()
    set_property(GLOBAL PROPERTY JOB_POOLS
        compile_pool=${BRO_COMPILE_JOBS}
        link_pool=${BRO_LINK_JOBS})
    set(CMAKE_JOB_POOL_COMPILE compile_pool)
    set(CMAKE_JOB_POOL_LINK link_pool)
endif()

function(broime_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---- brosearch -----------------------------------------------------------------
bro_dependency(brosearch GITHUB wlejon/brosearch
               OPTIONS BROSEARCH_BUILD_TESTS=OFF BROSEARCH_BUILD_TOOLS=OFF)

set(BROIME_SOURCES
    src/types.cpp
    src/compose_table.cpp
    src/candidate_model.cpp
    src/dictionary_trie.cpp
    src/ime_engine.cpp
)

add_library(broime STATIC ${BROIME_SOURCES})
add_library(broime::broime ALIAS broime)

target_include_directories(broime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

broime_warnings(broime)
target_link_libraries(broime PUBLIC brosearch)

# --- Standalone Bronze JavaScript API ---
option(BROIME_ENABLE_API "Build Bronze JavaScript API" ${PROJECT_IS_TOP_LEVEL})
if(BROIME_ENABLE_API)
    # bronze, the JavaScript runtime the api binds (it brings its code
    # generator, brass).
    bro_dependency(bronze GITHUB wlejon/bronze TARGET bronze_runtime_shared
                   OPTIONS BRONZE_BUILD_SHARED_RUNTIME=ON BRONZE_BUILD_TESTS=OFF)

    add_subdirectory(src/api)
endif()

if(BROIME_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
