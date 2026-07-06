from __future__ import annotations

from tts.text_selection import select_tts_text


def test_operator_guide_uses_final_answer() -> None:
    text = select_tts_text(
        "operator_guide",
        {"final_answer": "분쇄기는 원석을 다음 단계 재료로 가공하는 장비입니다."},
    )

    assert text == "분쇄기는 원석을 다음 단계 재료로 가공하는 장비입니다."


def test_process_optimizer_ignores_summary_and_alert_without_display_message() -> None:
    # 7차 재리뷰 보강: display/TTS 일치 계약에 따라 display_message가 없으면 summary나 alert를 우회하여 사용하지 않고 None을 반환해야 함
    text = select_tts_text(
        "process_optimizer",
        {
            "summary": "제련기 출력이 막혀 생산량이 낮습니다.",
            "optimization_alert": {
                "needed": True,
                "problem": "전력이 부족합니다.",
                "recommended_action": "발전기를 추가하세요.",
            },
        },
    )

    assert text is None


def test_text_selection_strips_markdown_and_limits_length() -> None:
    long_text = "**주의:** " + ("가" * 900)

    text = select_tts_text("operator_guide", {"final_answer": long_text}, max_chars=20)

    assert text == "주의: " + ("가" * 16)


def test_process_optimizer_uses_display_message() -> None:
    # 7차 재리뷰 보강: process_optimizer는 display_message 필드값만 매칭하여 사용함
    text = select_tts_text(
        "process_optimizer",
        {
            "display_message": "문제가 발견되지 않았습니다.",
            "summary": "제련기 출력이 막혀 생산량이 낮습니다.",
        },
    )

    assert text == "문제가 발견되지 않았습니다."


def test_uses_tts_text_hint_absolute_priority() -> None:
    # 4차 재리뷰 보강: payload.tts.text 입력 힌트가 들어온 경우 에이전트 종류 무관 최우선순위로 선택
    text = select_tts_text(
        "operator_guide",
        {
            "final_answer": "이것은 최종 답변입니다.",
            "tts": {"text": "이것은 강제 합성 문장입니다."},
        },
    )
    assert text == "이것은 강제 합성 문장입니다."
