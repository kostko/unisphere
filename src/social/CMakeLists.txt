
set(unisphere_social_src
social_identity.cpp
compact_router.cpp
address.cpp
routing_table.cpp
routed_message.cpp
rpc_engine.cpp
name_database.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_social_proto_src unisphere_social_proto_hdr
messages.proto
rpc.proto
)

add_library(UnisphereSocial SHARED ${unisphere_social_src} ${unisphere_social_proto_src})
target_link_libraries(UnisphereSocial UnisphereCore UnisphereIdentity UnisphereInterplex ${unisphere_external_libraries})
