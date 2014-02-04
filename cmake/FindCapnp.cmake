# No rights reserved.

if(${CAPNP_FOUND})
  set(CAPNP_FIND_QUIETLY on)
endif()

find_path(CAPNP_INCLUDE_DIR
          NAMES "kj/common.h" "capnp/common.h"
          PATH_SUFFIXES "include")

find_library(LIB_KJ kj)
find_library(LIB_KJ_ASYNC kj-async)
find_library(LIB_CAPNP capnp)
find_library(LIB_CAPNPC capnpc)
find_library(LIB_CAPNP_RPC capnp-rpc)

find_program(CAPNP_EXECUTABLE capnp)
find_program(CAPNP_PLUGIN_CXX capnpc-c++)
find_program(CAPNP_PLUGIN_CAPNP capnpc-capnp)

set(CAPNP_LIBRARIES ${LIB_CAPNP} ${LIB_KJ} CACHE STRING "")
set(CAPNP_RPC_LIBRARIES ${LIB_CAPNP_RPC} ${LIB_KJ_ASYNC} CACHE STRING "")
set(CAPNP_COMPILER_LIBRARIES ${LIB_CAPNPC} ${CAPNP_LIBRARIES} CACHE STRING "")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CAPNP DEFAULT_MSG CAPNP_INCLUDE_DIR LIB_KJ LIB_KJ_ASYNC
                                  LIB_CAPNP LIB_CAPNPC LIB_CAPNP_RPC CAPNP_EXECUTABLE
                                  CAPNP_PLUGIN_CXX CAPNP_PLUGIN_CAPNP)

unset(LIB_KJ CACHE)
unset(LIB_KJ_ASYNC CACHE)
unset(LIB_CAPNP CACHE)
unset(LIB_CAPNPC CACHE)
unset(LIB_CAPNP_RPC CACHE)
