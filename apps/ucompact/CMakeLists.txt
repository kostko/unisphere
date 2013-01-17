
set(ucompact_src
main.cpp
)

add_executable(ucompact ${ucompact_src})
target_link_libraries(ucompact UnisphereCore UnisphereIdentity UnisphereInterplex UnisphereSocial)
