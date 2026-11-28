# The game's details for the packaging scripts (sourced), as Toy Commander's
# games.sh (ToyCommander--QilinRecomp, the launcher's master copy) has them.
# Everything game-specific in a download comes from here: the README
# templates' @NAME@ places (fill) and kit/game.txt (write_game_txt), which
# is all the launcher knows about the game (tools/player-kit/launcher/ADOPTING.md).
game=wildhorserescue   # the data folder (qilin/wildhorserescue, the runtime's own)
REPO="Qilin1066/BarbieWildHorseRescue--QilinRecomp"
# TITLE names files (Play <TITLE>.exe, <TITLE>.app), so it has no colon;
# FULL_TITLE is the game's whole name, for the READMEs' prose.
TITLE="Wild Horse Rescue"; FULL_TITLE="Barbie Horse Adventures: Wild Horse Rescue"
PACKAGE="BarbieWildHorseRescue--QilinRecomp"; EXE="WildHorseRescue.exe"; BOOT="default.xbe"
BUILD_TITLE="Barbie Horse Adventures: Wild Horse Rescue (USA)"; RELEASE="the USA release, VU-042"; PRODUCT="VU-042"
STUDIO="Blitz Games / Vivendi Universal Games, Xbox, 2003"
# Fills a template's @NAME@ places with the game's details.
fill() {
    sed -e "s|@GAME@|$game|g" -e "s|@FULL_TITLE@|$FULL_TITLE|g" -e "s|@TITLE@|$TITLE|g" -e "s|@PACKAGE@|$PACKAGE|g" \
        -e "s|@EXE@|$EXE|g" -e "s|@BOOT@|$BOOT|g" -e "s|@BUILD_TITLE@|$BUILD_TITLE|g" -e "s|@RELEASE@|$RELEASE|g" \
        -e "s|@STUDIO@|$STUDIO|g" -e "s|@REPO@|$REPO|g" "$1"
}
# Writes kit/game.txt, one name=value a line:
#   write_game_txt <file> <exe> <compiler> [<compiler_args>]
# with Windows line ends where GAME_TXT_CRLF=1. Every key is written, the
# launcher's defaults too, so the file says the whole story: the game's
# names; the data it builds from (an extracted disc, a folder: the runtime
# reads the disc's files from it while the game runs; `build` is
# recomp/src/identify.hpp's supported build, which `qilin identify --build`
# checks); how it is recompiled (one step: qilin lift of the folder, which
# finds its default.xbe whatever its case); what one compile needs at its
# peak (the biggest lifted file, 4.3 MB, took about 1160 MB with
# zig's Clang at -O2: 1300 leaves some room); how the game starts (the
# disc's folder, as qilin_run takes it); where releases are published (tags
# vX.Y.Z); and what the Windows downloads before the launcher (0.13 to
# 0.16: Play Wild Horse Rescue.bat running kit\play.ps1 and Ninja, the game
# and its logs beside them) left in the folder, removed at the launcher's
# start, with the disc folder they remembered taken over (launcher 2's
# old_files and old_input_file). The launcher builds the game in work\, so
# the old top-level WildHorseRescue.exe is one of those leftovers.
write_game_txt() {
    {
        printf 'game=%s\ntitle=%s\nexe=%s\nboot=%s\n' "$game" "$TITLE" "$2" "$BOOT"
        printf 'build=%s\nproduct=%s\nrelease=%s\n' "$BUILD_TITLE" "$PRODUCT" "$RELEASE"
        printf 'package=%s\nrepo=%s\nrelease_tag_prefix=v\nrelease_asset_prefix=%s-\n' "$PACKAGE" "$REPO" "$PACKAGE"
        printf 'input=folder\ninput_noun=disc folder\n'
        printf 'input_hint=Choose the folder you extracted your disc into (with extract-xiso or xdvdfs, for example): the one holding default.xbe and the Packages and Video folders.\n'
        printf 'identify={qilin} identify {input} --json --build {game:build}\n'
        printf 'step.1={qilin} lift {input} --out {gen}\n'
        printf 'compile_memory_mb=1300\nbuild_space_mb=2048\ncache_mb=2048\n'
        printf 'run_args={input}\n'
        printf 'old_files=Play Wild Horse Rescue.bat; WildHorseRescue.exe; WildHorseRescue.exe.previous; WildHorseRescue.exe.new; run.log; run-errors.log; kit/play.ps1; kit/ninja.exe; kit/kit.ninja; work/build.ninja; work/.ninja_log; work/.ninja_deps; work/built-with.txt\n'
        printf 'old_input_file=work/disc-folder.txt\n'
        printf 'compiler=%s\n' "$3"
        if [ -n "${4:-}" ]; then printf 'compiler_args=%s\n' "$4"; fi
    } | if [ "${GAME_TXT_CRLF:-0}" = 1 ]; then sed 's/$/\r/'; else cat; fi > "$1"
}
# Copies the licences every download shares into <LICENSES folder>, by
# component: this project's (GPL-3.0); SDL3 (fetched by CMake into <build>);
# and the vendored third_party/ code the kit's library and the launcher are
# built from (third_party/README.md): Dear ImGui, Vulkan-Headers, volk,
# Zydis with Zycore (qilin), FidelityFX FSR 1 and SMAA (the renderer's
# passes), nxdk's kernel export list (CC0-1.0) and XbSymbolDatabase (the
# XDK function names the runtime carries).
#   common_licenses <LICENSES folder> <build folder> <repository root>
common_licenses() {
    lic=$1 build=$2 root=$3
    mkdir -p "$lic/$PACKAGE" "$lic/SDL3" "$lic/Dear ImGui" "$lic/Vulkan-Headers" "$lic/volk" "$lic/Zydis" \
             "$lic/FidelityFX FSR" "$lic/SMAA" "$lic/nxdk" "$lic/XbSymbolDatabase"
    cp "$root/LICENSE" "$lic/$PACKAGE/LICENSE.txt"
    sdl=$(sed -n 's/^FETCHCONTENT_SOURCE_DIR_SDL3:PATH=//p' "$build/CMakeCache.txt")
    [ -n "$sdl" ] || sdl="$build/_deps/sdl3-src"
    cp "$sdl/LICENSE.txt" "$lic/SDL3/"
    cp "$root/third_party/imgui/LICENSE.txt" "$lic/Dear ImGui/"
    cp -R "$root/third_party/vulkan-headers/LICENSE.md" "$root/third_party/vulkan-headers/LICENSES" "$lic/Vulkan-Headers/"
    cp "$root/third_party/volk/LICENSE.md" "$lic/volk/"
    cp "$root/third_party/zydis/LICENSE" "$lic/Zydis/LICENSE.txt"
    cp "$root/third_party/zydis/dependencies/zycore/LICENSE" "$lic/Zydis/Zycore-LICENSE.txt"
    cp "$root/third_party/fsr1/LICENSE.txt" "$lic/FidelityFX FSR/"
    cp "$root/third_party/smaa/LICENSE.txt" "$lic/SMAA/"
    cp "$root/third_party/xbsymboldatabase/LICENSE" "$lic/XbSymbolDatabase/LICENSE.txt"
    cat > "$lic/nxdk/README.txt" <<'NXDK'
The runtime's Xbox kernel export list (names, ordinals and argument sizes)
was made from nxdk's lib/xboxkrnl/xboxkrnl.exe.def
(https://github.com/XboxDev/nxdk, commit
fafbe41b48fa00a31b79dd49ad1f86a30adcdd9e), whose header says:

    SPDX-License-Identifier: CC0-1.0
    SPDX-FileCopyrightText: 2017 Stefan Schmidt

CC0 1.0 Universal: https://creativecommons.org/publicdomain/zero/1.0/
NXDK
    cat > "$lic/XbSymbolDatabase/README.txt" <<'XBSDB'
The runtime carries the addresses and names of the XDK library functions in
the game's default.xbe (recomp/runtime/host/xdk_symbols.inc), found once
with XbSymbolDatabase (https://github.com/Cxbx-Reloaded/XbSymbolDatabase,
commit 20eced544726f5558c5a408458f38a086cc4e543), under the MIT licence:
LICENSE.txt. None of its code is included.
XBSDB
}
