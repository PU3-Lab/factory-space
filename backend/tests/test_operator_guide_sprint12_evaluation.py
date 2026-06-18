"""Sprint 12 Memory Evaluation & Runtime Tuning 테스트.

초보자용 설명:
    이 테스트 파일은 Sprint 12의 핵심 완료 기준을 자동 검증합니다.
    1. 플레이어 질문에서 "전력 정상", "컨베이어 멈춤" 등의 확정 사실을 올바르게 파싱하는지(extract_confirmed_facts)
    2. 중복 사실 방지 및 상충 정보 업데이트(전력 정상 <-> 부족) 시 summary version이 잘 늘어나는지
    3. 세션 내 멀티턴 대화 진행 시 RAG 검색기에 최종적으로 facts가 조립된 확장 쿼리가 잘 들어가는지
    을 단위 테스트와 파이프라인 통합 테스트를 통해 검증합니다.
"""

from __future__ import annotations

from dataclasses import dataclass

from agents.operator_guide.service import ManualQAService
from agents.operator_guide.session_memory import (
    OperatorGuideSessionMemory,
    extract_confirmed_facts,
)
from agents.pipeline import AgentPipeline
from tests.harness import StubLLM, assert_agent_response, top_agent_decision


@dataclass(frozen=True)
class FakeSubQuestionResult:
    question: str


@dataclass(frozen=True)
class FakeRagRuntimeResult:
    context_text: str
    is_multi_question: bool
    sub_question_results: list[FakeSubQuestionResult]
    metadata: dict[str, object]


class FakeRagRuntime:
    """테스트용 RAG 런타임 스텁.

    초보자용 설명:
        실제 DB를 찔러 검색하지 않고, 검색기(retriever)에 어떤 질의가 들어왔는지
        기록해 둠으로써 쿼리 확장(Query Expansion)이 정상 동작했는지 확인합니다.
    """

    def __init__(self) -> None:
        self.calls: list[str] = []

    def retrieve(self, question: str) -> FakeRagRuntimeResult:
        self.calls.append(question)
        return FakeRagRuntimeResult(
            context_text=f"fake context for: {question}",
            is_multi_question=False,
            sub_question_results=[FakeSubQuestionResult(question=question)],
            metadata={
                "sub_question_count": 1,
                "max_sub_questions": 3,
                "truncated": False,
                "confidence_counts": {"high": 1, "medium": 0, "low": 0},
            },
        )


def test_extract_confirmed_facts() -> None:
    """사용자 질문에서 게임 상태 사실을 추출하는 규칙을 검증합니다."""

    # 전력 정상/부족 확인
    assert "전력 상태는 정상" in extract_confirmed_facts("전력은 정상인데?")
    assert "전력 부족" in extract_confirmed_facts("전력이 부족해서 그런가?")

    # 컨베이어 멈춤 확인
    assert "컨베이어가 멈춤" in extract_confirmed_facts(
        "컨베이어 벨트가 멈췄는데 어쩌지"
    )

    # 저장고 비어있음/가득참 확인
    assert "출력 저장고는 비어 있음" in extract_confirmed_facts(
        "출력 보관함은 비어 있어요"
    )
    assert "출력 저장고가 가득 참" in extract_confirmed_facts("저장고가 가득 찼어")

    # 라인 정체 확인
    assert "라인 정체" in extract_confirmed_facts("라인이 꽉 막혔는데")


def test_session_memory_fact_updates_and_contradiction() -> None:
    """세션 메모리의 사실 정보 추가, 중복 방지 및 상충 정보 업데이트 규칙을 검증합니다."""

    memory = OperatorGuideSessionMemory()
    session_id = "test-session-fact-update"

    # 최초 질문 시 사실 추가
    memory.update_facts_from_question(session_id, "컨베이어가 멈췄는데 뭘 확인해야 해?")
    assert memory.confirmed_facts(session_id) == ["컨베이어가 멈춤"]
    assert memory.summary_version(session_id) == 1

    # 무관한 질문 시 변화 없음
    memory.update_facts_from_question(session_id, "제련기가 뭐야?")
    assert memory.confirmed_facts(session_id) == ["컨베이어가 멈춤"]
    assert memory.summary_version(session_id) == 1

    # 새로운 사실 추가 및 버전 증가
    memory.update_facts_from_question(session_id, "전력은 정상인데?")
    assert "전력 상태는 정상" in memory.confirmed_facts(session_id)
    assert memory.summary_version(session_id) == 2

    # 중복 사실 추가 시 무시 및 버전 유지
    memory.update_facts_from_question(session_id, "전력 정상 상태야")
    assert memory.summary_version(session_id) == 2

    # 상충 사실 추가 시 교체 및 버전 증가
    memory.update_facts_from_question(session_id, "갑자기 전력이 부족하다고 뜨네")
    facts = memory.confirmed_facts(session_id)
    assert "전력 부족" in facts
    assert "전력 상태는 정상" not in facts
    assert memory.summary_version(session_id) == 3


def test_pipeline_memory_context_and_query_expansion_integration() -> None:
    """파이프라인 실행 중 메모리 연동 및 RAG 확장 쿼리 질의를 통합 검증합니다.

    데이터 흐름:
        1. 첫 번째 턴: "컨베이어가 멈췄어"
           -> RAG 질의: "컨베이어가 멈췄어 컨베이어가 멈춤" (현재 팩트 반영)
           -> 세션 메모리: confirmed_facts = ["컨베이어가 멈춤"]
        2. 두 번째 턴: "전력은 정상인데?"
           -> RAG 질의: "전력은 정상인데? 컨베이어가 멈춤 전력 상태는 정상" (누적 팩트 반영)
           -> 세션 메모리: confirmed_facts = ["컨베이어가 멈춤", "전력 상태는 정상"]
        3. 세 번째 턴: "그럼 다음은?"
           -> RAG 질의: "그럼 다음은? 컨베이어가 멈춤 전력 상태는 정상" (컨텍스트 기반 RAG 검색 성공!)
    """

    llm = StubLLM(
        [
            # 1턴: 라우팅 결정 & 최종 답변
            top_agent_decision("operator_guide"),
            (
                '{"final_answer":"컨베이어 정지 시에는 전력과 출력 저장고를 확인하세요.",'
                '"actions":[],"question":"컨베이어가 멈췄어","topic":"troubleshooting"}'
            ),
            # 2턴: 라우팅 결정 & 최종 답변
            top_agent_decision("operator_guide"),
            (
                '{"final_answer":"전력이 정상이면 출력 저장 공간이 가득 찼는지 확인하세요.",'
                '"actions":[],"question":"전력은 정상인데?","topic":"troubleshooting"}'
            ),
            # 3턴: 라우팅 결정 & 최종 답변
            top_agent_decision("operator_guide"),
            (
                '{"final_answer":"저장 공간도 비어 있다면 벨트 방향 연결을 확인하세요.",'
                '"actions":[],"question":"그럼 다음은?","topic":"troubleshooting"}'
            ),
        ]
    )

    fake_rag = FakeRagRuntime()
    # 글로벌 RAG 런타임 스텁 등록
    old_rag = ManualQAService._global_rag_runtime
    ManualQAService.set_global_rag_runtime(fake_rag)

    try:
        pipeline = AgentPipeline(llm=llm)
        session_id = "sprint12-session"

        # 1턴 실행
        res1 = pipeline.run(
            {
                "type": "agent.request",
                "request_id": "req-1",
                "session_id": session_id,
                "agent": "operator_guide",
                "payload": {
                    "question": "컨베이어가 멈췄어",
                    "sub_agent": "operator_guide.troubleshooter",
                },
            }
        )
        assert_agent_response(
            res1, agent="operator_guide", sub_agent="operator_guide.troubleshooter"
        )
        mem1 = res1["payload"]["metadata"]["memory"]
        assert mem1["used"] is True
        assert mem1["confirmed_facts"] == ["컨베이어가 멈춤"]
        assert mem1["summary_version"] == 1

        # 2턴 실행
        res2 = pipeline.run(
            {
                "type": "agent.request",
                "request_id": "req-2",
                "session_id": session_id,
                "agent": "operator_guide",
                "payload": {
                    "question": "전력은 정상인데?",
                    "sub_agent": "operator_guide.troubleshooter",
                },
            }
        )
        assert_agent_response(
            res2, agent="operator_guide", sub_agent="operator_guide.troubleshooter"
        )
        mem2 = res2["payload"]["metadata"]["memory"]
        assert mem2["used"] is True
        assert "컨베이어가 멈춤" in mem2["confirmed_facts"]
        assert "전력 상태는 정상" in mem2["confirmed_facts"]
        assert mem2["summary_version"] == 2

        # 3턴 실행 ("그럼 다음은?")
        res3 = pipeline.run(
            {
                "type": "agent.request",
                "request_id": "req-3",
                "session_id": session_id,
                "agent": "operator_guide",
                "payload": {
                    "question": "그럼 다음은?",
                    "sub_agent": "operator_guide.troubleshooter",
                },
            }
        )
        assert_agent_response(
            res3, agent="operator_guide", sub_agent="operator_guide.troubleshooter"
        )
        mem3 = res3["payload"]["metadata"]["memory"]
        assert mem3["used"] is True
        assert mem3["summary_version"] == 2  # 신규 사실 없으므로 버전 고정

        # LLM에 전달된 프롬프트 내용에 [CONFIRMED_FACTS] 섹션이 올바르게 주입되었는지 확인합니다.
        # 1턴: 답변(0)
        prompt_1 = llm.prompt_messages[0][1]["content"]
        assert "[CONFIRMED_FACTS]" in prompt_1
        assert "- 컨베이어가 멈춤" in prompt_1

        # 2턴: 답변(1)
        prompt_2 = llm.prompt_messages[1][1]["content"]
        assert "[CONFIRMED_FACTS]" in prompt_2
        assert "- 컨베이어가 멈춤" in prompt_2
        assert "- 전력 상태는 정상" in prompt_2

        # 3턴: 답변(2)
        prompt_3 = llm.prompt_messages[2][1]["content"]
        assert "[CONFIRMED_FACTS]" in prompt_3
        assert "- 컨베이어가 멈춤" in prompt_3
        assert "- 전력 상태는 정상" in prompt_3

        # RAG 검색기에 들어간 최종 질의들 검증 (Query Expansion 적용 완료 확인!)
        # 중복 호출 캐싱이 적용되어 턴당 1번씩 총 3번만 호출됩니다.
        assert len(fake_rag.calls) == 3
        # 1턴 쿼리: 질문 + 추출 팩트
        assert "컨베이어가 멈췄어" in fake_rag.calls[0]
        assert "컨베이어가 멈춤" in fake_rag.calls[0]

        # 2턴 쿼리: 질문 + 누적 팩트
        assert "전력은 정상인데?" in fake_rag.calls[1]
        assert "컨베이어가 멈춤" in fake_rag.calls[1]
        assert "전력 상태는 정상" in fake_rag.calls[1]

        # 3턴 쿼리: 질문 + 누적 팩트 ("그럼 다음은?" 질문이 누적 팩트로 보강됨!)
        assert "그럼 다음은?" in fake_rag.calls[2]
        assert "컨베이어가 멈춤" in fake_rag.calls[2]
        assert "전력 상태는 정상" in fake_rag.calls[2]

    finally:
        # 글로벌 RAG 런타임 복구
        ManualQAService.set_global_rag_runtime(old_rag)
