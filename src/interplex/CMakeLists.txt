
set(unisphere_interplex_src
contact.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_interplex_proto_src unisphere_interplex_proto_hdr
contact.proto
)

add_library(UnisphereInterplex SHARED ${unisphere_interplex_src} ${unisphere_interplex_proto_src} ${unisphere_interplex_proto_hdr})
target_link_libraries(UnisphereInterplex UnisphereCore UnisphereIdentity ${unisphere_external_libraries})
