from __future__ import annotations

import os
import tempfile
import unittest.mock
from collections.abc import Generator

import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from agents.quest_generator.compose_service import compose_first_support_quest
from agents.quest_generator.models import QuestContext, SupportQuestDraft
from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
from db.models import Base


@pytest.fixture
def test_db() -> Generator[Session, None, None]:
    """임시 SQLite DB 세션을 제공하는 fixture입니다."""
    db_fd, db_path = tempfile.mkstemp(suffix=".db")
    os.close(db_fd)

    db_url = f"sqlite:///{db_path}"
    engine = create_engine(db_url, connect_args={"check_same_thread": False})
    Base.metadata.create_all(bind=engine)
    session_local = sessionmaker(bind=engine, autoflush=False, autocommit=False)
    session = session_local()
    try:
        yield session
    finally:
        session.close()
        if os.path.exists(db_path):
            os.unlink(db_path)


@pytest.fixture
def mock_refiner() -> unittest.mock.Mock:
    """PhraseRefiner를 모킹하여 입력된 초안을 그대로 반환하도록 합니다."""
    refiner = unittest.mock.Mock(spec=QuestPhraseRefiner)

    def mock_refine(
        draft: SupportQuestDraft, context: QuestContext
    ) -> SupportQuestDraft:
        return draft.model_copy()

    refiner.refine.side_effect = mock_refine
    return refiner


def test_compose_service_success(
    test_db: Session, mock_refiner: unittest.mock.Mock
) -> None:
    # 1. 부족 자원이 있고 레시피가 해금되어 생성이 가능한 상태
    payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_001",
                    "objective_type": "collect_item",
                    "item_id": "resource_copper_ingot",
                    "required": 10,
                    "current": 2,
                }
            ],
        },
        "inventory": {"resource_copper_ingot": 2},
        "unlocked_recipes": ["recipe_copper_ingot"],
    }

    # 2. 실행
    res = compose_first_support_quest(
        session=test_db,
        factory_id="factory_001",
        context_payload=payload,
        phrase_refiner=mock_refiner,
    )

    # 3. 검증
    assert res.outcome == "composed"
    assert res.instance is not None
    assert res.instance.factory_id == "factory_001"
    assert res.instance.status == "in_progress"
    assert len(res.instance.objectives) == 1
    assert res.instance.objectives[0].target_id == "resource_copper_ingot"


def test_compose_service_limit_exceeded(
    test_db: Session, mock_refiner: unittest.mock.Mock
) -> None:
    # 1. 3개의 활성 퀘스트를 미리 생성해 둠
    from agents.quest_generator.manager import QuestManager
    from agents.quest_generator.models import QuestObjective, QuestReward

    draft = SupportQuestDraft(
        title="임시 퀘스트",
        description="설명",
        quest_type="support",
        support_type="collect_item",
        objectives=[
            QuestObjective(
                id="obj_1",
                type="collect_item",
                target_id="resource_wood",
                target_amount=10,
            )
        ],
        rewards=[QuestReward(type="currency", target_id="gold", amount=100)],
    )
    for i in range(3):
        QuestManager.create_quest_from_draft(test_db, "factory_001", draft, "main_001")

    # 2. 추가 생성 요청
    payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "나무 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_001",
                    "objective_type": "collect_item",
                    "item_id": "resource_wood",
                    "required": 10,
                    "current": 2,
                }
            ],
        },
        "inventory": {"resource_wood": 2},
    }

    res = compose_first_support_quest(
        session=test_db,
        factory_id="factory_001",
        context_payload=payload,
        phrase_refiner=mock_refiner,
    )

    # 3. 검증: 상한 초과로 생성 안 됨
    assert res.outcome == "none"
    assert res.reason == "limit_exceeded"


def test_compose_service_no_candidates(
    test_db: Session, mock_refiner: unittest.mock.Mock
) -> None:
    # 1. 부족 자원이 없는 상태
    payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_001",
                    "objective_type": "collect_item",
                    "item_id": "resource_copper_ingot",
                    "required": 10,
                    "current": 10,  # 꽉 참
                }
            ],
        },
        "inventory": {"resource_copper_ingot": 10},
        "unlocked_recipes": ["recipe_copper_ingot"],
    }

    res = compose_first_support_quest(
        session=test_db,
        factory_id="factory_001",
        context_payload=payload,
        phrase_refiner=mock_refiner,
    )

    # 3. 검증: 후보군 없음
    assert res.outcome == "none"
    assert res.reason == "no_candidates"


def test_compose_service_no_valid_draft(
    test_db: Session, mock_refiner: unittest.mock.Mock
) -> None:
    # 1. 구리괴 제작 레시피가 해금되지 않아 feasibility(달성 가능성) 검증 실패
    payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_001",
                    "objective_type": "collect_item",
                    "item_id": "resource_copper_ingot",
                    "required": 10,
                    "current": 2,
                }
            ],
        },
        "inventory": {"resource_copper_ingot": 0},
        "unlocked_recipes": [],  # 레시피 해금 안 됨
    }

    res = compose_first_support_quest(
        session=test_db,
        factory_id="factory_001",
        context_payload=payload,
        phrase_refiner=mock_refiner,
    )

    # 3. 검증: 통과한 draft 없음
    assert res.outcome == "none"
    assert res.reason == "no_valid_draft"
