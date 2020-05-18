#!/bin/sh

set -eu

export WM_LAUNCH_FACTORY="test"

id1="$(genid)"

echo "[TEST] no-factory: launching window with factory in env ($id1)"
(sleep 1; wm-launch "$id1" createwindow 2>&1) &

r1="$(maprequestwait 2>&1)"
echo "[TEST] no-factory: map request received (${r1:-none})"

[ -z "$r1" ] || exit 1

id2="$(genid)"

echo "[TEST] no-factory: launching window with factory nulled ($id2)"
(sleep 1; wm-launch -f "" "$id2" createwindow 2>&1) &

r2="$(maprequestwait 2>&1)"
echo "[TEST] no-factory: map request received ($r2)"

[ "$r2" = "$id2" ] || exit 1
