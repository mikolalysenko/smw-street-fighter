#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
if [ ! -x build/world-warrior-dojo ]; then
  echo 'Run make dojo first.' >&2
  exit 1
fi
exec ./build/world-warrior-dojo "$@"
