#!/bin/sh

set -eu

id="$(genid)"

echo "[TEST] ld-preload: creating window ($id)"
(sleep 1
LD_PRELOAD="$WM_LAUNCH_PRELOAD" WM_LAUNCH_ID="$id" createwindow 2>&1) &

r="$(maprequestwait 2>&1)"
echo "[TEST] ld-preload: map request received ($r)"

[ "$r" = "$id" ] || exit 1
