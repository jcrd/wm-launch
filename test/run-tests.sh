#!/bin/sh

n=0
s=0
f=0

export DISPLAY=:99
export PATH="$PWD/..:$PWD/tools:$PATH"
export XDG_RUNTIME_DIR="$PWD/run"
export WM_LAUNCH_PRELOAD=../wm-launch-preload.so
export DEBUG=true

run_test() {
    n=$((n+1))
    echo "[TEST] --- $n ---"
    if xvfb-run ./"$1"; then
        echo "[TEST] --- $n PASSED ---"
    elif [ $? -eq 2 ]; then
        echo "[TEST] --- $n SKIPPED ---"
        s=$((s+1))
    else
        echo "[TEST] --- $n FAILED ---"
        f=$((f+1))
    fi
}

wm-launchd &

if [ $# -eq 0 ]; then
    for t in *-test-*.sh; do
        run_test "$t"
    done
else
    for t in "$@"; do
        run_test "$t"
    done
fi

rm -rf "$XDG_RUNTIME_DIR"
echo "[TEST] $((n - f - s))/$n passed, $f failed, $s skipped"
exit $f
