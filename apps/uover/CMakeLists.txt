
set(uover_src
main.cpp
)

add_executable(uover ${uover_src})
target_link_libraries(uover UnisphereCore UnisphereIdentity UnisphereInterplex UnispherePlexus)
