#!/bin/bash
# Launch the Oracle GUI in French. Meant to be linked to from a desktop: it resolves
# its own real location (through symlinks), finds the folder holding bin/oracle-gui
# (its own folder when deployed, e.g. /home/partage/oracle; the repo root when run
# from tools/nas/), cd's there and starts the game.

set -euo pipefail

here="$(dirname "$(readlink -f "$0")")"
for dir in "$here" "$here/.." "$here/../.."; do
  if [[ -x "$dir/bin/oracle-gui" ]]; then
    cd "$dir"
    exec ./bin/oracle-gui -g -u=fr
  fi
done
echo "error: bin/oracle-gui not found near $here" >&2
exit 1
