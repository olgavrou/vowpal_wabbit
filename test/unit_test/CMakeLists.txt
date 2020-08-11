add_executable(vw-unit-test.out
  input_formats_benchmarks.cc
  ../../utl/flatbuffer/vw_to_flat.cc
)

find_package(benchmark REQUIRED)

# Add the include directories from vw target for testing
target_include_directories(vw-unit-test.out PRIVATE $<TARGET_PROPERTY:vw,INCLUDE_DIRECTORIES> ../../utl/flatbuffer)
target_link_libraries(vw-unit-test.out PRIVATE vw allreduce Boost::unit_test_framework Boost::system benchmark::benchmark)
if(NOT DEFINED DO_NOT_BUILD_VW_C_WRAPPER)
  target_sources(vw-unit-test.out PUBLIC vwdll_test.cc)
  target_link_libraries(vw-unit-test.out PRIVATE vw_c_wrapper)

  add_custom_command(TARGET vw-unit-test.out POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:vw_c_wrapper>
      $<TARGET_FILE_DIR:vw-unit-test.out>
  )
endif()

# Communicate that Boost Unit Test is being statically linked
if(STATIC_LINK_VW)
  target_compile_definitions(vw-unit-test.out PRIVATE STATIC_LINK_VW)
endif()

add_test(
  NAME vw_unit_test
  COMMAND ./vw-unit-test.out
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
