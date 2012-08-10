project(unisphere)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(Boost_USE_MULTITHREAD ON)
set(Boost_USE_STATIC_LIBS OFF)

find_package(Threads REQUIRED)
find_package(Boost 1.46.0 COMPONENTS system filesystem signals thread program_options date_time REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Botan REQUIRED)

# Require at least GCC 4.5 due to some C++0x features
execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
if (GCC_VERSION VERSION_LESS 4.5)
  message(SEND_ERROR "The UNISPHERE framework requires at least GCC 4.5 to compile.")
endif()

# Uncomment these lines for optimization
#set(CMAKE_BUILD_TYPE distribution)
#set(CMAKE_CXX_FLAGS_DISTRIBUTION "-O3 -fvisibility=hidden -std=c++0x")
#set(CMAKE_C_FLAGS_DISTRIBUTION "-O3 -fvisibility=hidden")

# Uncomment these lines for full debugging
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -fvisibility=hidden -DUNISPHERE_DEBUG -std=c++0x")
set(CMAKE_C_FLAGS_DEBUG "-g3 -fvisibility=hidden -DUNISPHERE_DEBUG")

#SET(UNISPHERE_TEST_CERT_PATH ${PROJECT_SOURCE_DIR}/certs)
#CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
#               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

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

add_subdirectory(src)
