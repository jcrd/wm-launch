#!/bin/sh

n=0
f=0

export PATH="$PWD/tools:$PATH"
export WM_LAUNCH_PRELOAD=../wm-launch-preload.so
export DEBUG=true

for t in *-test-*.sh; do
    n=$((n+1))
    echo "[TEST] --- $n ---"
    if xvfb-run ./"$t"; then
        echo "[TEST] --- $n PASSED ---"
    else
        echo "[TEST] --- $n FAILED ---"
        f=$((f+1))
    fi
done

echo "[TEST] $((n - f))/$n tests passed"
exit $f
