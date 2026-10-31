#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Relocate module paths in CERN's ROOT 6.40.04 Ubuntu 24.04/GCC 13.3 binary.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
root_prefix="$project_dir/.deps/root"
if [[ ! -x "$root_prefix/bin/root" || "$("$root_prefix/bin/root-config" --version)" != 6.40.04 ]]; then
    printf 'First unpack CERN ROOT 6.40.04 for Ubuntu 24.04/GCC 13.3 into %s.\n' "$root_prefix" >&2
    exit 1
fi
python3 - "$root_prefix" <<'PY'
import hashlib
from pathlib import Path
import sys

prefix = Path(sys.argv[1]).resolve()
old = b'/github/home/ROOT-CI/build'
alias = Path('/tmp/rd-' + hashlib.sha256(str(prefix).encode()).hexdigest()[:8] + '-64004')
if alias.is_symlink():
    if alias.resolve() != prefix:
        raise SystemExit(f'Refusing to replace an alias pointing elsewhere: {alias}')
elif alias.exists():
    raise SystemExit(f'Refusing to replace existing path: {alias}')
else:
    alias.symlink_to(prefix, target_is_directory=True)
replacement = str(alias).encode()
if len(replacement) > len(old):
    raise SystemExit('Relocation alias exceeds the original path field width')
replacement += b'/' * (len(old) - len(replacement))

backup = prefix / 'pcm-original'
backup.mkdir(exist_ok=True)
sources = sorted(backup.glob('*.pcm'))
if not sources:
    sources = sorted((prefix / 'lib').glob('*.pcm'))
    if len(sources) != 113 or not any(old in p.read_bytes() for p in sources):
        raise SystemExit('Unexpected PCM layout; expected CERN ROOT 6.40.04 for Ubuntu 24.04/GCC 13.3')
    for source in sources:
        (backup / source.name).write_bytes(source.read_bytes())
    sources = sorted(backup.glob('*.pcm'))
if len(sources) != 113:
    raise SystemExit('Incomplete original PCM backup; expected 113 modules')

changes = 0
for source in sources:
    original = source.read_bytes()
    changes += original.count(old)
    relocated = original.replace(old, replacement)
    if len(original) != len(relocated):
        raise SystemExit(f'Module size changed unexpectedly: {source}')
    (prefix / 'lib' / source.name).write_bytes(relocated)
if not changes:
    raise SystemExit('No expected build paths found in original modules')
(prefix / '.rootdoom-relocated-modules').write_text(str(alias) + '\n')
print(f'Relocated {changes} path references in {len(sources)} PCMs; originals preserved in {backup}')
PY

ROOTDOOM_ROOT_PREFIX="$root_prefix" "$project_dir/scripts/with-local-root.sh" \
    "$project_dir/scripts/check-environment.sh" --root-only
