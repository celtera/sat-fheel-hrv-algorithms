cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(score_addon_hrv LANGUAGES CXX)

# Use the Avendish the host (ossia score) provides; otherwise fetch it.
find_package(Avendish QUIET)
if(NOT Avendish_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    Avendish
    GIT_REPOSITORY "https://github.com/celtera/avendish"
    GIT_TAG main)
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

avnd_addon_object(
  BASE score_addon_hrv
  C_NAME heartbeat_metrics
  CLASS HeartbeatMetrics
  NAMESPACE fheel
  DEPENDENCIES DataFrame
  SOURCES
    "Hrv/HeartbeatMetrics.hpp"
    "Hrv/HeartbeatMetrics.cpp")

avnd_addon_finalize(NAME score_addon_hrv UUID 9d9576a9-bbd2-42de-90b0-c6e2c6469f42 VERSION 1)
