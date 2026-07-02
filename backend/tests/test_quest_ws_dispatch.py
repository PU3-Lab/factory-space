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

from app import create_app
from db.models import Base


class MockDbSession:
    """테스트용 DB 세션 컨텍스트 매니저 Mock입니다."""

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
    """임시 SQLite DB 세션을 생성하는 fixture입니다."""
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
        engine.dispose()
        if os.path.exists(db_path):
            os.unlink(db_path)


@pytest.fixture
def mock_db_ctx(test_db: Session) -> Generator[None, None, None]:
    """quest_dispatch 내부의 get_db_session을 임시 DB 세션으로 모킹합니다."""
    mock_session_ctx = MockDbSession(test_db)
    with unittest.mock.patch(
        "websocket_gateway.quest_dispatch.get_db_session",
        return_value=mock_session_ctx,
    ):
        yield


def test_ws_tutorial_completed_composed(mock_db_ctx: None) -> None:
    app = create_app()
    with TestClient(app) as client:
        # 1. WS 연결 및 정상 tutorial_completed 메시지 송신
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "quest.tutorial_completed",
                    "request_id": "req-ws-123",
                    "session_id": "sess-ws-123",
                    "client_id": "cli-ws-123",
                    "payload": {
                        "context": {
                            "factory_id": "factory_ws_test",
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
                    },
                }
            )

            response = websocket.receive_json()

    # 2. 검증: 정상적으로 quest.composed 가 반환되고 반향 필드가 일치해야 함
    assert response["type"] == "quest.composed"
    assert response["request_id"] == "req-ws-123"
    assert response["session_id"] == "sess-ws-123"
    assert response["client_id"] == "cli-ws-123"

    payload = response["payload"]
    assert payload["factory_id"] == "factory_ws_test"
    assert payload["status"] == "in_progress"
    assert len(payload["objectives"]) == 1
    assert payload["objectives"][0]["target_id"] == "resource_copper_ingot"


def test_ws_tutorial_completed_none(mock_db_ctx: None) -> None:
    app = create_app()
    with TestClient(app) as client:
        # 1. 부족 자원이 없어 퀘스트 생성이 불가한 페이로드 송신
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "quest.tutorial_completed",
                    "request_id": "req-ws-456",
                    "session_id": "sess-ws-456",
                    "client_id": "cli-ws-456",
                    "payload": {
                        "context": {
                            "factory_id": "factory_ws_test",
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
                    },
                }
            )

            response = websocket.receive_json()

    # 2. 검증: quest.none 이 반환되어야 함
    assert response["type"] == "quest.none"
    assert response["request_id"] == "req-ws-456"
    assert response["payload"]["reason"] == "no_candidates"


def test_ws_tutorial_completed_malformed_payload(mock_db_ctx: None) -> None:
    app = create_app()
    with TestClient(app) as client:
        # 1. payload가 누락된 잘못된 봉투 송신
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "quest.tutorial_completed",
                    "request_id": "req-ws-789",
                }
            )

            response = websocket.receive_json()

    # 2. 검증: agent.error 및 INVALID_PAYLOAD 수신 확인
    assert response["type"] == "agent.error"
    assert response["request_id"] == "req-ws-789"
    assert response["error"]["code"] == "INVALID_PAYLOAD"


def test_ws_unknown_quest_type(mock_db_ctx: None) -> None:
    app = create_app()
    with TestClient(app) as client:
        # 1. 알 수 없는 quest. 하위 메시지 송신
        with client.websocket_connect("/ws/agent") as websocket:
            websocket.send_json(
                {
                    "type": "quest.invalid_action",
                    "request_id": "req-ws-999",
                }
            )

            response = websocket.receive_json()

    # 2. 검증: agent.error 및 UNKNOWN_MESSAGE_TYPE 수신 확인
    assert response["type"] == "agent.error"
    assert response["request_id"] == "req-ws-999"
    assert response["error"]["code"] == "UNKNOWN_MESSAGE_TYPE"
