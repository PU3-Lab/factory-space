"""Game data loader for resources.csv and recipes.csv."""

from __future__ import annotations

import csv
import re
from pathlib import Path

_initialized = False
_valid_item_ids: set[str] = set()
_item_name_map: dict[str, str] = {}
_recipe_map: dict[str, tuple[str, list[str]]] = {}


def _init_game_data() -> None:
    global _initialized
    if _initialized:
        return

    current_dir = Path(__file__).resolve().parent
    data_dir = None

    # Search upwards for 'data/game' directory
    for parent in [current_dir] + list(current_dir.parents):
        candidate = parent / "data" / "game"
        if candidate.exists() and candidate.is_dir():
            data_dir = candidate
            break

    # Fallback to absolute workspace path if not found relative to script
    if not data_dir:
        workspace_root = Path("/Users/kimkyungpyo/Workspaces/projests/factory-space")
        candidate = workspace_root / "data" / "game"
        if candidate.exists() and candidate.is_dir():
            data_dir = candidate

    if not data_dir:
        raise FileNotFoundError("Could not find data/game directory in any parent paths.")

    resources_path = data_dir / "resources.csv"
    recipes_path = data_dir / "recipes.csv"

    # Load resources
    with open(resources_path, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        next(reader)  # Skip header
        for row in reader:
            if not row or len(row) < 2:
                continue
            item_id = row[0].strip()
            item_name = row[1].strip()
            _valid_item_ids.add(item_id)
            _item_name_map[item_id] = item_name

    # Load recipes
    with open(recipes_path, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        next(reader)  # Skip header
        for row in reader:
            if not row or len(row) < 4:
                continue
            recipe_id = row[0].strip()
            inputs_col = row[2].strip()
            output_col = row[3].strip()

            input_ids = re.findall(r"resource_[a-zA-Z0-9_]+", inputs_col)
            output_ids = re.findall(r"resource_[a-zA-Z0-9_]+", output_col)

            if output_ids:
                _recipe_map[recipe_id] = (output_ids[0], input_ids)

    _initialized = True


def get_valid_item_ids() -> set[str]:
    """Returns the set of all valid item IDs (resource_*)."""
    _init_game_data()
    return _valid_item_ids


def get_item_name(item_id: str) -> str:
    """Returns the Korean name of the item. Returns the item_id itself if not found."""
    _init_game_data()
    return _item_name_map.get(item_id, item_id)


def get_recipe_map() -> dict[str, tuple[str, list[str]]]:
    """Returns the map of recipe_id -> (output_item_id, input_item_ids)."""
    _init_game_data()
    return _recipe_map
