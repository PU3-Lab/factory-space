"""Unit tests for the ExperimentClassifier."""

from __future__ import annotations

from sqlalchemy.orm import Session

from agents.material_generation.classifier import ExperimentClassifier


def test_classifier_simple_variation(db_session: Session) -> None:
    inputs = [{"item_id": "iron_ore", "qty": 5}]
    cls_type = ExperimentClassifier.classify(db_session, "Smelter", inputs)
    assert cls_type == "simple_variation"


def test_classifier_failed_result(db_session: Session) -> None:
    inputs = [
        {"item_id": "iron_ore", "qty": 1},
        {"item_id": "iron_ingot", "qty": 1},
    ]
    cls_type = ExperimentClassifier.classify(db_session, "Smelter", inputs)
    assert cls_type == "failed_result"


def test_classifier_intermediate_material(db_session: Session) -> None:
    inputs = [
        {"item_id": "iron_ore", "qty": 1},
        {"item_id": "iron_ingot", "qty": 1},
    ]
    cls_type = ExperimentClassifier.classify(db_session, "Grinder", inputs)
    assert cls_type == "intermediate_material"


def test_classifier_candidate(db_session: Session) -> None:
    inputs = [
        {"item_id": "iron_ore", "qty": 1},
        {"item_id": "iron_ingot", "qty": 1},
    ]
    cls_type = ExperimentClassifier.classify(db_session, "Synthesizer", inputs)
    assert cls_type == "candidate"
