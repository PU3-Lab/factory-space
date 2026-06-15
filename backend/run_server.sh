#!/bin/bash
# Get the directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -d "$DIR/.venv" ]; then
    "$DIR/.venv/bin/python" "$DIR/scripts/run_server.py" "$@"
else
    python3 "$DIR/scripts/run_server.py" "$@"
fi
