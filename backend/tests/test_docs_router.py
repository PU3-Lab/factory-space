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


def test_docs_router_does_not_change_health_endpoint() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}
