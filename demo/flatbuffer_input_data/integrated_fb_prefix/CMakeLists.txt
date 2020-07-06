cmake_minimum_required(VERSION 3.5)

if (NOT DEFINED ENV{FB_EXAMPLE_SCHEMA})
  message(FATAL_ERROR "FB_EXAMPLE_SCHEMA environmental variable isn't set")
endif()

set(schema_generated_dir ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(all_generated_files "")
foreach(schema $ENV{FB_EXAMPLE_SCHEMA})
    get_filename_component(filename ${schema} NAME_WE)
    set(generated_include ${schema_generated_dir}/${filename}_generated.h)
    add_custom_command(
      OUTPUT ${generated_include}
      COMMAND flatc
      -o ${schema_generated_dir}
      -c ${schema}
      DEPENDS ${schema})
    list(APPEND all_generated_files ${generated_include})

    set(python_schema ${schema_generated_dir}/VW)
    add_custom_command(
      OUTPUT ${python_schema}
      COMMAND flatc
      -o ${schema_generated_dir}
      --python ${schema}
      DEPENDS ${schema})
    list(APPEND all_generated_files ${python_schema})
endforeach()

add_custom_target(fbschemas DEPENDS ${all_generated_files})

add_executable(create-fb-data.out
  create_fb_data.cc
)

add_executable(read-fb-data.out
  read_fb_data.cc
)

add_dependencies(create-fb-data.out fbschemas)
add_dependencies(read-fb-data.out fbschemas)
