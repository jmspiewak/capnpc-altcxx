include(CapnpCompile)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CAPNP_INCLUDE_DIR} ${CAPNP_ALTCXX_INCLUDE_DIR})

capnp_compile(CAPNP_CXX "addressbook.capnp" PLUGIN ${CAPNP_PLUGIN_ALTCXX})

add_executable(addressbook addressbook.c++ ${CAPNP_CXX})
target_link_libraries(addressbook ${CAPNP_LIBRARIES})

add_custom_target(dummy-target00 SOURCES addressbook.capnp)
