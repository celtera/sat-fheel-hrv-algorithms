cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

if(NOT TARGET score_lib_base)
  include(ScoreExternalAddon)
endif()

if(NOT TARGET score_plugin_avnd)
  return()
endif()

project(score_addon_hrv LANGUAGES CXX)

avnd_score_plugin_init(
  BASE_TARGET score_addon_hrv
)

avnd_score_plugin_add(
  BASE_TARGET score_addon_hrv
  SOURCES
    "Hrv/HeartbeatMetrics.hpp"
  TARGET heartbeat_metrics
  MAIN_CLASS HeartbeatMetrics
  NAMESPACE fheel
)

avnd_score_plugin_finalize(
  BASE_TARGET score_addon_hrv
  PLUGIN_VERSION 1
  PLUGIN_UUID "9d9576a9-bbd2-42de-90b0-c6e2c6469f42"
)

target_sources(score_addon_hrv PRIVATE Hrv/armpd.cc)
