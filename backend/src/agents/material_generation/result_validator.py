"""LLM이 제안한 재료의 이름·id_hint를 검증·새니타이징합니다.

속성·희귀도·category·state는 derivation 모듈에서 결정론적으로 산출되어
주입되므로 본 검증기는 자유 텍스트(이름/id_hint)와 후보 개수만 다룬다.
"""

from __future__ import annotations

import re

from agents.material_generation.schemas import MaterialProposal

# 검증 및 변환 시 필터링할 비속어 및 금지 키워드 목록
FORBIDDEN_KEYWORDS = {"trash", "error", "test", "dummy", "fuck", "shit", "invalid"}

FALLBACK_NAME = "미상의 합성물"
MIN_NAME_LEN = 2
MAX_NAME_LEN = 24
MAX_CANDIDATES = 5

# 한글·영숫자·공백·하이픈만 허용
_DISALLOWED_NAME_CHARS = re.compile(r"[^0-9A-Za-z가-힣\s\-]")
_HAS_VALID_CHAR = re.compile(r"[0-9A-Za-z가-힣]")
# id_hint: 소문자·숫자·언더스코어만 허용
_DISALLOWED_ID_CHARS = re.compile(r"[^a-z0-9_]")


class MaterialResultValidator:
    """이름·id_hint 새니타이징 및 후보 개수 제한을 수행합니다."""

    @classmethod
    def _sanitize_name(cls, name: str) -> str:
        """재료 이름 문자열을 정리합니다.

        1. 앞뒤 공백을 제거합니다.
        2. 금지 키워드가 포함될 경우 '합금'으로 치환합니다.
        3. 한글, 영숫자, 공백, 하이픈 외의 모든 특수문자를 제거합니다.
        4. 유효한 글자가 전혀 없거나 길이가 2자 미만인 경우 FALLBACK_NAME으로 대체합니다.
        5. 길이가 24자를 초과할 경우 24자까지 자르고 정리합니다.
        """
        cleaned = name.strip()
        if not cleaned:
            return FALLBACK_NAME

        for term in FORBIDDEN_KEYWORDS:
            pattern = r"(?<![A-Za-z])" + re.escape(term) + r"(?![A-Za-z])"
            cleaned = re.sub(pattern, "합금", cleaned, flags=re.IGNORECASE)

        cleaned = _DISALLOWED_NAME_CHARS.sub("", cleaned).strip()

        if not _HAS_VALID_CHAR.search(cleaned) or len(cleaned) < MIN_NAME_LEN:
            return FALLBACK_NAME
        if len(cleaned) > MAX_NAME_LEN:
            cleaned = cleaned[:MAX_NAME_LEN].strip()
        return cleaned

    @classmethod
    def _sanitize_id_hint(cls, id_hint: str) -> str:
        """재료의 식별자(ID) 힌트 문자열을 규격에 맞춰 정리합니다.

        1. 소문자화하고 금지 키워드를 'alloy'로 치환합니다.
        2. 소문자, 숫자, 언더스코어 외의 비허용 기호를 언더스코어로 변환합니다.
        3. 다수의 언더스코어가 연속될 경우 단일 언더스코어로 결합하고, 앞뒤 언더스코어를 제거합니다.
        4. 정리 후 빈 문자열이 될 경우 기본값 'material'을 반환합니다.
        """
        cleaned = id_hint.strip().lower()
        for term in FORBIDDEN_KEYWORDS:
            pattern = r"(?<![A-Za-z])" + re.escape(term) + r"(?![A-Za-z])"
            cleaned = re.sub(pattern, "alloy", cleaned)
        cleaned = _DISALLOWED_ID_CHARS.sub("_", cleaned)
        cleaned = re.sub(r"_+", "_", cleaned).strip("_")
        return cleaned or "material"

    @classmethod
    def validate_and_correct(cls, proposal: MaterialProposal) -> MaterialProposal:
        """이름·id_hint를 새니타이징하고 후보 개수를 제한합니다.

        또한, 다음 연계 레시피 후보군 목록이 최대 5개를 초과하지 않도록 앞부분 5개로 제한합니다.
        """
        if not proposal.result:
            return proposal

        result = proposal.result
        result.name = cls._sanitize_name(result.name)
        result.id_hint = cls._sanitize_id_hint(result.id_hint)

        if len(result.next_recipe_candidates) > MAX_CANDIDATES:
            result.next_recipe_candidates = result.next_recipe_candidates[
                :MAX_CANDIDATES
            ]

        return proposal
