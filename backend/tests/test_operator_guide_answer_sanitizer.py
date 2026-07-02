from agents.base import AgentContext
from agents.operator_guide.answer_sanitizer import (
    sanitize_operator_guide_response_payload,
    sanitize_player_facing_answer,
)
from agents.pipeline.runtime import AgentPipeline
from agents.pipeline.utils import build_cache_key
from cache.response_cache import ResponseCache
from tests.harness import StubLLM, top_agent_decision


def test_sanitize_player_facing_answer_removes_duplicate_name_and_raw_ids() -> None:
    answer = (
        "분쇄기(분쇄기)는 고체 자원을 분말/톱밥으로 바꾸는 장비예요. "
        "철괴는 제련기(equipment_smelter)에서 만들어요."
    )

    assert sanitize_player_facing_answer(answer) == (
        "분쇄기는 고체 자원을 분말이나 톱밥으로 바꾸는 장비예요.\n\n"
        "철괴는 제련기에서 만들어요."
    )


def test_sanitize_operator_guide_response_payload_updates_only_final_answer() -> None:
    payload = {
        "final_answer": "분쇄기(분쇄기)는 분말/톱밥을 만들어요.",
        "actions": [],
        "question": "분쇄기가 뭐야?",
        "topic": "machine",
    }

    sanitized = sanitize_operator_guide_response_payload(payload)

    assert sanitized["final_answer"] == "분쇄기는 분말이나 톱밥을 만들어요."
    assert sanitized["question"] == "분쇄기가 뭐야?"
    assert payload["final_answer"] == "분쇄기(분쇄기)는 분말/톱밥을 만들어요."


def test_sanitize_player_facing_answer_splits_dialogue_paragraphs() -> None:
    answer = (
        "제련기는 광석이나 원재료를 가공해서 주괴 같은 자원으로 바꿔 주는 생산 장비예요. "
        "제련기에서 나온 결과물은 다음 공정의 재료로 이어져서 더 복잡한 제작으로 사용됩니다."
    )

    assert sanitize_player_facing_answer(answer) == (
        "제련기는 광석이나 원재료를 가공해서 주괴 같은 자원으로 바꿔 주는 생산 장비예요.\n\n"
        "제련기에서 나온 결과물은 다음 공정의 재료로 이어져서 더 복잡한 제작으로 사용됩니다."
    )


def test_pipeline_sanitizes_cached_operator_guide_response() -> None:
    payload = {
        "sub_agent": "operator_guide.machine_help",
        "question": "분쇄기가 뭐야?",
    }
    context = AgentContext(
        request_id="cache-sanitize",
        session_id="cache-session",
        client_id="cache-client",
        metadata={},
    )
    cache_key = build_cache_key(
        "operator_guide",
        "operator_guide.machine_help",
        payload,
        context,
    )
    cache = ResponseCache()
    cache.set(
        cache_key,
        {
            "final_answer": "분쇄기(분쇄기)는 분말/톱밥을 만들어요.",
            "actions": [],
            "question": "분쇄기가 뭐야?",
            "topic": "machine",
        },
    )
    pipeline = AgentPipeline(
        cache=cache,
        llm=StubLLM(
            [
                top_agent_decision("operator_guide"),
            ]
        ),
    )

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "cache-sanitize",
            "session_id": "cache-session",
            "client_id": "cache-client",
            "agent": "operator_guide",
            "payload": payload,
        }
    )

    assert response["payload"]["final_answer"] == ("분쇄기는 분말이나 톱밥을 만들어요.")
    assert response["payload"]["metadata"]["cache"] == "hit"
