
set(unisphere_social_src
social_identity.cpp
compact_router.cpp
address.cpp
routing_table.cpp
routed_message.cpp
name_database.cpp
sloppy_group.cpp
rpc_channel.cpp
profiling/message_tracer.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_social_proto_src unisphere_social_proto_hdr
address.proto
messages.proto
core_methods.proto
)

add_library(UnisphereSocial SHARED ${unisphere_social_src} ${unisphere_social_proto_src})
target_link_libraries(UnisphereSocial UnisphereCore UnisphereIdentity UnisphereInterplex UnisphereRpc ${unisphere_external_libraries})
