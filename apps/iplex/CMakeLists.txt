
set(iplex_src
main.cpp
)

PROTOBUF_GENERATE_CPP(iplex_proto_src iplex_proto_hdr
hello.proto
)

add_executable(iplex ${iplex_src} ${iplex_proto_src} ${iplex_proto_hdr})
target_link_libraries(iplex UnisphereCore UnisphereIdentity UnisphereInterplex)

