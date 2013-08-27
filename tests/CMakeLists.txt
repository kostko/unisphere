
add_executable(test_core core.cpp)
target_link_libraries(test_core UnisphereCore)
add_test(test_core test_core)

add_executable(test_identity identity.cpp)
target_link_libraries(test_identity UnisphereCore UnisphereIdentity)
add_test(test_identity test_identity)

add_executable(test_social social.cpp)
target_link_libraries(test_social UnisphereCore UnisphereIdentity UnisphereInterplex UnisphereSocial)
add_test(test_social test_social)
