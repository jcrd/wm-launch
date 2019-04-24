#!/bin/sh

set -eu

id1="$(genid)"

echo "[TEST] implicit: launching factory window ($id1)"
(sleep 1; ../wm-launch -f implicit "$id1" factory implicit 2>&1) &

r1="$(maprequestwait 2>&1)"
echo "[TEST] implicit: factory map request received ($r1)"

[ "$r1" = "$id1" ] || exit 1

id2="$(genid)"

echo "[TEST] implicit: launching window ($id2)"
(sleep 1; ../wm-launch -f implicit "$id2" factory implicit 2>&1) &

r2="$(maprequestwait 2>&1)"
echo "[TEST] implicit: map request received ($r2)"

[ "$r2" = "$id2" ] || exit 1
