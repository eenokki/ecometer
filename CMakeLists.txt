cmake_minimum_required(VERSION 3.20)

# ── Read .env for user-specific paths ───────────────────────────
function(read_env_file)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
        file(READ "${CMAKE_SOURCE_DIR}/.env" ENV_CONTENTS)
        string(REPLACE "\n" ";" ENV_LINES "${ENV_CONTENTS}")
        foreach(LINE ${ENV_LINES})
            string(STRIP "${LINE}" LINE)
            if(LINE MATCHES "^PCM_DIR=(.+)$")
                set(PCM_DIR "${CMAKE_MATCH_1}" CACHE PATH "" FORCE)
                message(STATUS "PCM_DIR from .env: ${PCM_DIR}")
            endif()
        endforeach()
    endif()
endfunction()

read_env_file()

if(NOT DEFINED PCM_DIR OR PCM_DIR STREQUAL "")
    message(FATAL_ERROR "PCM_DIR not set. Add PCM_DIR=path/to/pcm in .env file")
endif()

# ── vcpkg MUST be set before project() ──────────────────────────
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")

project(ecometer)
set(CMAKE_CXX_STANDARD 17)

find_package(CURL          REQUIRED)
find_package(nlohmann_json REQUIRED)

set(BUILD_SHARED_LIBS                  OFF CACHE BOOL "" FORCE)
set(PCM_NO_EXAMPLES                    ON  CACHE BOOL "" FORCE)
set(PCM_NO_TESTS                       ON  CACHE BOOL "" FORCE)
set(NO_STATIC_MSVC_RUNTIME_LIBRARY     ON  CACHE BOOL "" FORCE)
set(PCM_NO_STATIC_MSVC_RUNTIME_LIBRARY ON  CACHE BOOL "" FORCE)

add_subdirectory(${PCM_DIR} ${CMAKE_BINARY_DIR}/pcm EXCLUDE_FROM_ALL)

add_executable(ecometer main.cpp)

target_include_directories(ecometer PRIVATE
    ${PCM_DIR}/src
    ${CMAKE_BINARY_DIR}/pcm/src
)

target_link_libraries(ecometer PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    PCM_STATIC
)

if(WIN32)
    target_link_libraries(ecometer PRIVATE
        pdh
        version
        dbghelp
    )
endif()

add_custom_command(TARGET ecometer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:ecometer>
        $<TARGET_FILE_DIR:ecometer>
)