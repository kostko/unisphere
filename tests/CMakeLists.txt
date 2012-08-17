
add_executable(test_identity identity.cpp)
target_link_libraries(test_identity UnisphereCore UnisphereIdentity)
add_test(test_identity test_identity)

add_executable(test_plexus plexus.cpp)
target_link_libraries(test_plexus UnisphereCore UnisphereIdentity UnispherePlexus)
add_test(test_plexus test_plexus)
