#!/bin/sh

set -eu

id="$(genid)"

echo "[TEST] wm-launch: launching window ($id)"
(sleep 1; wm-launch "$id" createwindow 2>&1) &

r="$(maprequestwait 2>&1)"
echo "[TEST] wm-launch: map request received ($r)"

[ "$r" = "$id" ] || exit 1
