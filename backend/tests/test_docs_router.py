from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_quest_agent_docs_page_returns_renderable_html() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/quest-agent-docs")

    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/html")
    assert "https://cdn.jsdelivr.net/npm/mermaid" in response.text
    assert "퀘스트 에이전트 서비스 문서" in response.text
    assert "quest_generator.production_quest" in response.text
    assert "quest_generator.economy_quest" in response.text
    assert "quest_generator.tutorial_quest" not in response.text
    assert "quest_generator.exploration_quest" not in response.text
    assert "사용자 입력 예시" in response.text


def test_quest_agent_architecture_page_returns_code_mapping_html() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/quest-agent-architecture")

    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/html")
    assert "퀘스트 에이전트 아키텍처" in response.text
    assert "LangChain/LangGraph 개념 매핑" in response.text
    assert "StateGraph" in response.text
    assert "before_model" in response.text
    assert "quest_generator.production_quest" in response.text
    assert "quest_generator.economy_quest" in response.text
    assert "quest_generator.tutorial_quest" not in response.text
    assert "quest_generator.exploration_quest" not in response.text


def test_agent_test_page_has_random_inputs_button() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/agent-test")

    assert response.status_code == 200
    assert "랜덤 조합" in response.text
    assert "function randomInputs()" in response.text
    assert 'onclick="randomInputs()"' in response.text


def test_agent_test_response_log_has_independent_scroll_area() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/agent-test")

    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/html")
    assert "#log" in response.text
    assert "overscroll-behavior: contain" in response.text
    assert "scrollbar-gutter: stable" in response.text
    assert "white-space: pre-wrap" in response.text
    assert "word-break: break-word" in response.text


def test_agent_test_progress_messages_do_not_update_quality_metrics() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/agent-test")

    assert response.status_code == 200
    assert "function isProgressMessage(parsed)" in response.text
    assert "if (isProgressMessage(parsed)) return;" in response.text
    assert "t-progress" in response.text
    assert "PROGRESS" in response.text


def test_agent_test_page_documents_unreal_material_fields() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/agent-test")

    assert response.status_code == 200
    assert "신물질 DataTable 필드" in response.text
    assert "rowname: CSV/DataTable 행 식별자" in response.text
    assert "form: 물질의 물리 상태" in response.text
    assert "substance: 기본 성분 또는 재료 계열" in response.text
    assert "type: 물질 분류" in response.text
    assert "shape: 물질 형태" in response.text
    assert "DIsplayName: UI에 표시할 한국어 이름" in response.text
    assert "VisualColor: 언리얼 RGBA 색상 문자열" in response.text
    assert '<option value="new_material_generator">신소재 후보 생성</option>' in response.text
    assert '"agent": "new_material_generator"' in response.text
    assert '"goal": "heat-resistant alloy"' in response.text
    assert "mrRow('RowName', p.rowname)" in response.text
    assert "mrRow('DisplayName', p.DIsplayName)" in response.text
    assert "mrRow('VisualColor', p.VisualColor)" in response.text


def test_docs_router_does_not_change_health_endpoint() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}
