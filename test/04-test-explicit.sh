#!/bin/sh

set -eu

id="$(genid)"

echo "[TEST] explicit: starting factory"
(sleep 1
LD_PRELOAD="$WM_LAUNCH_PRELOAD" WM_LAUNCH_FACTORY=explicit factory explicit 2>&1) &

echo "[TEST] explicit: creating window ($id)"
(sleep 1; ../wm-launch -f explicit "$id" factory explicit 2>&1) &

r="$(maprequestwait 2>&1)"
echo "[TEST] explicit: map request received ($r)"

[ "$r" = "$id" ] || exit 1
