
set(testbed_src
main.cpp
#tests.cpp
scenarios.cpp
)

add_executable(testbed ${testbed_src})
target_link_libraries(testbed UnisphereTestbed)
