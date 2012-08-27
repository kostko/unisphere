
set(unisphere_plexus_src
routing_table.cpp
router.cpp
routed_message.cpp
bootstrap.cpp
rpc_engine.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_plexus_proto_src unisphere_plexus_proto_hdr
routed_message.proto
rpc.proto
core_methods.proto
)

add_library(UnispherePlexus SHARED ${unisphere_plexus_src} ${unisphere_plexus_proto_src})
target_link_libraries(UnispherePlexus UnisphereCore UnisphereIdentity UnisphereInterplex ${unisphere_external_libraries})
