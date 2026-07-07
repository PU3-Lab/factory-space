#!/bin/bash
# Get the directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if command -v uv >/dev/null 2>&1; then
    (cd "$DIR" && uv run python scripts/run_dev_server.py "$@")
elif [ -d "$DIR/.venv" ]; then
    "$DIR/.venv/bin/python" "$DIR/scripts/run_dev_server.py" "$@"
else
    python3 "$DIR/scripts/run_dev_server.py" "$@"
fi
