add_executable(vw-benchmarks.out
  input_formats_benchmarks.cc
  ../../utl/flatbuffer/vw_to_flat.cc
)

find_package(benchmark REQUIRED)

# Add the include directories from vw target for testing
target_include_directories(vw-benchmarks.out PRIVATE $<TARGET_PROPERTY:vw,INCLUDE_DIRECTORIES> ../../utl/flatbuffer)
target_link_libraries(vw-benchmarks.out PRIVATE vw benchmark::benchmark)

# Communicate that Boost Unit Test is being statically linked
if(STATIC_LINK_VW)
  target_compile_definitions(vw-benchmarks.out PRIVATE STATIC_LINK_VW)
endif()

add_test(
  NAME vw_benchmarks
  COMMAND ./vw-benchmarks.out
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
