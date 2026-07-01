#!/bin/bash
# Get the directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -d "$DIR/../backend/.venv" ]; then
    # Use the python binary inside the virtualenv directly
    "$DIR/../backend/.venv/bin/python" "$DIR/../backend/scripts/seed_demo.py" "$@"
else
    # Fallback to system python if venv does not exist
    python3 "$DIR/../backend/scripts/seed_demo.py" "$@"
fi
