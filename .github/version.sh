#!/usr/bin/env bash
# Reads the release version out of ada83.c, which is the single source of
# truth (ADA83_VERSION_MAJOR / _MINOR). Versioning is classic two-part —
# 0.9, then 1.0, 1.1 — not semantic versioning, so there is no patch field.
# Used by the release workflow's tag check. Run it by hand to see what the
# tree currently claims.
#
#   ./.github/version.sh          -> 0.9
#   ./.github/version.sh number   -> 9     (comparable integer)

set -euo pipefail

source_file="$(dirname "$0")/../ada83.c"

read_version_field() {
  local field_name="$1" value
  value=$(sed -n "s/^#define ADA83_VERSION_${field_name}[[:space:]]\{1,\}\([0-9]\{1,\}\).*/\1/p" \
          "$source_file" | head -1)
  if [[ -z $value ]]; then
    echo "error: ADA83_VERSION_${field_name} not found in ada83.c" >&2
    exit 1
  fi
  printf '%s' "$value"
}

major=$(read_version_field MAJOR)
minor=$(read_version_field MINOR)

case "${1:-string}" in
  string) printf '%d.%d\n' "$major" "$minor" ;;
  number) printf '%d\n' $((major * 100 + minor)) ;;
  *) echo "usage: version.sh [string|number]" >&2; exit 2 ;;
esac
