"""operator_guide 질문을 여러 sub-question으로 나누는 모듈.

초보자용 설명:
    플레이어는 한 번에 여러 질문을 섞어서 물어볼 수 있다.
    예를 들어 "분쇄기가 뭐야? 그리고 철괴는 어떻게 만들어?"는 실제로
    두 개의 질문이다. 이 모듈은 답을 정하지 않고, RAG 검색을 더 정확히
    하기 위해 입력 문장을 작은 질문 단위로 나눈다.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

DEFAULT_MAX_SUB_QUESTIONS = 3


@dataclass(frozen=True)
class DecomposedQuestion:
    """분해된 질문 하나를 표현한다."""

    index: int
    question: str


@dataclass(frozen=True)
class QuestionDecompositionResult:
    """원본 질문과 분해된 질문 목록을 함께 담는 결과 객체."""

    original_question: str
    sub_questions: list[DecomposedQuestion]
    is_multi_question: bool
    metadata: dict[str, object]


def decompose_question(
    question: str,
    *,
    max_sub_questions: int = DEFAULT_MAX_SUB_QUESTIONS,
) -> QuestionDecompositionResult:
    """플레이어 입력을 RAG 검색에 사용할 sub-question 목록으로 나눈다."""

    cleaned_question = question.strip()
    candidates = _split_question_candidates(cleaned_question)
    limited_candidates = candidates[:max_sub_questions]
    sub_questions = [
        DecomposedQuestion(index=index, question=candidate)
        for index, candidate in enumerate(limited_candidates, start=1)
    ]
    return QuestionDecompositionResult(
        original_question=cleaned_question,
        sub_questions=sub_questions,
        is_multi_question=len(candidates) > 1,
        metadata={
            "sub_question_count": len(sub_questions),
            "max_sub_questions": max_sub_questions,
            "truncated": len(candidates) > len(limited_candidates),
        },
    )


def _split_question_candidates(question: str) -> list[str]:
    if not question:
        return []

    marked_question = re.sub(r"\?\s+", "?<SPLIT>", question)
    marked_question = re.sub(
        r"\b(?:and|also|then)\b\s+",
        "<SPLIT>",
        marked_question,
        flags=re.IGNORECASE,
    )
    marked_question = re.sub(
        r"\s*(?:그리고|또|그다음|하고)\s+",
        "<SPLIT>",
        marked_question,
    )
    return [_ensure_question_mark(part.strip()) for part in marked_question.split("<SPLIT>") if part.strip()]


def _ensure_question_mark(question: str) -> str:
    if question.endswith("?"):
        return question
    return f"{question}?"
