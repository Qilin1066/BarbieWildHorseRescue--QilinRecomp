#!/bin/sh
# Builds the game in one go on Linux or macOS, as ApbArcade-QilinRecomp's
# build script does: the recompiler, the lift of your disc's default.xbe,
# then the game. Run it again after a `git pull`: only what changed is
# rebuilt (the lift leaves unchanged files alone).
#
#   tools/build-wildhorse.sh [disc-folder] [--run]
#
# disc-folder is your extracted disc: the folder holding default.xbe,
# Packages and Video. With none it uses the one it used last
# (build-game/disc-folder.txt). The lifted code goes to build-game/gen,
# which git ignores: it's made from your disc and never leaves your
# computer. Needs CMake 3.25 and GCC 14 or Clang 19 (Xcode 16 on macOS);
# the README lists the Linux packages SDL needs for a window and sound.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/build-game"
disc=""
run=0
for arg in "$@"; do
    case "$arg" in
        --run) run=1 ;;
        -h|--help)
            echo "usage: tools/build-wildhorse.sh [disc-folder] [--run]"
            exit 0 ;;
        *) disc=$arg ;;
    esac
done

if [ -z "$disc" ] && [ -f "$build/disc-folder.txt" ]; then
    disc=$(cat "$build/disc-folder.txt")
fi
if [ -z "$disc" ]; then
    echo "usage: tools/build-wildhorse.sh <disc-folder> [--run]   (your extracted disc: the folder holding default.xbe)" >&2
    exit 2
fi
for needed in default.xbe Packages Video; do
    if [ ! -e "$disc/$needed" ]; then
        echo "$disc has no $needed: extract the whole disc image (for example with extract-xiso or xdvdfs) and give the folder that holds default.xbe" >&2
        exit 1
    fi
done
disc=$(cd "$disc" && pwd)

step() { printf '\n== %s\n' "$1"; }
mkdir -p "$build/gen"
step "Configuring"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DQILIN_GEN_DIR="$build/gen"
printf '%s\n' "$disc" > "$build/disc-folder.txt"
step "Building the recompiler"
cmake --build "$build" --target qilin --parallel
step "Checking default.xbe"
if ! "$build/qilin" info "$disc/default.xbe" --any-build | grep -q "build: the supported build"; then
    echo "$disc/default.xbe isn't the supported build: this port needs Barbie Horse Adventures: Wild Horse Rescue, the USA release" >&2
    exit 1
fi
step "Recompiling default.xbe into C++"
"$build/qilin" lift "$disc/default.xbe" --out "$build/gen"
if [ -s "$build/gen/failures.txt" ]; then
    echo "qilin couldn't recompile all of default.xbe (see $build/gen/failures.txt)" >&2
    exit 1
fi
step "Configuring with the recompiled code"
cmake -S "$root" -B "$build"
step "Building the game (the first time compiles all of it: 10 to 40 minutes)"
cmake --build "$build" --target qilin_run --parallel

printf '\nBuilt: %s\nRun it with: "%s" "%s"\n' "$build/qilin_run" "$build/qilin_run" "$disc"
if [ "$run" = 1 ]; then exec "$build/qilin_run" "$disc"; fi
