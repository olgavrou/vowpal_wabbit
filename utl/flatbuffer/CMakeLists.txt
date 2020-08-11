add_executable(to_flatbuff txt_to_flat.cc vw_to_flat.cc vw_to_flat.h)
target_link_libraries(to_flatbuff vw explore)
set_property(TARGET to_flatbuff PROPERTY ENABLE_EXPORTS 1)