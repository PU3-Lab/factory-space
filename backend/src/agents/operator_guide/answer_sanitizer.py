"""operator_guide가 플레이어에게 보여줄 답변 문장을 다듬는 작은 보정기.

초보자용 설명:
    LLM은 프롬프트를 잘 따라도 가끔 "분쇄기(분쇄기)"처럼 같은 이름을
    반복하거나 "분말/톱밥"처럼 UI 말풍선에서 딱딱해 보이는 표현을 만들 수
    있습니다. 이 파일은 답변의 의미를 바꾸지 않고, 플레이어가 읽는
    final_answer 문장만 자연스럽게 정리합니다.
"""

from __future__ import annotations

import re
from typing import Any

_SENTENCE_BOUNDARY_RE = re.compile(r"(?<=[.!?。！？])\s+")

_DUPLICATE_DISPLAY_NAME_RE = re.compile(
    r"(?<![0-9A-Za-z_가-힣])([0-9A-Za-z_가-힣]+)\(\1\)"
)
_RAW_ID_PAREN_RE = re.compile(
    r"\s*\((?:equipment|resource|recipe|issue|action)_[^)]+\)"
)
_HANGUL_SLASH_PAIR_RE = re.compile(r"([가-힣]{1,12})\s*/\s*([가-힣]{1,12})")


def sanitize_operator_guide_response_payload(payload: dict[str, Any]) -> dict[str, Any]:
    """operator_guide 응답 payload의 플레이어 노출 문장만 보정합니다.

    초보자용 설명:
        payload는 최종 WebSocket 응답의 본문입니다. 여기서 final_answer는
        Unreal NPC 말풍선에 그대로 표시되므로, 내부 ID나 기계적인 표기를
        줄여 플레이어가 읽기 쉬운 문장으로 만듭니다.
    """

    final_answer = payload.get("final_answer")
    if not isinstance(final_answer, str):
        return payload

    sanitized = sanitize_player_facing_answer(final_answer)
    if sanitized == final_answer:
        return payload

    return {
        **payload,
        "final_answer": sanitized,
    }


def sanitize_player_facing_answer(answer: str) -> str:
    """플레이어가 보는 답변에서 반복 표기와 딱딱한 구분자를 정리합니다."""

    text = answer.replace("**", "")
    text = _RAW_ID_PAREN_RE.sub("", text)
    text = _DUPLICATE_DISPLAY_NAME_RE.sub(r"\1", text)
    text = _HANGUL_SLASH_PAIR_RE.sub(r"\1이나 \2", text)
    text = re.sub(r"[ \t]{2,}", " ", text)
    return _format_dialogue_paragraphs(text.strip())


def _format_dialogue_paragraphs(answer: str) -> str:
    """NPC 대화창에서 읽기 쉽도록 짧은 답변도 첫 문장 뒤에 문단을 나눕니다."""

    if "\n" in answer:
        return answer

    sentences = [
        sentence.strip()
        for sentence in _SENTENCE_BOUNDARY_RE.split(answer)
        if sentence.strip()
    ]
    if len(sentences) < 2:
        return answer

    return f"{sentences[0]}\n\n{' '.join(sentences[1:])}"
