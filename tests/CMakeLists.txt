include(CapnpCompile)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CAPNP_INCLUDE_DIR} ${CAPNP_ALTCXX_INCLUDE_DIR})

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

capnp_compile(CAPNP_CXX "test.capnp" PLUGIN ${CAPNP_PLUGIN_ALTCXX}
              IMPORT ${CAPNP_ALTCXX_INCLUDE_DIR})

set(SOURCES
  ${CAPNP_CXX}
  any-test.c++
  basic-test.c++
  rpc-test.c++
  test-util.c++
)

add_executable(altc++-test ${SOURCES})
target_link_libraries(altc++-test ${CAPNP_RPC_LIBRARIES} ${GTEST_BOTH_LIBRARIES} -lpthread)

enable_testing()
add_test(AltCxxTest altc++-test)

add_custom_target(check ${CMAKE_CTEST_COMMAND} DEPENDS altc++-test)
add_custom_target(dummy-target01 SOURCES test.capnp test-util.h)
