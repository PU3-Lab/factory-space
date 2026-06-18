from __future__ import annotations

import os
import tempfile
import unittest.mock
from collections.abc import Generator
from types import TracebackType

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from agents.quest_generator.models import (
    QuestContext,
    SupportQuestDraft,
    ValidationResult,
)
from app import create_app
from db.models import Base


class MockDbSession:
    """테스트 실행 도중 라우터의 DB 세션을 대접하기 위한 Mock 컨텍스트 매니저입니다."""

    def __init__(self, session: Session) -> None:
        self.session = session

    def __enter__(self) -> Session:
        return self.session

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: TracebackType | None,
    ) -> None:
        pass


@pytest.fixture
def test_db() -> Generator[Session, None, None]:
    """E2E 테스트를 위한 임시 파일 기반 SQLite 데이터베이스 세션을 제공합니다."""
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
def mock_db_ctx(test_db: Session) -> Generator[None, None, None]:
    """라우터 내부의 get_db_session을 E2E 임시 세션으로 모킹하는 fixture입니다. (DRY)"""
    mock_session_ctx = MockDbSession(test_db)
    with unittest.mock.patch(
        "agents.quest_generator.quest_router.get_db_session",
        return_value=mock_session_ctx,
    ):
        yield


def test_quest_router_flow_e2e(mock_db_ctx: None) -> None:
    app = create_app()
    client = TestClient(app)

    # 1. GET 목록 조회 -> 빈 목록 [] 확인
    response = client.get("/api/v1/factories/factory_001/quests")
    assert response.status_code == 200
    assert response.json() == []

    # 2. compose-support 요청용 payload 셋업
    # 구리괴가 부족한 상태의 context
    context_payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 10개 확보",
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
        "unlocked_recipes": [
            "recipe_copper_ingot"  # 구리괴 제작 레시피 해금
        ],
    }

    # 3. compose-support 호출 -> 200/201 응답 및 퀘스트 인스턴스 정보 확인
    resp_compose = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
    )
    assert resp_compose.status_code in (200, 201)
    instance = resp_compose.json()
    assert "id" in instance
    assert instance["factory_id"] == "factory_001"
    assert instance["status"] == "in_progress"
    assert len(instance["objectives"]) == 1
    assert instance["objectives"][0]["target_id"] == "resource_copper_ingot"
    assert instance["objectives"][0]["target_amount"] == 10
    assert instance["objectives"][0]["current_amount"] == 0

    # 4. 다시 목록 조회 -> 1개 퀘스트 확인
    resp_list = client.get("/api/v1/factories/factory_001/quests")
    assert resp_list.status_code == 200
    list_data = resp_list.json()
    assert len(list_data) == 1
    assert list_data[0]["id"] == instance["id"]
    assert list_data[0]["status"] == "in_progress"

    # 5. 이벤트 전송 -> item_collected로 보유량 10 달성
    event_payload = {
        "event_id": "evt_e2e_100",
        "factory_id": "factory_001",
        "item_id": "copper_ingot",
        "current_total": 10,
    }
    resp_event = client.post(
        "/api/v1/factories/factory_001/quests/events", json=event_payload
    )
    assert resp_event.status_code in (200, 204)

    # 6. 목록 재조회 -> completed 확인
    resp_list_final = client.get("/api/v1/factories/factory_001/quests")
    assert resp_list_final.status_code == 200
    list_data_final = resp_list_final.json()
    assert len(list_data_final) == 1
    assert list_data_final[0]["status"] == "completed"
    assert list_data_final[0]["completed_at"] is not None


def test_quest_router_limits_and_duplication(mock_db_ctx: None) -> None:
    app = create_app()
    client = TestClient(app)

    # 1. 3개의 서로 다른 지원 퀘스트를 성공적으로 생성할 수 있는지 확인
    # (나무, 석탄, 철광석 부족)
    items = ["wood", "coal", "iron_ore"]
    created_ids = []

    for idx, item in enumerate(items):
        context_payload = {
            "factory_id": "factory_001",
            "factory_level": 1,
            "current_main_quest": {
                "quest_id": "main_001",
                "title": "재료 확보",
                "objectives": [
                    {
                        "main_objective_id": f"mobj_{idx}",
                        "objective_type": "collect_item",
                        "item_id": f"resource_{item}",
                        "required": 10,
                        "current": 1,
                    }
                ],
            },
            "inventory": {f"resource_{item}": 1},
            "unlocked_recipes": [],
        }
        # compose-support 호출
        resp = client.post(
            "/api/v1/factories/factory_001/quests/compose-support",
            json=context_payload,
        )
        assert resp.status_code in (200, 201)
        created_ids.append(resp.json()["id"])

    # 2. 4번째 compose-support 호출 -> active 지원 퀘스트 상한 3개 초과로 400 Bad Request 확인
    extra_payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리 확보",
            "objectives": [
                {
                    "main_objective_id": "mobj_extra",
                    "objective_type": "collect_item",
                    "item_id": "resource_copper_ingot",
                    "required": 10,
                    "current": 1,
                }
            ],
        },
        "inventory": {"resource_copper_ingot": 1},
        "unlocked_recipes": [],
    }
    resp_fail_limit = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=extra_payload
    )
    assert resp_fail_limit.status_code == 400
    assert "limit" in resp_fail_limit.json()["detail"].lower()

    # 3. 1개 완료 처리 -> active 개수 2개로 감소
    event_payload = {
        "event_id": "evt_limit_clear",
        "factory_id": "factory_001",
        "item_id": "wood",
        "current_total": 10,
    }
    resp_event = client.post(
        "/api/v1/factories/factory_001/quests/events", json=event_payload
    )
    assert resp_event.status_code in (200, 204)

    # 4. 이제 다시 compose-support 호출 -> 1개 생성 성공 확인
    resp_success = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=extra_payload
    )
    assert resp_success.status_code in (200, 201)


def test_quest_router_event_factory_id_mismatch(mock_db_ctx: None) -> None:
    """이벤트 바디의 factory_id와 경로의 factory_id가 다를 때 400 Bad Request를 리턴하는지 검증합니다. (m1)"""
    app = create_app()
    client = TestClient(app)

    event_payload = {
        "event_id": "evt_mismatch_100",
        "factory_id": "factory_999",  # mismatch with path
        "item_id": "copper_ingot",
        "current_total": 10,
    }
    response = client.post(
        "/api/v1/factories/factory_001/quests/events", json=event_payload
    )
    assert response.status_code == 400
    assert "match" in response.json()["detail"].lower()


def test_quest_router_duplicate_target_blocked(mock_db_ctx: None) -> None:
    """활성 퀘스트의 대상 아이템과 동일한 아이템으로 지원 퀘스트 생성을 다시 요청할 때, 중복 차단되어 400 Bad Request를 반환하는지 검증합니다. (B1)"""
    app = create_app()
    client = TestClient(app)

    # 1. 구리괴가 부족한 상태의 context로 첫 번째 퀘스트 생성 요청
    context_payload = {
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

    # 첫 번째 생성: 성공 (201 Created)
    resp1 = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
    )
    assert resp1.status_code in (200, 201)

    # 2. 동일한 아이템(resource_copper_ingot)에 대한 두 번째 생성 요청: 실패 (400 Bad Request)
    resp2 = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
    )
    assert resp2.status_code == 400
    assert "already active" in resp2.json()["detail"].lower()


def test_compose_support_uses_refined_phrasing(mock_db_ctx: None) -> None:
    """QuestPhraseRefiner가 다듬은 문구로 퀘스트가 정상 생성되는지 검증합니다."""
    app = create_app()
    client = TestClient(app)

    # get_phrase_refiner 모킹을 위해 mock 설정
    from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
    from agents.quest_generator.quest_router import get_phrase_refiner

    mock_refiner = unittest.mock.Mock(spec=QuestPhraseRefiner)

    def mock_refine(
        draft: SupportQuestDraft, context: QuestContext
    ) -> SupportQuestDraft:
        return draft.model_copy(
            update={
                "title": "다듬어진 구리괴 수집",
                "description": "다듬어진 설명 문구입니다.",
            }
        )

    mock_refiner.refine.side_effect = mock_refine
    app.dependency_overrides[get_phrase_refiner] = lambda: mock_refiner

    context_payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 10개 확보",
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

    resp = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
    )
    assert resp.status_code in (200, 201)
    data = resp.json()
    assert data["title"] == "다듬어진 구리괴 수집"
    assert data["description"] == "다듬어진 설명 문구입니다."
    # 수량 등 중요 정보는 유지
    assert data["objectives"][0]["target_id"] == "resource_copper_ingot"
    assert data["objectives"][0]["target_amount"] == 10


def test_compose_support_keeps_rule_decided_values(mock_db_ctx: None) -> None:
    """LLM이 objectives나 rewards를 임의로 변조하려 하더라도, 강제로 원본 규칙의 값으로 유지되는지 검증합니다."""
    app = create_app()
    client = TestClient(app)

    from agents.quest_generator.models import QuestObjective, QuestReward
    from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
    from agents.quest_generator.quest_router import get_phrase_refiner

    mock_refiner = unittest.mock.Mock(spec=QuestPhraseRefiner)

    # LLM이 수량을 999로, 보상을 9999로 바꾼 draft를 반환하도록 refiner가 위조함
    def mock_refine(
        draft: SupportQuestDraft, context: QuestContext
    ) -> SupportQuestDraft:
        modified_objectives = [
            QuestObjective(
                id=draft.objectives[0].id,
                type=draft.objectives[0].type,
                target_id="resource_gold_ore",  # 아이템 ID 변조
                target_amount=999,  # 수량 변조
                current_amount=0,
                status="in_progress",
            )
        ]
        modified_rewards = [
            QuestReward(
                type="currency",
                target_id="gold",
                amount=9999,  # 보상 변조
            )
        ]
        return draft.model_copy(
            update={
                "title": "변조된 퀘스트",
                "description": "설명",
                "objectives": modified_objectives,
                "rewards": modified_rewards,
            }
        )

    mock_refiner.refine.side_effect = mock_refine
    app.dependency_overrides[get_phrase_refiner] = lambda: mock_refiner

    context_payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 10개 확보",
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

    resp = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
    )
    assert resp.status_code in (200, 201)
    data = resp.json()
    assert data["title"] == "변조된 퀘스트"  # 문구는 변경 허용

    # 핵심 가드: objectives, rewards 등 수치 정보는 원본 draft의 규칙 값으로 강제 복원되었는지 검증
    assert data["objectives"][0]["target_id"] == "resource_copper_ingot"
    assert data["objectives"][0]["target_amount"] == 10
    assert data["rewards"][0]["amount"] == 100  # 원본 보상값(100) 유지


def test_compose_support_falls_back_when_llm_unavailable(mock_db_ctx: None) -> None:
    """LLM refiner가 예외를 던지며 실패하더라도, 원본 draft 그대로 정상 생성(201)되는지 검증합니다."""
    app = create_app()
    client = TestClient(app)

    from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
    from agents.quest_generator.quest_router import get_phrase_refiner

    mock_refiner = unittest.mock.Mock(spec=QuestPhraseRefiner)
    mock_refiner.refine.side_effect = RuntimeError("LLM Service Outage")
    app.dependency_overrides[get_phrase_refiner] = lambda: mock_refiner

    context_payload = {
        "factory_id": "factory_001",
        "factory_level": 1,
        "current_main_quest": {
            "quest_id": "main_001",
            "title": "구리괴 10개 확보",
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

    resp = client.post(
        "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
    )
    assert resp.status_code in (200, 201)
    data = resp.json()
    # 원본 문구가 그대로 사용됨
    assert "구리괴" in data["title"]
    assert data["objectives"][0]["target_id"] == "resource_copper_ingot"


def test_revalidation_failure_falls_back_to_original_draft(mock_db_ctx: None) -> None:
    """윤색된 퀘스트 draft가 재검증(QuestValidator.validate)에 실패하더라도, 원본 draft로 정상 생성(201)되는지 검증합니다."""
    app = create_app()
    client = TestClient(app)

    from agents.quest_generator.models import ValidationResult
    from agents.quest_generator.phrase_refiner import QuestPhraseRefiner
    from agents.quest_generator.quest_router import get_phrase_refiner
    from agents.quest_generator.validator import QuestValidator

    mock_refiner = unittest.mock.Mock(spec=QuestPhraseRefiner)

    def mock_refine(
        draft: SupportQuestDraft, context: QuestContext
    ) -> SupportQuestDraft:
        return draft.model_copy(
            update={"title": "윤색되었으나 검증실패할 제목", "description": "설명"}
        )

    mock_refiner.refine.side_effect = mock_refine
    app.dependency_overrides[get_phrase_refiner] = lambda: mock_refiner

    # 첫 번째 검증(selected_draft)은 통과하지만, 두 번째 검증(refined_draft)에서 실패하도록 Validator.validate를 모킹
    original_validate = QuestValidator.validate
    call_count = 0

    def mock_validate(
        draft: SupportQuestDraft, context: QuestContext, active_target_ids: set[str]
    ) -> ValidationResult:
        nonlocal call_count
        call_count += 1
        if call_count == 1:
            # 1. selected_draft 찾기 단계: 정상 통과
            return original_validate(draft, context, active_target_ids)
        else:
            # 2. refined_draft 재검증 단계: 실패 시뮬레이션
            return ValidationResult(
                valid=False,
                reason="invalid_refined_content",
                message="LLM output failed validation",
            )

    with unittest.mock.patch(
        "agents.quest_generator.compose_service.QuestValidator.validate",
        side_effect=mock_validate,
    ):
        context_payload = {
            "factory_id": "factory_001",
            "factory_level": 1,
            "current_main_quest": {
                "quest_id": "main_001",
                "title": "구리괴 10개 확보",
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

        resp = client.post(
            "/api/v1/factories/factory_001/quests/compose-support", json=context_payload
        )
        assert resp.status_code in (200, 201)
        data = resp.json()

        # 중요: 재검증 실패로 인해 윤색된 제목이 아니라 원본 제목이 되어야 함
        assert data["title"] != "윤색되었으나 검증실패할 제목"
        assert "구리괴" in data["title"]
        assert data["objectives"][0]["target_id"] == "resource_copper_ingot"
