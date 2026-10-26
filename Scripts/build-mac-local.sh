#!/bin/bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
engine_root="${UE_MAC_ENGINE_ROOT:-/Users/Shared/UnrealEngine/Launcher/UE_5.3}"
toolchain_root="${UE_MAC_TOOLCHAIN_ROOT:-/private/tmp/ue53-xcode-shim}"
developer_dir="$toolchain_root/Xcode.app/Contents/Developer"

if [[ ! -x "$toolchain_root/bin/xcode-select" || ! -x "$developer_dir/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++" ]]; then
    printf '%s\n' "The local UE 5.3 compiler setup is missing: $toolchain_root" >&2
    printf '%s\n' 'Set UE_MAC_TOOLCHAIN_ROOT to its new location if it has been moved.' >&2
    exit 1
fi

if [[ ! -f "$engine_root/Engine/Build/BatchFiles/Mac/Build.sh" ]]; then
    printf '%s\n' "Unreal Engine was not found at: $engine_root" >&2
    printf '%s\n' 'Set UE_MAC_ENGINE_ROOT to the engine installation directory.' >&2
    exit 1
fi

exec env PATH="$toolchain_root/bin:$PATH" DEVELOPER_DIR="$developer_dir" \
    bash "$engine_root/Engine/Build/BatchFiles/Mac/Build.sh" \
    mechanic_testingEditor Mac Development \
    "-Project=$project_dir/mechanic_testing.uproject" \
    -WaitMutex -NoHotReloadFromIDE -NoEngineChanges "$@"
