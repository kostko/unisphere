#!/bin/bash -e
PREFIX="$1"

if [ ! -d build ]; then
  echo "ERROR: Missing build directory."
  exit 1
fi

mkdir -p build/{debug,release}

export CMAKE_PREFIX_PATH="${PREFIX}"

cd build/debug
cmake -DCMAKE_BUILD_TYPE=debug ../..
cd ../..

cd build/release
cmake -DCMAKE_BUILD_TYPE=release ../..
cd ../..
