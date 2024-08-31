cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(MyProcessor)

include(dependencies.cmake)

include_directories(${concurrentqueue_SOURCE_DIR})
include_directories(${MAXSDK_MAX_INCLUDE_DIR})
include_directories(${MAXSDK_MSP_INCLUDE_DIR})

# Create a target for our base library
add_library(Heartbeat STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/../Hrv/HeartbeatMetrics.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../Hrv/HeartbeatMetrics.cpp"
)

target_link_libraries(Heartbeat PUBLIC
    Avendish::Avendish
    Boost::boost
)

avnd_make(
  TARGET Heartbeat
  MAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Hrv/HeartbeatMetrics.hpp"
  MAIN_CLASS fheel::HeartbeatMetrics
  C_NAME heartbeat_metrics
  BACKENDS "dump;max"
)

# Create a Max package
if(MAX_PACKAGE_FOLDER)
  if(WIN32 AND NOT MSVC)
    get_filename_component(cxx_path "${CMAKE_CXX_COMPILER}" PATH)
    set(FILES_TO_INSTALL
          ${FILES_TO_INSTALL}
          "${cxx_path}/libc++.dll"
          "${cxx_path}/libunwind.dll"
    )
  endif()

  avnd_create_max_package(
      CODESIGN
      NOTARIZE

      NAME Heartbeat
      SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/package"
      PACKAGE_ROOT "${MAX_PACKAGE_FOLDER}"

      KEYCHAIN_FILE "$ENV{MAC_CODESIGN_KEYCHAIN}"
      KEYCHAIN_PASSWORD "$ENV{MAC_CODESIGN_KEYCHAIN_PASSWORD}"
      CODESIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/entitlements.plist"
      CODESIGN_IDENTITY "$ENV{MAC_CODESIGN_IDENTITY}"
      NOTARIZE_TEAM "$ENV{MAC_CODESIGN_TEAM}"
      NOTARIZE_EMAIL "$ENV{MAC_CODESIGN_EMAIL}"
      NOTARIZE_PASSWORD "$ENV{MAC_NOTARIZE_PASSWORD}"

      EXTERNALS Heartbeat_max
      SUPPORT "${FILES_TO_INSTALL}"
  )
endif()
