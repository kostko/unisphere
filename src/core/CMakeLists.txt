
set(unisphere_core_src
context.cpp
program_options.cpp
)

add_library(UnisphereCore SHARED ${unisphere_core_src})
target_link_libraries(UnisphereCore ${unisphere_external_libraries})

