#!/bin/bash
# Get the directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BACKEND_DIR="$DIR/../backend"

if command -v uv >/dev/null 2>&1; then
    (cd "$BACKEND_DIR" && uv run python scripts/run_dev_server.py "$@")
elif [ -d "$BACKEND_DIR/.venv" ]; then
    # Use the python binary inside the virtualenv directly
    "$BACKEND_DIR/.venv/bin/python" "$BACKEND_DIR/scripts/run_dev_server.py" "$@"
else
    # Fallback to system python if venv does not exist
    python3 "$BACKEND_DIR/scripts/run_dev_server.py" "$@"
fi
