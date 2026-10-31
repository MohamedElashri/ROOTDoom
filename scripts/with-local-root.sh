#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Run one command with the project-local ROOT, without modifying the host setup.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root_prefix=${ROOTDOOM_ROOT_PREFIX:-"$project_dir/.deps/root"}
if [[ ! -x "$root_prefix/bin/root" || ! -x "$root_prefix/bin/root-config" ]]; then
    printf 'No local ROOT installation at %s. Set ROOTDOOM_ROOT_PREFIX to a ROOT installation.\n' "$root_prefix" >&2
    exit 1
fi
if [[ $# -eq 0 ]]; then
    printf 'Usage: %s COMMAND [ARGUMENTS...]\n' "$0" >&2
    exit 2
fi

export ROOTSYS="$root_prefix"
export PATH="$root_prefix/bin:$PATH"
export LD_LIBRARY_PATH="$root_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export CMAKE_PREFIX_PATH="$root_prefix${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
vdt_prefix=${ROOTDOOM_VDT_PREFIX:-}
if [[ -z "$vdt_prefix" ]]; then
    for candidate in /usr /usr/local /home/linuxbrew/.linuxbrew/opt/vdt; do
        if [[ -f "$candidate/include/vdt/vdtMath.h" ]]; then
            vdt_prefix=$candidate
            break
        fi
    done
fi
if [[ -n "$vdt_prefix" ]]; then
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH:$vdt_prefix"
fi
if [[ -f "$root_prefix/.rootdoom-relocated-modules" ]]; then
    IFS= read -r module_alias < "$root_prefix/.rootdoom-relocated-modules"
    if [[ "$module_alias" != /tmp/rd-????????-64004 ]]; then
        printf 'Invalid local ROOT module alias. Rerun scripts/prepare-local-root.sh.\n' >&2
        exit 1
    fi
    if [[ ! -e "$module_alias" && ! -L "$module_alias" ]]; then
        ln -s -- "$root_prefix" "$module_alias"
    fi
    if [[ ! -L "$module_alias" || "$(readlink -f -- "$module_alias")" != "$(readlink -f -- "$root_prefix")" ]]; then
        printf 'Local ROOT module alias points elsewhere: %s\n' "$module_alias" >&2
        exit 1
    fi
fi
exec "$@"
