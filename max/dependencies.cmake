include(FetchContent)

# Boost
FetchContent_Declare(
  boost
  URL "https://github.com/ossia/sdk/releases/download/sdk30/boost_1_85_0.tar.gz"
)
FetchContent_Populate(boost)
set(BOOST_ROOT "${boost_SOURCE_DIR}")

# Max/MSP sdk
FetchContent_Declare(
  max_sdk
  GIT_REPOSITORY "https://github.com/jcelerier/max-sdk-base"
  GIT_TAG        main
  GIT_PROGRESS   true
)
FetchContent_Populate(max_sdk)

set(AVND_MAXSDK_PATH "${max_sdk_SOURCE_DIR}" CACHE INTERNAL "")

# PureData
if(WIN32)
  FetchContent_Declare(
    puredata
    URL "http://msp.ucsd.edu/Software/pd-0.54-0.msw.zip"
  )
  FetchContent_Populate(puredata)
  set(CMAKE_PREFIX_PATH "${puredata_SOURCE_DIR}/src;${puredata_SOURCE_DIR}/bin;${CMAKE_PREFIX_PATH}")
else()
  FetchContent_Declare(
    puredata
    GIT_REPOSITORY "https://github.com/pure-data/pure-data"
    GIT_TAG        master
    GIT_PROGRESS   true
  )
  FetchContent_Populate(puredata)
  set(CMAKE_PREFIX_PATH "${puredata_SOURCE_DIR}/src;${CMAKE_PREFIX_PATH}")
endif()


# ConcurrentQueue
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
  GIT_TAG        master
  GIT_PROGRESS   true
)
FetchContent_MakeAvailable(concurrentqueue)

# Avendish
if(AVENDISH_EXTERNAL_SOURCE_DIR)
FetchContent_Declare(
    avendish
    DOWNLOAD_COMMAND ""
    SOURCE_DIR "${AVENDISH_EXTERNAL_SOURCE_DIR}"
)
else()
FetchContent_Declare(
  avendish
  GIT_REPOSITORY "https://github.com/celtera/avendish"
  GIT_TAG main
  GIT_PROGRESS true
)
endif()
FetchContent_Populate(avendish)

set(CMAKE_PREFIX_PATH "${avendish_SOURCE_DIR};${CMAKE_PREFIX_PATH}")
find_package(Avendish REQUIRED)
