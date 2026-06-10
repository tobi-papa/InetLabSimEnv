cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0169 OLD)
project(entropy_sandbox LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(FetchContent)

# ---------------------------------------------------------------------------
# Git version baked in at configure time
# ---------------------------------------------------------------------------
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_SHA)
    set(GIT_SHA "unknown")
endif()

execute_process(
    COMMAND git diff --quiet
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_DIRTY_CODE
    ERROR_QUIET
)
if(GIT_DIRTY_CODE EQUAL 0)
    set(GIT_DIRTY_BOOL "false")
else()
    set(GIT_DIRTY_BOOL "true")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

configure_file(
    include/entropy/util/version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/entropy/util/version.hpp
    @ONLY
)

# ---------------------------------------------------------------------------
# Dependencies via FetchContent
# ---------------------------------------------------------------------------
set(FETCHCONTENT_QUIET OFF)

# Eigen — header only, avoid running its CMakeLists (prevents building its tests)
FetchContent_Declare(Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(Eigen3)
if(NOT eigen3_POPULATED)
    FetchContent_Populate(Eigen3)
endif()
add_library(Eigen3_iface INTERFACE)
target_include_directories(Eigen3_iface INTERFACE ${eigen3_SOURCE_DIR})
add_library(Eigen3::Eigen ALIAS Eigen3_iface)

# nlohmann/json
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# toml++
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# CLI11
FetchContent_Declare(CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(CLI11)

# spdlog
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# xxhash — compiled as a tiny static library from a single .c file
FetchContent_Declare(xxhash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG        v0.8.2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(xxhash)
if(NOT xxhash_POPULATED)
    FetchContent_Populate(xxhash)
endif()
add_library(xxhash_lib STATIC ${xxhash_SOURCE_DIR}/xxhash.c)
target_include_directories(xxhash_lib PUBLIC ${xxhash_SOURCE_DIR})
set_target_properties(xxhash_lib PROPERTIES LINKER_LANGUAGE C)

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(entropy_core STATIC
    src/core/graph.cpp
    src/core/graph_cache.cpp
    src/core/algorithm_registry.cpp
    src/core/graph_registry.cpp
    src/core/experiment_runner.cpp
    src/core/parameters.cpp
    src/core/requirements.cpp
    src/core/result.cpp
    src/io/config.cpp
    src/io/result_exporter.cpp
)
target_include_directories(entropy_core PUBLIC
    include
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(entropy_core PUBLIC
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    xxhash_lib
)
apply_compiler_warnings(entropy_core)
apply_sanitizers(entropy_core)

# ---------------------------------------------------------------------------
# Plugins — OBJECT library
#
# IMPORTANT: These are compiled into an OBJECT library (not a static archive).
# Each .cpp registers itself into AlgorithmRegistry / GraphRegistry via a
# static initialiser macro (ENTROPY_REGISTER_ALGORITHM / ENTROPY_REGISTER_LOADER).
#
# Without the $<TARGET_OBJECTS:entropy_plugins> trick below, the linker would
# silently discard every plugin TU that has no direct symbol reference from
# main.cpp, leaving the registry empty at runtime.
#
# To add a new algorithm: add its .cpp here. That is the only existing-file
# change required.
# ---------------------------------------------------------------------------
add_library(entropy_plugins OBJECT
    src/algorithms/structural_entropy.cpp
    src/loaders/edgelist_loader.cpp
)
target_link_libraries(entropy_plugins PUBLIC entropy_core)
apply_compiler_warnings(entropy_plugins)
apply_sanitizers(entropy_plugins)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(entropy-cli apps/cli/main.cpp)
target_link_libraries(entropy-cli PRIVATE
    entropy_core
    CLI11::CLI11
)

# Force all plugin object files into the final binary so static initialisers run.
if(MSVC)
    target_link_options(entropy-cli PRIVATE
        /WHOLEARCHIVE:$<TARGET_FILE:entropy_plugins>)
else()
    target_link_libraries(entropy-cli PRIVATE
        -Wl,--whole-archive
        $<TARGET_OBJECTS:entropy_plugins>
        -Wl,--no-whole-archive
    )
endif()

apply_compiler_warnings(entropy-cli)
apply_sanitizers(entropy-cli)

# ---------------------------------------------------------------------------
# Catch2 v3 (tests)
# ---------------------------------------------------------------------------
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# ---------------------------------------------------------------------------
# premerge kernel (label-keyed; NOT an IEntropyAlgorithm plugin)
# ---------------------------------------------------------------------------
add_library(entropy_premerge STATIC
    src/premerge/h1.cpp
    src/premerge/partition_entropy.cpp
    src/premerge/merge.cpp
    src/premerge/h2_min.cpp
    src/premerge/message.cpp
    src/premerge/bias.cpp
)
target_include_directories(entropy_premerge PUBLIC include ${CMAKE_BINARY_DIR}/generated)
apply_compiler_warnings(entropy_premerge)
apply_sanitizers(entropy_premerge)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
include(Catch)
add_executable(premerge_tests
    tests/premerge/test_smoke.cpp
)
target_link_libraries(premerge_tests PRIVATE entropy_premerge Catch2::Catch2WithMain)
apply_compiler_warnings(premerge_tests)
catch_discover_tests(premerge_tests)
