project(capnpc-altc++)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# ========================================================================================

find_package(Capnp REQUIRED)
include_directories(${CAPNP_INCLUDE_DIR})

set(SOURCES
    src/capnpc-altc++.c++
)

add_executable(capnpc-altc++ ${SOURCES})
target_link_libraries(capnpc-altc++ ${CAPNP_LIBRARIES})

# ========================================================================================

file(GLOB_RECURSE INCLUDES "include/*")
add_custom_target(dummy-target SOURCES ${INCLUDES})

install(TARGETS capnpc-altc++ RUNTIME DESTINATION bin)
install(FILES ${INCLUDES} DESTINATION "include/capnp/altc++")