from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_manual_qa_docs_page_returns_html() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/manual-qa-docs")

    assert response.status_code == 200
    assert "text/html" in response.headers["content-type"]
    assert "operator_guide" in response.text
    assert "ManualQAService" in response.text
    assert "mermaid" in response.text


def test_manual_qa_architecture_page_returns_html() -> None:
    with TestClient(create_app()) as client:
        response = client.get("/manual-qa-architecture")

    assert response.status_code == 200
    assert "text/html" in response.headers["content-type"]
    assert "LangChain / LangGraph" in response.text
    assert "append_middleware_log" in response.text
    assert "CsvManualQARepository" in response.text
