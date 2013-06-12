
set(unisphere_rpc_src
exceptions.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_rpc_proto_src unisphere_rpc_proto_hdr
rpc.proto
)

add_library(UnisphereRpc SHARED ${unisphere_rpc_src} ${unisphere_rpc_proto_src})
target_link_libraries(UnisphereRpc UnisphereCore UnisphereIdentity ${unisphere_external_libraries})
