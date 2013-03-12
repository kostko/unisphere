
set(unisphere_testbed_src
nodes.cpp
scenario.cpp
test_case.cpp
test_bed.cpp
)

add_library(UnisphereTestbed SHARED ${unisphere_testbed_src})
target_link_libraries(UnisphereTestbed UnisphereCore UnisphereIdentity UnisphereInterplex UnisphereSocial ${unisphere_external_libraries})
