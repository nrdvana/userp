#! /bin/sh
Target="$@"

if which opera >/dev/null; then opera "${Target}" & disown
elif which firefox >/dev/null; then firefox "${Target}" & disown
elif which mozilla >/dev/null; then mozilla "${Target}" & disown
else echo " *"; echo " * Open ${Target} with your favorite browser."; echo " *"
fi
