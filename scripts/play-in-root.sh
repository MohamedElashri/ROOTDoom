#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_dir"
if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    printf 'Usage: %s /path/to/doom1.wad (or set ROOTDOOM_IWAD)\n' "$0"
    exit 0
fi
iwad=${1:-${ROOTDOOM_IWAD:-}}
if [[ -z "$iwad" && -f .deps/doom-shareware/doom1.wad ]]; then
    iwad="$project_dir/.deps/doom-shareware/doom1.wad"
fi
if [[ $# -gt 1 || -z "$iwad" ]]; then
    printf 'Usage: %s /path/to/doom1.wad (or set ROOTDOOM_IWAD)\n' "$0" >&2
    exit 2
fi
escaped=${iwad//\\/\\\\}
escaped=${escaped//\"/\\\"}
escaped=${escaped//$'\n'/\\n}
if [[ -n ${ROOTDOOM_ROOT_PREFIX:-} || -x "$project_dir/.deps/root/bin/root" ]]; then
    exec ./scripts/with-local-root.sh root -l "macros/doom.C(\"$escaped\")"
fi
exec root -l "macros/doom.C(\"$escaped\")"
