#!/bin/sh

n=0
s=0
f=0

export PATH="$PWD/tools:$PATH"
export WM_LAUNCH_PRELOAD=../wm-launch-preload.so
export DEBUG=true

for t in *-test-*.sh; do
    n=$((n+1))
    echo "[TEST] --- $n ---"
    if xvfb-run ./"$t"; then
        echo "[TEST] --- $n PASSED ---"
    elif [ $? -eq 2 ]; then
        echo "[TEST] --- $n SKIPPED ---"
        s=$((s+1))
    else
        echo "[TEST] --- $n FAILED ---"
        f=$((f+1))
    fi
done

echo "[TEST] $((n - f - s))/$n passed, $f failed, $s skipped"
exit $f