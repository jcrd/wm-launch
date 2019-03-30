#!/bin/sh

CMD="xterm"
ID="$(date +%N)"

echo "[TEST] launching $CMD ($ID)"
(sleep 1; LD_PRELOAD=../wm-launch-preload.so WM_LAUNCH_ID=$ID $CMD > "$ID".log 2>&1) &

r="$(./maprequestwait 2>&1)"
echo "[TEST] map request received ($r)"

if [ "$r" = "$ID" ]; then
    echo "[TEST] SUCCESS"
else
    echo "[TEST] FAILURE"
    echo "[TEST] $CMD output:"
    cat "$ID".log
    exit 1
fi
