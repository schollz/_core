#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
preset="${1:?Missing CMake preset}"
jobs="${2:?Missing job count}"
version="${3:?Missing version}"
log_file="${4:?Missing log path}"
build_dir="build/$preset"

mkdir -p -- "$(dirname -- "$log_file")"
if [[ -f "$log_file" ]]; then
    mv -f -- "$log_file" "${log_file%.log}.previous.log"
fi
printf 'Debug log: %s\n' "$log_file"

run_command() {
    printf '\n$'
    printf ' %q' "$@"
    printf '\n'
    "$@"
}

run_session() (
    trap 'result=$?; printf "\n[%s] Session finished, exit code %s\n" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$result"' EXIT
    printf '[%s] Core Sample Manager diagnostic session\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'Log: %s\nWorking directory: %s\nPreset: %s\nVersion: %s\nJobs: %s\n' \
        "$log_file" "$PWD" "$preset" "$version" "$jobs"
    run_command uname -smr
    run_command cmake --version
    run_command cmake --preset "$preset" -S . "-DCORE_MANAGER_VERSION=$version"
    run_command cmake --build "$build_dir" --parallel "$jobs" --verbose

    app="$build_dir/CoreSampleManager_artefacts/Release/Core Sample Manager"
    if [[ "$(uname -s)" == Darwin ]]; then
        app="$app.app/Contents/MacOS/Core Sample Manager"
    fi
    # Run the actual executable so stdout, stderr and its exit status stay here.
    # The runtime diagnostics are also enabled in Release builds.
    export CORE_MANAGER_DEBUG=1
    run_command "$app"
)

# pipefail preserves configuration, build and application failures through tee.
run_session 2>&1 | tee "$log_file"
