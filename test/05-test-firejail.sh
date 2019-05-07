#!/bin/sh

set -eu

if ! command -v firejail > /dev/null; then
    echo "[TEST] firejail: command not found"
    exit 2
fi

id="$(genid)"

echo "[TEST] firejail: launching client in firejail"
(sleep 1; ../wm-launch -j "$id" createwindow 2>&1) &

r="$(maprequestwait 2>&1)"
echo "[TEST] firejail: map request received ($r)"

[ "$r" = "$id" ] || exit 1
