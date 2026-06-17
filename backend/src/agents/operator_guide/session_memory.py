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


def extract_confirmed_facts(question: str) -> list[str]:
    """플레이어의 질문에서 확정/확인된 게임 상태 사실(confirmed facts)을 추출합니다.

    초보자용 설명:
        플레이어가 "전력은 정상인데?"라고 질문하면, 플레이어는 이미 전력을 확인했고
        그것이 정상임을 우리에게 '확정 사실'로 알려준 것입니다.
        이 함수는 질문에서 이러한 키워드 패턴을 매칭하여 현재 대화 세션의
        상태 정보로 누적할 수 있는 사실(Fact) 문자열 리스트를 반환합니다.
        LLM이 추론하여 얻은 원인이나 가능성은 확정 사실로 취급하지 않으므로,
        순수하게 플레이어가 직접 언급하여 확인해 준 내용만 여기에 담깁니다.

    데이터 흐름:
        입력 질문(str) -> 키워드 패턴 매칭 -> 확인된 사실 목록(list[str]) 반환
    """

    facts: list[str] = []
    q = question.lower()

    # 1. 전력 상태 관련 확인 사실
    if "전력" in q:
        if any(x in q for x in ["정상", "ok", "오케이", "문제 없", "연결"]):
            facts.append("전력 상태는 정상")
        elif any(x in q for x in ["부족", "차단", "끊", "안 들어"]):
            facts.append("전력 부족")

    # 2. 컨베이어 벨트 상태 관련 확인 사실
    if "컨베이어" in q or "벨트" in q:
        if any(x in q for x in ["멈췄", "멈춤", "안 움직", "정지", "막혔", "막힘"]):
            facts.append("컨베이어가 멈춤")

    # 3. 출력 공간 / 저장고 상태 관련 확인 사실
    if any(x in q for x in ["출력", "저장고", "보관함", "창고"]):
        if any(x in q for x in ["비어", "비었"]):
            facts.append("출력 저장고는 비어 있음")
        elif any(x in q for x in ["가득", "꽉", "포화"]):
            facts.append("출력 저장고가 가득 참")

    # 4. 라인 정체 관련 확인 사실
    if "라인" in q or "벨트" in q:
        if any(x in q for x in ["막", "정체"]):
            facts.append("라인 정체")

    return facts


class OperatorGuideSessionMemory:
    """session_id별 최근 operator_guide 대화 및 확인된 사실을 보관한다.

    초보자용 설명:
        session_id는 같은 플레이어 대화 흐름을 구분하는 값입니다.
        이 클래스는 최근 대화 내용(max_turns)과 플레이어가 직접 언급해 확인된
        게임 상태 정보(confirmed_facts)를 함께 보관합니다.
        이를 통해 "그럼 다음은?" 같은 후속 질문에서도 이전 상태 맥락을 이어갈 수 있습니다.
    """

    def __init__(self, max_turns: int = 4) -> None:
        self.max_turns = max_turns
        self._turns_by_session: dict[str, list[OperatorGuideMemoryTurn]] = {}
        self._confirmed_facts_by_session: dict[str, list[str]] = {}
        self._summary_version_by_session: dict[str, int] = {}

    def recent_turns(self, session_id: str | None) -> list[OperatorGuideMemoryTurn]:
        """해당 세션의 최근 대화 turn을 오래된 순서대로 반환한다."""

        if not session_id:
            return []
        return list(self._turns_by_session.get(session_id, []))

    def confirmed_facts(self, session_id: str | None) -> list[str]:
        """해당 세션에서 플레이어가 확인해 준 사실 목록을 반환한다."""

        if not session_id:
            return []
        return list(self._confirmed_facts_by_session.get(session_id, []))

    def summary_version(self, session_id: str | None) -> int:
        """해당 세션의 메모리 요약 버전 번호를 반환한다."""

        if not session_id:
            return 0
        return self._summary_version_by_session.get(session_id, 0)

    def update_facts_from_question(self, session_id: str | None, question: str) -> None:
        """플레이어 질문에서 확인 사실을 추출하고 업데이트 규칙에 맞게 반영한다.

        초보자용 설명:
            새로운 확인 사실이 있으면 기존 사실에 추가하되,
            상충되는 사실(예: "전력 부족" vs "전력 상태는 정상")이 들어오는 경우
            기존 상태를 대체하고 요약 버전을 증가시킵니다.
        """

        if not session_id or not question:
            return

        new_facts = extract_confirmed_facts(question)
        if not new_facts:
            return

        existing = self._confirmed_facts_by_session.setdefault(session_id, [])
        version = self._summary_version_by_session.get(session_id, 0)

        updated = list(existing)
        changed = False

        for fact in new_facts:
            # 상충되는 전력 사실에 대한 업데이트 처리
            if fact == "전력 상태는 정상":
                if "전력 부족" in updated:
                    updated.remove("전력 부족")
                    changed = True
            elif fact == "전력 부족":
                if "전력 상태는 정상" in updated:
                    updated.remove("전력 상태는 정상")
                    changed = True

            # 중복되지 않는 새로운 사실 추가
            if fact not in updated:
                updated.append(fact)
                changed = True

        if changed:
            self._confirmed_facts_by_session[session_id] = updated
            self._summary_version_by_session[session_id] = version + 1

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
