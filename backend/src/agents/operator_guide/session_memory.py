"""operator_guide가 세션 안의 최근 대화를 기억하기 위한 메모리 저장소.

초보자용 설명:
    여기서 말하는 memory는 장기 저장 DB가 아니라, 서버가 켜져 있는 동안만
    유지되는 간단한 in-memory 저장소입니다. 같은 session_id로 들어온 질문과
    답변을 최근 몇 턴만 보관해서, "그럼?", "방금 말한 장비" 같은 후속 질문을
    LLM이 이해할 수 있도록 prompt에 짧게 넣어줍니다.

    개인정보나 장기 사용자 취향을 저장하는 기능은 아니며, Sprint 9에서는
    플레이어 질문 흐름을 자연스럽게 만드는 최소 기능만 담당합니다.
"""

from __future__ import annotations

from dataclasses import dataclass

OPERATOR_GUIDE_RECENT_CONVERSATION_KEY = "operator_guide_recent_conversation"


@dataclass(frozen=True)
class OperatorGuideMemoryTurn:
    """한 번의 플레이어 질문과 operator_guide 답변을 묶어 저장한다."""

    question: str
    answer: str

    def to_prompt_dict(self) -> dict[str, str]:
        """prompt builder가 읽기 쉬운 dict 형태로 변환한다."""

        return {
            "question": self.question,
            "answer": self.answer,
        }


class OperatorGuideSessionMemory:
    """session_id별 최근 operator_guide 대화를 짧게 보관한다.

    초보자용 설명:
        session_id는 같은 플레이어 대화 흐름을 구분하는 값입니다.
        이 클래스는 session_id를 key로 사용해서 최근 질문/답변 몇 개만 저장합니다.
        max_turns를 넘으면 오래된 대화부터 버려서 prompt가 너무 길어지지 않게 합니다.
    """

    def __init__(self, max_turns: int = 4) -> None:
        self.max_turns = max_turns
        self._turns_by_session: dict[str, list[OperatorGuideMemoryTurn]] = {}

    def recent_turns(self, session_id: str | None) -> list[OperatorGuideMemoryTurn]:
        """해당 세션의 최근 대화 turn을 오래된 순서대로 반환한다."""

        if not session_id:
            return []
        return list(self._turns_by_session.get(session_id, []))

    def remember(
        self,
        session_id: str | None,
        *,
        question: str,
        answer: str,
    ) -> None:
        """최종 답변이 만들어진 뒤 질문과 답변을 최근 대화로 저장한다."""

        if not session_id or not question or not answer:
            return

        turns = self._turns_by_session.setdefault(session_id, [])
        turns.append(OperatorGuideMemoryTurn(question=question, answer=answer))
        if len(turns) > self.max_turns:
            del turns[: len(turns) - self.max_turns]
