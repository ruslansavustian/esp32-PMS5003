#!/usr/bin/env bash
set -eo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
action="${1:-monitor}"
case "$action" in
  monitor|build|flash) ;;
  *) echo "Unknown action: $action" >&2; exit 1 ;;
esac

# EIM installs an activation script for each SDK version.
activation="${ESP_IDF_ACTIVATE:-$HOME/.espressif/tools/activate_idf_v6.1.sh}"
if [[ ! -f "$activation" ]]; then
  echo "ESP-IDF activation script not found: $activation" >&2
  echo "Install ESP-IDF 6.1 with EIM or set ESP_IDF_ACTIVATE." >&2
  exit 1
fi
# EIM supports printing its environment without activating an interactive shell.
# Parse assignments as data (no eval); preserve the caller's normal PATH.
environment="$(bash "$activation" -e)"
original_path="$PATH"
while IFS='=' read -r key value; do
  case "$key" in
    PATH) export PATH="$value:$original_path" ;;
    SYSTEM_PATH) ;; # Keep the caller's system paths.
    *)
      if [[ "$key" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
        export "$key=$value"
      fi
      ;;
  esac
done <<< "$environment"
cd "$project_dir/firmware"

args=("$action")
if [[ "$action" != build ]]; then
  # Match the CH340 USB bridge already identified on this board.
  # Do not choose arbitrarily if several matching boards are connected.
  port="${ESP_PORT:-}"
  if [[ -z "$port" ]]; then
    port="$(python - <<'PY'
import sys
from serial.tools import list_ports

ports = sorted(p.device for p in list_ports.comports()
               if p.vid == 0x1A86 and p.pid == 0x7523)
if not ports:
    sys.exit('ESP32 CH340 port not found. Connect the board with a USB data cable.')
if len(ports) > 1:
    sys.exit('Several CH340 ports found: ' + ', '.join(ports)
             + '\nChoose one explicitly: ESP_PORT=/dev/cu... make monitor')
print(ports[0])
PY
    )"
  fi
  echo "ESP32 port: $port"
  args=(-p "$port" "$action")
fi

if [[ "${2:-}" == --dry-run ]]; then
  printf 'Working directory: %s\n' "$PWD"
  printf 'Command:'
  printf ' %q' idf.py "${args[@]}"
  printf '\n'
  exit 0
fi

exec "$IDF_PYTHON_ENV_PATH/bin/python" "$IDF_PATH/tools/idf.py" "${args[@]}"
