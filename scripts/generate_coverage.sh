#!/usr/bin/env bash
set -euo pipefail

# Generate an HTML coverage report for the usb-power-osd-tests target.
#
# Usage:
#   scripts/generate_coverage.sh [build-dir] [report-dir]
#
# Defaults:
#   build-dir  = build-coverage
#   report-dir = build-coverage/coverage-html

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Normalize paths so they stay correct after we cd into the build directory.
BUILD_DIR="${1:-build-coverage}"
if [[ ! "${BUILD_DIR}" =~ ^/ ]]; then
    BUILD_DIR="${REPO_ROOT}/${BUILD_DIR}"
fi
REPORT_DIR="${2:-${BUILD_DIR}/coverage-html}"
if [[ ! "${REPORT_DIR}" =~ ^/ ]]; then
    REPORT_DIR="${REPO_ROOT}/${REPORT_DIR}"
fi

cd "${REPO_ROOT}"

# Detect LLVM toolchain prefix. On macOS use xcrun; on Linux assume llvm-
# tools are on PATH.
if [[ "$OSTYPE" == "darwin"* ]]; then
    LLVM_PREFIX=(xcrun)
else
    LLVM_PREFIX=()
fi

PROFRAW="${BUILD_DIR}/default.profraw"
PROFDATA="${BUILD_DIR}/default.profdata"

rm -rf "${BUILD_DIR}" "${REPORT_DIR}"

echo "==> Configuring with coverage instrumentation..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DUSBPOWEROSD_ENABLE_COVERAGE=ON \
    "${REPO_ROOT}"

echo "==> Building tests..."
cmake --build "${BUILD_DIR}" --target usb-power-osd-tests --config Debug -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo "==> Running tests..."
cd "${BUILD_DIR}"
LLVM_PROFILE_FILE="${PROFRAW}" ./usb-power-osd-tests

echo "==> Merging raw profile..."
"${LLVM_PREFIX[@]}" llvm-profdata merge \
    -sparse "${PROFRAW}" \
    -o "${PROFDATA}"

echo "==> Generating HTML report in ${REPORT_DIR}..."
"${LLVM_PREFIX[@]}" llvm-cov show \
    ./usb-power-osd-tests \
    -instr-profile="${PROFDATA}" \
    -format=html \
    -output-dir="${REPORT_DIR}" \
    -show-line-counts \
    -show-instantiations \
    -show-regions \
    -Xdemangler c++filt \
    -Xdemangler -n

echo "==> Summary"
"${LLVM_PREFIX[@]}" llvm-cov report \
    ./usb-power-osd-tests \
    -instr-profile="${PROFDATA}"

echo
echo "Coverage report written to: ${REPORT_DIR}/index.html"
