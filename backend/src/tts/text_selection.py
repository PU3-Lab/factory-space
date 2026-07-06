"""에이전트 응답 페이로드에서 플레이어 노출용 문장을 선택하고 정제하는 모듈.

초보자용 설명:
    에이전트(AI 가이드 또는 공정 최적화 도구)의 출력 페이로드에는 마크다운 기호(**, ` 등)나
    디버깅용 데이터가 섞여 있을 수 있습니다. 이 모듈은 그러한 불필요한 서식을 지우고,
    목소리로 재생하기 적합한 순수 텍스트 문장만 골라내어 적절한 길이로 잘라줍니다.
"""

from __future__ import annotations

import re
from typing import Any


def _clean_markdown(text: str) -> str:
    """마크다운 기호 및 중복 공백을 정리합니다."""
    # **주의:** 와 같은 굵은 글씨 기호 제거
    text = re.sub(r"\*\*([^*]+)\*\*", r"\1", text)
    text = re.sub(r"\*([^*]+)\*", r"\1", text)
    # 백틱 기호 제거
    text = text.replace("`", "")
    # 줄 바꿈 및 다중 공백 단일 공백으로 치환
    text = re.sub(r"\s+", " ", text)
    return text.strip()


def select_tts_text(agent: str, payload: dict[str, Any], max_chars: int = 600) -> str | None:
    """에이전트 타입과 페이로드 형식에 맞게 TTS 대상 문장을 선택하고 정제합니다."""
    raw_text: str | None = None

    # 4차 재리뷰 보강: payload.tts.text 입력 힌트가 들어온 경우 최우선순위로 선택
    if isinstance(payload.get("tts"), dict):
        raw_text = payload["tts"].get("text")

    if not raw_text:
        if agent == "operator_guide":
            # operator_guide는 final_answer -> answer -> text 순으로 대상 추출
            raw_text = (
                payload.get("final_answer")
                or payload.get("answer")
                or payload.get("text")
            )

        elif agent == "process_optimizer":
            # 7차 재리뷰 보강: process_optimizer는 display/TTS 일치 계약에 따라 display_message만 사용
            raw_text = payload.get("display_message")

    if not raw_text:
        return None

    # 마크다운 및 서식 정리
    cleaned = _clean_markdown(str(raw_text))
    if not cleaned:
        return None

    # 글자 수 제한 적용
    if len(cleaned) > max_chars:
        cleaned = cleaned[:max_chars]

    return cleaned
