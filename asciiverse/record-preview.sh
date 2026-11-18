#!/bin/sh
# Records the catalog preview, asciiverse/aftershock.cast: 100x30, sound off.
# A fight in the first level of the prequel: walk in, meet what is there,
# rockets, sparks and stone debris, the status line throughout. God mode
# and the rocket launcher are set through AFTERSHOCK_CMD_INGAME, with the
# notify lines off so the console commands do not show.
# Runs asciinema in a detached tmux pane; a throwaway state folder keeps
# real settings and saves untouched. Re-encode afterwards:
#
#   make && sh tools/build-data.sh && sh asciiverse/record-preview.sh
#   python3 asciiverse/recast.py build/raw.cast asciiverse/aftershock.cast 12 8
set -eu
cd "$(dirname "$0")/.."
out=build/raw.cast
tmp=$PWD/build/rec-state
rm -rf "$tmp"; mkdir -p "$tmp"
trap 'tmux kill-session -t as-rec 2>/dev/null || true; rm -rf "$tmp"' EXIT
rm -f "$out"
tmux new-session -d -s as-rec -x 100 -y 30 \
  "env -i PATH=/usr/bin:/bin:/snap/bin HOME=$tmp XDG_STATE_HOME=$tmp USER=player LOGNAME=player \
     TERM=xterm-256color COLORTERM=truecolor LANG=C.UTF-8 \
     AFTERSHOCK_CMD='map lq_e0m1' \
     AFTERSHOCK_CMD_INGAME='con_notifytime 0; god; give 7; give r 60; wait; wait; wait; impulse 7' \
   asciinema rec --window-size 100x30 --idle-time-limit 2 \
     --capture-env TERM,COLORTERM,LANG --title 'Aftershock' \
     --command '$PWD/build/aftershock --no-sound --mouse=none --no-pad --log=$tmp/session.log' $out"
k() { tmux send-keys -t as-rec "$@"; }
hold() { key=$1; n=$2; i=0; while [ $i -lt $n ]; do k "$key"; sleep 0.04; i=$((i+1)); done; }
sleep 6
hold Up 75
i=0
while [ $i -lt 45 ]; do
  k f; sleep 0.15; k Left; sleep 0.05; k Left; sleep 0.35
  i=$((i+1))
done
sleep 1
k F10; sleep 2
grep -a "state:" "$tmp/session.log" | tail -1 | grep -o "kills=[0-9/]*" || true
ls -la "$out"
