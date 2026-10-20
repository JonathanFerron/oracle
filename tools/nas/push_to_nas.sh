#!/bin/bash
# Publish the playable GUI build to the NAS (nougatoctet), copying only what changed.
#
# What goes to $DEST (default /mnt/nougatoctet/oracle/simulateur):
#   bin/oracle-gui          -- must be a `make gui-release` build (refused if it has debug info)
#   bin/oracle_config.ini   -- only if absent on the NAS (never overwrites the NAS copy)
#   assets/                 -- runtime files only: every font family / weights file the
#                              source actually references, minus *.md/*.json/*.sh sidecars;
#                              stale files under assets/ on the NAS are deleted
#   INSTALL_SDL3.txt, update_from_nas.sh, oracle-gui-fr.sh  -- from tools/nas/
# Nothing else in $DEST (ressources/, spreadsheets, makeabackup.sh...) is touched.
#
# Usage: tools/nas/push_to_nas.sh [-n] [DEST]     (-n = dry run, show what would change)

set -euo pipefail

DRY=()
if [[ "${1:-}" == "-n" ]]; then DRY=(--dry-run); shift; fi
DEST="${1:-/mnt/nougatoctet/oracle/simulateur}"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO"

# CIFS mount: permissions/owners are forced by the mount, and timestamps are coarse,
# so compare by size + mtime with a 1 s window and don't try to set perms.
RSYNC=(rsync -rt --modify-window=1 --itemize-changes "${DRY[@]}")

[[ -d "$DEST" ]] || { echo "error: $DEST not found (NAS not mounted?)" >&2; exit 1; }
[[ -x bin/oracle-gui ]] || { echo "error: bin/oracle-gui missing -- run 'make gui-release'" >&2; exit 1; }
if file bin/oracle-gui | grep -q 'with debug_info'; then
  echo "error: bin/oracle-gui is a debug build -- run 'make gui-release' first" >&2
  echo "       (then 'make gui-clean' before going back to 'make gui')" >&2
  exit 1
fi

# Runtime asset selection, derived from the source so it follows code changes.
FILTERS=(--filter='- *.md' --filter='- *.json' --filter='- *.sh')
for fam in $(grep -rhoE 'assets/fonts/[A-Za-z0-9_]+/' src | sort -u | cut -d/ -f3); do
  FILTERS+=(--filter="+ /fonts/$fam/***")
done
FILTERS+=(--filter='+ /fonts/' --filter='- /fonts/*')
for w in $(grep -rhoE '"assets/[a-z]+/[A-Za-z0-9_]+\.bin"' src | tr -d '"' | sort -u); do
  dir=$(echo "$w" | cut -d/ -f2)
  FILTERS+=(--filter="+ /$dir/" --filter="+ /$dir/$(basename "$w")" --filter="- /$dir/*")
done

echo "== bin/"
mkdir -p "$DEST/bin"
"${RSYNC[@]}" bin/oracle-gui "$DEST/bin/"
"${RSYNC[@]}" --ignore-existing bin/oracle_config.ini "$DEST/bin/"
echo "== assets/"
"${RSYNC[@]}" --delete --delete-excluded --prune-empty-dirs "${FILTERS[@]}" assets/ "$DEST/assets/"
echo "== docs/scripts"
"${RSYNC[@]}" tools/nas/INSTALL_SDL3.txt tools/nas/update_from_nas.sh tools/nas/oracle-gui-fr.sh "$DEST/"
echo "done${DRY:+ (dry run, nothing copied)}."
