#!/usr/bin/env sh
# prerequisites
# qt6-base-dev qt6-connectivity-dev qt6-serialbus-dev cmake
ARCH=$(arch)
BUILDDIR="$(pwd)/build-linux-$ARCH"
cmake -B "$BUILDDIR" -G "Unix Makefiles" -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6/ -DCMAKE_BUILD_TYPE=Release .

cmake --build "$BUILDDIR" --config "Unix Makefiles" -j 8
