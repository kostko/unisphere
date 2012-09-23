
set(unisphere_social_src
social_identity.cpp
compact_router.cpp
routing_table.cpp
)

add_library(UnisphereSocial SHARED ${unisphere_social_src})
target_link_libraries(UnisphereSocial UnisphereCore UnisphereIdentity UnisphereInterplex ${unisphere_external_libraries})
