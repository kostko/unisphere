
set(unisphere_interplex_src
contact.cpp
link_manager.cpp
link.cpp
linklet.cpp
linklet_factory.cpp
ip_linklet.cpp
message.cpp
message_dispatcher.cpp
host_introspector.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_interplex_proto_src unisphere_interplex_proto_hdr
contact.proto
interplex.proto
)

add_library(UnisphereInterplex SHARED ${unisphere_interplex_src} ${unisphere_interplex_proto_src} ${unisphere_interplex_proto_hdr})
target_link_libraries(UnisphereInterplex UnisphereCore UnisphereIdentity ${unisphere_external_libraries})
