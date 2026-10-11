#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ ! -f "$project_dir/build/smw/smw_assets.dat" ]; then
  echo 'First run: make original-assets && make smw' >&2
  exit 1
fi
if [ ! -x "$project_dir/build/smw/smw" ]; then
  echo 'Run make smw first.' >&2
  exit 1
fi
if [ ! -f "$project_dir/build/smw/sf2/sprites.dat" ]; then
  echo 'Original SF2 assets are missing. Run make original-assets, then make smw.' >&2
  exit 1
fi
cd "$project_dir/build/smw"
if [ "${1:-}" = '--fullscreen' ]; then
  shift
  exec ./smw --config tv.ini "$@"
fi
exec ./smw --config smw.ini "$@"
