#!/usr/bin/env sh
# Builds and runs the Odin bindings' tests or example against libnre_c.
#
#   ./run.sh test            # headless tests (odin test tests)
#   ./run.sh orbit           # builds and runs the orbit example (opens a window)
#   ./run.sh build           # only builds the example, to build/orbit
#
# NRE_LIB_DIR: the directory with libnre_c.so (default: <repo>/cmake-build-debug/bin).
# Odin can't read environment variables in a foreign import, so it goes to the linker as -L and as the rpath.
# Extra arguments are passed on to odin (e.g. -o:speed).
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
lib_dir=${NRE_LIB_DIR:-$repo/cmake-build-debug/bin}
flags="-L$lib_dir -Wl,-rpath,$lib_dir"

what=${1:-}
[ $# -gt 0 ] && shift
mkdir -p "$here/build"
case "$what" in
    test)  exec odin test "$here/tests" -out:"$here/build/nre_tests" -extra-linker-flags:"$flags" -vet -strict-style "$@" ;;
    build) exec odin build "$here/examples/orbit" -out:"$here/build/orbit" -extra-linker-flags:"$flags" -vet -strict-style "$@" ;;
    orbit) exec odin run "$here/examples/orbit" -out:"$here/build/orbit" -extra-linker-flags:"$flags" -vet -strict-style "$@" ;;
    *)     echo "usage: $0 test|build|orbit [odin flags...]" >&2; exit 2 ;;
esac
