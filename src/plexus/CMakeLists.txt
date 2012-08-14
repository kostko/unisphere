
set(unisphere_plexus_src
routing_table.cpp
)

add_library(UnispherePlexus SHARED ${unisphere_plexus_src})
target_link_libraries(UnispherePlexus UnisphereCore UnisphereIdentity UnisphereInterplex ${unisphere_external_libraries})
