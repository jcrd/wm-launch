#!/bin/sh

set -eu

if ! command -v firejail > /dev/null; then
    echo "[TEST] firejail: command not found"
    exit 2
fi

id="$(genid)"

echo "[TEST] firejail: launching client in firejail"
(sleep 1; wm-launch -j "$id" createwindow 2>&1) &

r="$(maprequestwait 2>&1)"
echo "[TEST] firejail: map request received ($r)"

[ "$r" = "$id" ] || exit 1

id1="$(genid)"

echo "[TEST] firejail-implicit: launching factory window ($id1)"
(sleep 1; wm-launch -j -f implicit "$id1" factory implicit 2>&1) &

r1="$(maprequestwait 2>&1)"
echo "[TEST] firejail-implicit: factory map request received ($r1)"

[ "$r1" = "$id1" ] || exit 1

id2="$(genid)"

echo "[TEST] firejail-implicit: launching window ($id2)"
(sleep 1; wm-launch -j -f implicit "$id2" factory implicit 2>&1) &

r2="$(maprequestwait 2>&1)"
echo "[TEST] firejail-implicit: map request received ($r2)"

[ "$r2" = "$id2" ] || exit 1

id3="$(genid)"

echo "[TEST] firejail-explicit: starting factory"
(sleep 1
firejail --env=LD_PRELOAD="$WM_LAUNCH_PRELOAD" \
    --env=WM_LAUNCH_FACTORY=explicit factory explicit 2>&1) &

echo "[TEST] firejail-explicit: creating window ($id3)"
(sleep 1; wm-launch -j -f explicit "$id3" factory explicit 2>&1) &

r3="$(maprequestwait 2>&1)"
echo "[TEST] firejail-explicit: map request received ($r3)"

[ "$r3" = "$id3" ] || exit 1
