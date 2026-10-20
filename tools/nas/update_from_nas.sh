#!/bin/bash
# Copy the Oracle GUI from the NAS to this machine (e.g. the W530), only what changed.
#
# Lives in the NAS's simulateur/ folder (published there by tools/nas/push_to_nas.sh)
# and copies from wherever it is run from, so it works whatever the NAS mount point is.
# Copies bin/oracle-gui, assets/ (mirrored: stale files deleted), INSTALL_SDL3.txt, oracle-gui-fr.sh;
# bin/oracle_config.ini only if absent, so local [gui] settings are kept.
#
# Usage: bash /path/to/simulateur/update_from_nas.sh [-n] [DEST]
#        -n = dry run; DEST defaults to /home/partage/oracle

set -euo pipefail

DRY=()
if [[ "${1:-}" == "-n" ]]; then DRY=(--dry-run); shift; fi
DEST="${1:-/home/partage/oracle}"
SRC="$(cd "$(dirname "$0")" && pwd)"

command -v rsync >/dev/null || { echo "error: rsync not installed (sudo apt install rsync)" >&2; exit 1; }
[[ -x "$SRC/bin/oracle-gui" ]] || { echo "error: $SRC/bin/oracle-gui not found" >&2; exit 1; }

# Source is a CIFS mount (perms forced to 0755, coarse mtimes): compare size + mtime
# with a 1 s window.
RSYNC=(rsync -rt --modify-window=1 --itemize-changes "${DRY[@]}")

mkdir -p "$DEST/bin"
"${RSYNC[@]}" "$SRC/bin/oracle-gui" "$DEST/bin/"
"${RSYNC[@]}" --ignore-existing "$SRC/bin/oracle_config.ini" "$DEST/bin/"
"${RSYNC[@]}" --delete "$SRC/assets/" "$DEST/assets/"
"${RSYNC[@]}" "$SRC/INSTALL_SDL3.txt" "$SRC/oracle-gui-fr.sh" "$DEST/"
chmod +x "$DEST/bin/oracle-gui" 2>/dev/null || true

if [[ ${#DRY[@]} -eq 0 ]] && ldd "$DEST/bin/oracle-gui" | grep -q 'not found'; then
  echo "warning: missing SDL3 runtime libraries -- see $DEST/INSTALL_SDL3.txt" >&2
fi
echo "done${DRY:+ (dry run, nothing copied)}. Run: $DEST/bin/oracle-gui --stda.gui"
