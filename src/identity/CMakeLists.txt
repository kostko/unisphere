
set(unisphere_identity_src
node_identifier.cpp
peer_key.cpp
sign_key.cpp
)

add_library(UnisphereIdentity SHARED ${unisphere_identity_src})
target_link_libraries(UnisphereIdentity UnisphereCore ${unisphere_external_libraries})
