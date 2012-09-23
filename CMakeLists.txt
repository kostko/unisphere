project(unisphere)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(USE_CLANG "Use CLang compiler" ON)
option(USE_DEBUG "Compile everything in debug mode" ON)

if(USE_CLANG)
  message(STATUS "UNISPHERE: Using the CLang compiler.")

  set(CMAKE_C_COMPILER "/usr/bin/clang")
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endif(USE_CLANG)

set(Boost_USE_MULTITHREAD ON)
set(Boost_USE_STATIC_LIBS OFF)

find_package(Threads REQUIRED)
find_package(Boost 1.46.0 COMPONENTS system filesystem signals thread program_options date_time REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Botan REQUIRED)

# Uncomment these lines for optimization
#set(CMAKE_BUILD_TYPE distribution)
#set(CMAKE_CXX_FLAGS_DISTRIBUTION "-O3 -fvisibility=hidden -std=c++0x")
#set(CMAKE_C_FLAGS_DISTRIBUTION "-O3 -fvisibility=hidden")

if(USE_DEBUG)
  message(STATUS "UNISPHERE: Building in DEBUG mode.")

  set(CMAKE_BUILD_TYPE debug)
  set(CMAKE_CXX_FLAGS_DEBUG "-g3 -fvisibility=hidden -DUNISPHERE_DEBUG -std=c++0x")
  set(CMAKE_C_FLAGS_DEBUG "-g3 -fvisibility=hidden -DUNISPHERE_DEBUG")
endif(USE_DEBUG)

include_directories(
  # External includes
  ${Boost_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  ${Botan_INCLUDE_DIR}
  
  # Internal includes
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)

set(unisphere_external_libraries
  ${CMAKE_THREAD_LIBS_INIT}
  ${PROTOBUF_LIBRARY}
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${Botan_LIBRARIES}
)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(apps)
