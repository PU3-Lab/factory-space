"""Unit tests for the RecipePreValidator."""

from __future__ import annotations

from sqlalchemy.orm import Session

from agents.material_generation.prevalidator import RecipePreValidator


def test_prevalidator_valid_inputs(db_session: Session) -> None:
    inputs = [{"item_id": "iron_ore", "qty": 2}]
    err = RecipePreValidator.validate_inputs(db_session, "Smelter", inputs)
    assert err is None


def test_prevalidator_invalid_machine(db_session: Session) -> None:
    inputs = [{"item_id": "iron_ore", "qty": 2}]
    err = RecipePreValidator.validate_inputs(db_session, "Furnace", inputs)
    assert err == "invalid_machine"


def test_prevalidator_invalid_qty(db_session: Session) -> None:
    inputs = [{"item_id": "iron_ore", "qty": 0}]
    err = RecipePreValidator.validate_inputs(db_session, "Smelter", inputs)
    assert err == "invalid_qty"


def test_prevalidator_unknown_item(db_session: Session) -> None:
    inputs = [{"item_id": "unknown_gem", "qty": 1}]
    err = RecipePreValidator.validate_inputs(db_session, "Smelter", inputs)
    assert err == "unknown_item"
