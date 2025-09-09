#!/usr/bin/env sh
# prerequisites
# qt6-base-dev qt6-connectivity-dev qt6-serialbus-dev cmake
ARCH=$(arch)
BUILDDIR="$(pwd)/build-linux-$ARCH"
cmake -B "$BUILDDIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILDDIR" --config "Unix Makefiles" -j 8
