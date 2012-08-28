
set(uembed_src
main.cpp
)

add_executable(uembed ${uembed_src})
target_link_libraries(uembed UnisphereCore UnisphereIdentity UnisphereInterplex UnispherePlexus)
