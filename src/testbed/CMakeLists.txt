
set(unisphere_testbed_src
nodes.cpp
scenario.cpp
test_case.cpp
test_bed.cpp
data_collector.cpp
runner.cpp
cluster/node.cpp
cluster/master.cpp
cluster/slave.cpp
)

PROTOBUF_GENERATE_CPP(unisphere_testbed_proto_src unisphere_testbed_proto_hdr
cluster/messages.proto
)

add_library(UnisphereTestbed SHARED ${unisphere_testbed_src} ${unisphere_testbed_proto_src})
target_link_libraries(UnisphereTestbed UnisphereCore UnisphereIdentity UnisphereInterplex UnisphereSocial ${unisphere_external_libraries})
