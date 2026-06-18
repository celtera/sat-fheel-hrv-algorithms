cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(score_addon_hrv LANGUAGES CXX)

# Max/MSP externals link the static CRT (/MT) on Windows, as the official
# max-sdk-base requires. Set it for the whole addon -- before Avendish and the
# object library are created -- so every target shares one runtime; mixing /MT
# and /MD trips MSVC with LNK2038. (CMP0091 is NEW via cmake_minimum_required.)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Use the Avendish the host (ossia score) provides; otherwise fetch it.
find_package(Avendish QUIET)
if(NOT Avendish_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    Avendish
    GIT_REPOSITORY "https://github.com/celtera/avendish"
    GIT_TAG 7eafe173)
  FetchContent_Populate(Avendish)
  list(APPEND CMAKE_PREFIX_PATH "${avendish_SOURCE_DIR}")
  find_package(Avendish REQUIRED)
endif()

include(FetchContent)
FetchContent_Declare(
  DataFrame
  GIT_REPOSITORY "https://github.com/hosseinmoein/DataFrame"
  GIT_TAG        master
  GIT_PROGRESS   true)
FetchContent_MakeAvailable(DataFrame)

avnd_addon_init(NAME score_addon_hrv)

# Standalone back-ends compile generated sources (in the build tree) that include
# the object headers by their in-repo path (e.g. <Hrv/HeartbeatMetrics.hpp>); carry
# the addon root so those resolve. Score builds add it via score's own machinery.
add_library(hrv_deps INTERFACE)
target_include_directories(hrv_deps INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

avnd_addon_object(
  BASE score_addon_hrv
  C_NAME heartbeat_metrics
  CLASS HeartbeatMetrics
  NAMESPACE fheel
  DEPENDENCIES DataFrame hrv_deps
  SOURCES
    "Hrv/HeartbeatMetrics.hpp"
    "Hrv/HeartbeatMetrics.cpp")

avnd_addon_finalize(NAME score_addon_hrv UUID 9d9576a9-bbd2-42de-90b0-c6e2c6469f42 VERSION 1)
