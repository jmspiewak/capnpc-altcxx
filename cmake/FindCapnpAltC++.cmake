# No rights reserved.

if(${CAPNP_ALTCXX_FOUND})
  set(CAPNP_ALTCXX_FOUND on)
endif()

find_path(CAPNP_ALTCXX_INCLUDE_DIR
          NAMES "capnp/altc++/common.h"
          PATH_SUFFIXES "include")

find_program(CAPNP_PLUGIN_ALTCXX capnpc-altc++)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CAPNP_ALTCXX DEFAULT_MSG CAPNP_ALTCXX_INCLUDE_DIR
                                  CAPNP_PLUGIN_ALTCXX)
