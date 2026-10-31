#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_dir"
build_dir=${ROOTDOOM_BUILD_DIR:-}
if [[ -z "$build_dir" ]]; then
    for candidate in build-local build; do
        if [[ -x "$candidate/rootdoom" ]]; then build_dir=$candidate; break; fi
    done
fi
if [[ -z "$build_dir" || ! -x "$build_dir/rootdoom" ]]; then
    printf 'Build ROOTDoom first, or set ROOTDOOM_BUILD_DIR to its build directory.\n' >&2
    exit 1
fi
args=()
if [[ -n ${ROOTDOOM_IWAD:-} ]]; then
    args+=(--iwad "$ROOTDOOM_IWAD")
elif [[ -f .deps/doom-shareware/doom1.wad ]]; then
    args+=(--iwad "$project_dir/.deps/doom-shareware/doom1.wad")
fi
if [[ -n ${ROOTDOOM_ROOT_PREFIX:-} || -x "$project_dir/.deps/root/bin/root" ]]; then
    exec ./scripts/with-local-root.sh "$build_dir/rootdoom" "${args[@]}" "$@"
fi
exec "$build_dir/rootdoom" "${args[@]}" "$@"
