cmake_minimum_required(VERSION 3.24)

project(broime VERSION 0.1.0 LANGUAGES C CXX)

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
if(NOT TARGET brosearch)
    set(BROSEARCH_DIR "${CMAKE_SOURCE_DIR}/../brosearch" CACHE PATH
        "Path to the standalone brosearch repo")
    set(BROSEARCH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BROSEARCH_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    if(EXISTS "${BROSEARCH_DIR}/CMakeLists.txt")
        message(STATUS "broime: brosearch from ${BROSEARCH_DIR}")
        add_subdirectory("${BROSEARCH_DIR}" "${CMAKE_BINARY_DIR}/brosearch" EXCLUDE_FROM_ALL)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/brosearch/CMakeLists.txt")
        message(STATUS "broime: brosearch from ${CMAKE_SOURCE_DIR}/third_party/brosearch (submodule)")
        add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/brosearch" "${CMAKE_BINARY_DIR}/brosearch"
                         EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR
            "broime needs brosearch: not found at ${BROSEARCH_DIR} or "
            "${CMAKE_SOURCE_DIR}/third_party/brosearch.")
    endif()
endif()

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

if(BROIME_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
