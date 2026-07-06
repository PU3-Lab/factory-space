from __future__ import annotations

from typing import Any

from agents.pipeline.runtime import AgentPipeline
from tests.harness import StubLLM
from tts.schemas import TTSRequest
from tts.text_selection import select_tts_text


class FakeTTSService:
    def __init__(self) -> None:
        self.requests: list[TTSRequest] = []

    def synthesize_for_payload(self, request: TTSRequest) -> dict[str, Any]:
        self.requests.append(request)
        text = select_tts_text(request.agent, request.payload)
        return {
            "status": "ready",
            "provider": "edge_tts",
            "audio_url": f"/tts/{request.agent}/fake.mp3",
            "content_type": "audio/mpeg",
            "text_hash": "fake",
            "voice_id": "ko-KR-SunHiNeural",
            "model_id": "edge_tts",
            "cached": False,
            "text": text,
        }


def test_operator_guide_response_includes_tts_metadata() -> None:
    llm = StubLLM(
        [
            "operator_guide.machine_help",
            '{"final_answer":"컨베이어를 확인하세요.","actions":[],"question":"막혔어","topic":"troubleshooting"}',
        ]
    )
    pipeline = AgentPipeline(llm=llm, tts_service=FakeTTSService())

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "tts-1",
            "agent": "operator_guide",
            "payload": {"question": "막혔어"},
        }
    )

    assert response["type"] == "agent.response"
    assert response["payload"]["tts"]["status"] == "ready"
    assert response["payload"]["tts"]["audio_url"] == "/tts/operator_guide/fake.mp3"
    assert response["payload"]["tts"]["text"] == "컨베이어를 확인하세요."


def test_process_optimizer_state_update_excludes_tts() -> None:
    # 1. Preview response (status == "preview") should INCLUDE tts
    pipeline_preview = AgentPipeline(llm=StubLLM([]), tts_service=FakeTTSService())
    response_preview = pipeline_preview._attach_tts(
        agent="process_optimizer",
        payload={"status": "preview", "summary": "최적화 제안"},
    )
    assert "tts" in response_preview
    assert response_preview["tts"]["status"] == "ready"

    # 2. Highlight-only state_update response (status == "success") should EXCLUDE tts
    pipeline_state = AgentPipeline(llm=StubLLM([]), tts_service=FakeTTSService())
    response_state = pipeline_state._attach_tts(
        agent="process_optimizer",
        payload={"status": "success", "summary": "정기 상태 업데이트"},
    )
    assert "tts" not in response_state


def test_pipeline_process_optimizer_preview_includes_tts() -> None:
    # Preview response (status == "preview") from pipeline run should include TTS and match display_message
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=FakeTTSService())
    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "request-process-v2-analyze",
            "agent": "process_optimizer",
            "payload": {
                "operation": "analyze",
                "goal": "balance",
                "factoryRevision": 23,
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_v2",
                            "type": "smelter",
                            "status": "operating",
                            "inputs": [{"item_id": "iron_ore", "amount": 10.0}],
                        }
                    ],
                    "conveyors": [],
                    "power_grid": {"produced": 50.0, "consumed": 10.0},
                },
            },
        }
    )

    assert response["type"] == "agent.response"
    assert "tts" in response["payload"]
    assert response["payload"]["tts"]["status"] == "ready"
    # Verify display message matches synthesized text
    assert response["payload"]["tts"]["text"] == response["payload"]["display_message"]


def test_process_optimizer_no_alert_response_gets_backend_display_message_before_tts() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    prepared = pipeline._prepare_process_optimizer_tts_payload(
        {
            "status": "preview",
            "summary": "전체 상태는 안정적입니다.",
            "optimization_alert": {"needed": False},
        }
    )
    response = pipeline._attach_tts(agent="process_optimizer", payload=prepared)

    assert response["display_message"] == "문제가 발견되지 않았습니다."
    assert response["tts"]["text"] == "문제가 발견되지 않았습니다."
    assert service.requests[0].payload["display_message"] == "문제가 발견되지 않았습니다."


def test_process_optimizer_display_message_and_tts_text_stay_aligned() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    response = pipeline._attach_tts(
        agent="process_optimizer",
        payload={
            "status": "preview",
            "display_message": "문제가 발견되지 않았습니다.",
            "summary": "전체 상태는 안정적입니다.",
        },
    )

    assert response["tts"]["status"] == "ready"
    assert response["tts"]["text"] == "문제가 발견되지 않았습니다."
    assert service.requests[0].payload["display_message"] == "문제가 발견되지 않았습니다."


def test_pipeline_uses_tts_text_hint_for_synthesis() -> None:
    # 4차 재리뷰 보강: partial tts (text만 있는 경우)는 입력 힌트로 보고 합성을 수행해야 함
    llm = StubLLM(["operator_guide.machine_help"])
    pipeline = AgentPipeline(llm=llm, tts_service=FakeTTSService())

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "tts-hint",
            "agent": "operator_guide",
            "payload": {
                "question": "막혔어",
                "tts": {"text": "수동으로 지정한 안내음성입니다."},
            },
        }
    )

    assert response["type"] == "agent.response"
    assert response["payload"]["tts"]["status"] == "ready"
    # Verify it used the hint instead of final_answer
    assert response["payload"]["tts"]["text"] == "수동으로 지정한 안내음성입니다."


def test_pipeline_preserves_terminal_failed_disabled_status() -> None:
    # 5차 재리뷰 보강: status가 failed/disabled인 단말 메타데이터는 audio_url이 없어도 다시 합성하지 않고 그대로 보존해야 함
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    # 1. failed 상태 보존 테스트
    payload_failed = {
        "status": "preview",
        "display_message": "문제가 발견되었습니다.",
        "tts": {
            "status": "failed",
            "provider": "edge_tts",
            "error_code": "TTS_TIMEOUT",
        }
    }
    response_failed = pipeline._attach_tts(agent="process_optimizer", payload=payload_failed)
    assert response_failed["tts"]["status"] == "failed"
    assert response_failed["tts"]["error_code"] == "TTS_TIMEOUT"
    assert service.requests == []  # 합성 서비스가 호출되지 않아야 함

    # 2. disabled 상태 보존 테스트
    payload_disabled = {
        "status": "preview",
        "display_message": "문제가 발견되었습니다.",
        "tts": {
            "status": "disabled",
            "provider": "edge_tts",
            "error_code": "conflicting_environment",
        }
    }
    response_disabled = pipeline._attach_tts(agent="process_optimizer", payload=payload_disabled)
    assert response_disabled["tts"]["status"] == "disabled"
    assert response_disabled["tts"]["error_code"] == "conflicting_environment"
    assert service.requests == []  # 합성 서비스가 호출되지 않아야 함
