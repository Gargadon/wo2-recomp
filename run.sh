#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
sdk_dir=$(dirname -- "$project_dir")
export LD_LIBRARY_PATH="$sdk_dir/out/linux-amd64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$project_dir/out/build/linux-amd64-release/warriors_orochi_2" \
  --game_data_root "$sdk_dir/Warriors Orochi 2 (USA) (En,Fr,De)" \
  --gpu_plugin xenos "$@"
