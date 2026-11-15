#!/bin/sh
# Records the catalog preview, asciiverse/aftershock.cast: 100x30, sound off.
# LibreQuake's own attract demo with the status line, the menu, more of the
# demo, quit.
# Runs asciinema in a detached tmux pane; a throwaway state folder keeps
# real settings and saves untouched.
#
#   make && sh asciiverse/record-preview.sh
set -eu
cd "$(dirname "$0")/.."
out=asciiverse/aftershock.cast
tmp=$PWD/build/rec-state
rm -rf "$tmp"; mkdir -p "$tmp"
trap 'tmux kill-session -t as-rec 2>/dev/null || true; rm -rf "$tmp"' EXIT
rm -f "$out"
tmux new-session -d -s as-rec -x 100 -y 30 \
  "env -i PATH=/usr/bin:/bin:/snap/bin HOME=$tmp XDG_STATE_HOME=$tmp USER=player LOGNAME=player \
     TERM=xterm-256color COLORTERM=truecolor LANG=C.UTF-8 \
   asciinema rec --window-size 100x30 --idle-time-limit 2 \
     --capture-env TERM,COLORTERM,LANG --title 'Aftershock' \
     --command '$PWD/build/aftershock --mouse=none --log=$tmp/session.log' $out"
k() { tmux send-keys -t as-rec "$@"; }
hold() { key=$1; n=$2; i=0; while [ $i -lt $n ]; do k "$key"; sleep 0.05; i=$((i+1)); done; }
sleep 30
k Escape; sleep 3
k Down; sleep 0.8; k Down; sleep 0.8; k Up; sleep 0.8; k Up; sleep 1.5
k Escape; sleep 12
k F10; sleep 2
ls -la "$out"
