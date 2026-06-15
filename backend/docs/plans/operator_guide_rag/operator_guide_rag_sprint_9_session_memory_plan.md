# operator_guide RAG Sprint 9 Session Memory Plan

## 목표

Sprint 9에서는 operator_guide가 같은 세션 안의 최근 대화를 기억해 후속 질문을 더 자연스럽게 처리하도록 한다.

예시:

```text
플레이어: 분쇄기가 뭐야?
NPC: 분쇄기는 고체 자원을 분말로 바꾸는 장비예요...

플레이어: 그럼 철괴 만들 때도 필요해?
NPC: 방금 말한 분쇄기 기준으로 보면, 철괴 제작에는 보통 제련기 흐름을 먼저 확인하는 게 좋아요...
```

## 포함 범위

- operator_guide 전용 in-memory session memory 추가
- 최근 질문/답변을 최대 3~5턴만 보관
- prompt에 `RECENT_CONVERSATION_CONTEXT` 섹션 추가
- 후속 질문에서 최근 대화 맥락을 LLM prompt에 제공
- metadata에 memory 사용 여부와 turn 수 기록
- 초보자용 한글 docstring 추가

## 제외 범위

- PostgreSQL에 대화 기록 영구 저장
- 사용자별 장기 메모리
- 전체 대화 요약 LLM 호출
- 개인정보/계정 단위 메모리 정책
- Unreal UI 변경

위 항목은 장기 memory sprint에서 별도로 진행한다.

## 설계 방향

```text
agent.request
-> session_id 확인
-> operator_guide service
-> session memory에서 최근 turn 조회
-> ManualQAPromptContext에 recent conversation 주입
-> prompt_builder가 RECENT_CONVERSATION_CONTEXT 생성
-> LLM 답변 생성
-> 최종 응답 이후 question/final_answer를 session memory에 저장
```

## 메모리 범위

초기 구현은 세션별 최근 4턴을 기본값으로 둔다.

```text
session_id: agent-test-session
turns:
1. user question / assistant answer
2. user question / assistant answer
3. user question / assistant answer
4. user question / assistant answer
```

## 테스트 전략

사용자가 합의한 테스트 기준을 따른다.

```text
개발 중:
Sprint 9 테스트만 실행

구현 완료 직후:
prompt/service 주변 테스트 실행

커밋/PR 직전:
현재까지 연결된 RAG 테스트 묶음 실행
```

## 작업 로그

- 2026-06-15: Sprint 9 범위를 operator_guide 전용 in-memory session memory로 정의했다.
- 2026-06-15: RED 테스트로 같은 `session_id`의 두 번째 operator_guide 질문 prompt에 최근 대화가 없을 때 실패하는지 확인했다.
- 2026-06-15: `OperatorGuideSessionMemory`를 추가해 session_id별 최근 질문/답변을 최대 4턴 저장하도록 구현했다.
- 2026-06-15: `ManualQAPromptContext`와 `ManualQAPromptBuilder`에 `RECENT_CONVERSATION_CONTEXT` 섹션을 추가했다.
- 2026-06-15: operator_guide leaf agent가 `AgentContext.metadata`를 ManualQAService로 넘기도록 연결했다.
- 2026-06-15: pipeline runtime에서 prompt 생성 전 최근 대화를 주입하고, 정상 응답 이후 질문/답변을 memory에 저장하도록 연결했다.

## 트러블슈팅 로그

- 2026-06-15: PostgreSQL 영구 저장까지 한 번에 구현하면 범위가 커지므로, 먼저 런타임 세션 memory로 후속 질문 UX를 검증하기로 했다.
- 2026-06-15: response cache lookup은 memory 주입 전에 실행되므로 cache key에는 memory context를 포함하지 않도록 유지했다. 이렇게 해야 같은 payload/context 기준 cache 정책이 기존과 크게 달라지지 않는다.
- 2026-06-15: fallback 응답에도 memory metadata를 붙일 수 있도록 parse/fallback metadata 생성 양쪽에 같은 구조를 반영했다.

## 검증 로그

- 2026-06-15: Sprint 9 단독 RED `uv run pytest tests/test_operator_guide_session_memory.py -q` 실패 확인.
- 2026-06-15: Sprint 9 단독 GREEN `uv run pytest tests/test_operator_guide_session_memory.py -q` 통과.
- 2026-06-15: 주변 테스트 `uv run pytest tests/test_operator_guide_session_memory.py tests/test_operator_guide_rag_runtime_integration.py tests/test_operator_guide_prompt_injection_guard.py tests/test_message_router.py::test_pipeline_operator_guide_uses_llm_prompt_with_manual_csv_evidence tests/test_message_router.py::test_pipeline_uses_prompt_based_operator_guide_sub_agent_routing -q` 통과.
- 2026-06-15: lint `uv run ruff check src/agents/operator_guide/session_memory.py src/agents/operator_guide/manual_context_builder.py src/agents/operator_guide/prompt_builder.py src/agents/operator_guide/service.py src/agents/operator_guide/machine_help.py src/agents/operator_guide/recipe_explainer.py src/agents/operator_guide/troubleshooter.py src/agents/pipeline/runtime.py src/agents/pipeline/state.py tests/test_operator_guide_session_memory.py` 통과.
- 2026-06-15: 커밋 전 RAG 테스트 묶음 `uv run pytest tests/test_operator_guide_rag_documents.py tests/test_operator_guide_rag_embedding.py tests/test_operator_guide_rag_ingestion.py tests/test_operator_guide_rag_pgvector_schema.py tests/test_operator_guide_rag_upsert.py tests/test_operator_guide_rag_retriever.py tests/test_operator_guide_question_decomposer.py tests/test_operator_guide_multi_question_rag_retriever.py tests/test_operator_guide_rag_runtime_integration.py tests/test_operator_guide_prompt_injection_guard.py tests/test_operator_guide_session_memory.py tests/test_ingest_manual_rag_script.py -q` 통과.
