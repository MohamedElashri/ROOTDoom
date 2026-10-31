#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$project_dir"
iwad=${ROOTDOOM_IWAD:-"$project_dir/.deps/doom-shareware/doom1.wad"}
exec ./scripts/with-local-root.sh ./build-local/rootdoom --iwad "$iwad" "$@"
