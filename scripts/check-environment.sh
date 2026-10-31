#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Diagnose ROOT runtime and X11 independently, without changing host settings.
set -euo pipefail

root_only=false
if [[ $# -eq 1 && $1 == --root-only ]]; then
    root_only=true
elif [[ $# -ne 0 ]]; then
    printf 'Usage: %s [--root-only]\n' "$0" >&2
    exit 2
fi
failures=0
log_file=$(mktemp "${TMPDIR:-/tmp}/rootdoom-environment.XXXXXX")
trap 'rm -f -- "$log_file"' EXIT

if ! command -v root-config >/dev/null || ! command -v root >/dev/null; then
    printf 'FAIL: root and root-config must both be on PATH.\n'
    failures=$((failures + 1))
else
    printf 'ROOT executable: %s\n' "$(command -v root)"
    printf 'ROOT version: %s\n' "$(root-config --version)"
    printf 'ROOT prefix: %s\n' "$(root-config --prefix)"
    if timeout 30 root -l -n -b -q -e \
        'if (gSystem->Load("libGui") < 0 || gSystem->Load("libASImage") < 0) { gSystem->Exit(1); } std::cout << "ROOT_STARTUP_OK" << std::endl; 0;' \
        >"$log_file" 2>&1; then
        if grep -qx 'ROOT_STARTUP_OK' "$log_file" &&
            ! grep -Eq 'fatal error:|(^| )error:|^Error in|^Failed to load module|^Module .* not found' "$log_file"; then
            printf 'PASS: ROOT interpreter starts and loads native GUI/image libraries.\n'
        else
            printf 'FAIL: ROOT startup confirmation is missing or diagnostics contain errors.\n'
            cat "$log_file"
            failures=$((failures + 1))
        fi
    else
        result=$?
        printf 'FAIL: ROOT runtime probe exited with status %s.\n' "$result"
        head -n 20 "$log_file"
        failures=$((failures + 1))
    fi
fi

if [[ "$root_only" == true ]]; then
    printf 'Native display check omitted by --root-only.\n'
elif [[ -z ${DISPLAY:-} ]]; then
    printf 'FAIL: DISPLAY is unset; native ROOT needs an X11 display on Linux.\n'
    failures=$((failures + 1))
elif ! command -v xdpyinfo >/dev/null; then
    printf 'FAIL: xdpyinfo is unavailable; install the X11 diagnostic utility.\n'
    failures=$((failures + 1))
elif timeout 10 xdpyinfo >"$log_file" 2>&1; then
    printf 'PASS: X11 display %s is accessible.\n' "$DISPLAY"
else
    printf 'FAIL: Cannot access X11 display %s from this session.\n' "$DISPLAY"
    cat "$log_file"
    failures=$((failures + 1))
fi

if [[ $failures -gt 0 ]]; then
    printf '%s environment check(s) failed. Check the ROOT installation and display access above.\n' "$failures"
    exit 1
fi
printf 'Requested checks passed. Run rootdoom --self-test-gui to verify a real canvas.\n'
