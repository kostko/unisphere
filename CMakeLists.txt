project(unisphere)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(Boost_USE_MULTITHREAD ON)
set(Boost_USE_STATIC_LIBS OFF)

find_package(Threads REQUIRED)
find_package(Boost 1.54.0 COMPONENTS system filesystem thread program_options date_time graph log REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Botan REQUIRED)
find_package(GMP REQUIRED)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fvisibility=hidden -DBOOST_ALL_DYN_LINK -std=c++0x")
set(CMAKE_C_FLAGS_RELEASE "-O3 -fvisibility=hidden -DBOOST_ALL_DYN_LINK")

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -fvisibility=hidden -DBOOST_ALL_DYN_LINK -DUNISPHERE_DEBUG -std=c++0x")
set(CMAKE_C_FLAGS_DEBUG "-g3 -fvisibility=hidden -DBOOST_ALL_DYN_LINK -DUNISPHERE_DEBUG")

include_directories(
  # External includes
  ${Boost_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${BOTAN_INCLUDE_DIR}
  ${GMP_INCLUDE_DIR}
  
  # Internal includes
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(unisphere_external_libraries
  ${CMAKE_THREAD_LIBS_INIT}
  ${PROTOBUF_LIBRARY}
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${BOTAN_LIBRARIES}
  ${GMP_LIBRARIES}
  ${GMPXX_LIBRARIES}
)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(apps)
