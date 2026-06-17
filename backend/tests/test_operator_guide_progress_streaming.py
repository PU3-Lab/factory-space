"""operator_guide 진행 메시지 스트리밍 테스트.

초보자용 설명:
    operator_guide는 LLM 답변이 완성되기 전에 플레이어에게
    "지금 어떤 작업을 하는 중인지" 알려주는 progress message를 보냅니다.
    이 테스트는 진행 메시지가 질문 유형별로 올바른 순서로 나오고,
    pipeline 경로에서도 중복 전송되지 않는지 확인합니다.
"""

from __future__ import annotations

from agents.operator_guide.service import ManualQAService
from agents.pipeline.runtime import AgentPipeline
from tests.harness import StubLLM, top_agent_decision

RECIPE_PROGRESS = [
    ("rag_search", "관련 레시피를 찾는 중입니다..."),
    ("state_check", "필요한 입력 자원을 확인하는 중입니다..."),
    ("logic_format", "생산 흐름을 정리하는 중입니다..."),
]

MACHINE_PROGRESS = [
    ("rag_search", "장비 매뉴얼을 펼쳐보는 중입니다..."),
    ("state_check", "입력과 출력 자원을 확인하는 중입니다..."),
    ("logic_format", "연결 가능한 장비를 살펴보는 중입니다..."),
]

TROUBLESHOOTING_PROGRESS = [
    ("rag_search", "공장의 전체 흐름을 읽는 중입니다..."),
    ("state_check", "선택된 장비 상태를 확인하는 중입니다..."),
    ("power_check", "전력과 입력 자원 상태를 대조하는 중입니다..."),
    ("document_find", "관련 문제 해결 매뉴얼을 찾는 중입니다..."),
    ("step_arrange", "점검 순서를 정리하는 중입니다..."),
]


def test_progress_streaming_for_recipe_topic() -> None:
    """레시피 질문에서 진행 메시지가 정해진 순서로 한 번씩 나오는지 확인한다."""
    progress_calls: list[tuple[str, str]] = []

    def on_progress(stage: str, message: str) -> None:
        progress_calls.append((stage, message))

    service = ManualQAService()
    service.build_prompt(
        question="기어는 어떻게 만들어?",
        topic="recipe",
        sub_agent="operator_guide.recipe_explainer",
        on_progress=on_progress,
    )

    assert progress_calls == RECIPE_PROGRESS


def test_progress_streaming_for_machine_topic() -> None:
    """장비 질문에서 진행 메시지가 정해진 순서로 한 번씩 나오는지 확인한다."""
    progress_calls: list[tuple[str, str]] = []

    def on_progress(stage: str, message: str) -> None:
        progress_calls.append((stage, message))

    service = ManualQAService()
    service.build_prompt(
        question="제련기는 뭐야?",
        topic="machine",
        sub_agent="operator_guide.machine_help",
        on_progress=on_progress,
    )

    assert progress_calls == MACHINE_PROGRESS


def test_progress_streaming_for_troubleshooting_topic() -> None:
    """문제 해결 질문에서 진행 메시지가 정해진 순서로 한 번씩 나오는지 확인한다."""
    progress_calls: list[tuple[str, str]] = []

    def on_progress(stage: str, message: str) -> None:
        progress_calls.append((stage, message))

    service = ManualQAService()
    service.build_prompt(
        question="철괴가 안 만들어져. 왜 그래?",
        topic="troubleshooting",
        sub_agent="operator_guide.troubleshooter",
        on_progress=on_progress,
    )

    assert progress_calls == TROUBLESHOOTING_PROGRESS


def test_pipeline_emits_progress_once_per_stage() -> None:
    """AgentPipeline 경로에서도 progress message가 중복 없이 한 번씩만 전달되는지 확인한다."""
    progress_calls: list[tuple[str, str]] = []

    def on_progress(stage: str, message: str) -> None:
        progress_calls.append((stage, message))

    llm = StubLLM([
        top_agent_decision("operator_guide"),
        None,
    ])
    pipeline = AgentPipeline(llm=llm)
    message = {
        "type": "agent.request",
        "request_id": "test-pipeline-progress",
        "agent": "operator_guide",
        "payload": {
            "sub_agent": "operator_guide.recipe_explainer",
            "question": "구리선은 어떻게 만들어?",
        },
    }

    response = pipeline.run(message, on_progress=on_progress)

    assert response["type"] == "agent.response"
    assert progress_calls == RECIPE_PROGRESS
