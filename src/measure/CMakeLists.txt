
set(unisphere_measure_src
measure.cpp
)

add_library(UnisphereMeasure SHARED ${unisphere_measure_src})
target_link_libraries(UnisphereMeasure UnisphereCore ${unisphere_external_libraries})

