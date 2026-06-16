# operator_guide 시스템 가이드 최신화 계획

## 목적

기존 `docs/manual_qa_operator_guide_system_guide.md`는 초기 Manual Q&A 프로토 기준으로 작성되어 있다. 현재 구현은 LLM, RAG, PostgreSQL + pgvector, 현재 게임 상태 판단, memory, middleware, fallback까지 포함하므로 문서를 최신 구조로 교체한다.

## 수정 범위

- `docs/manual_qa_operator_guide_system_guide.md`
  - 전체 실행 흐름
  - LLM 사용 지점
  - RAG/embedding/pgvector 흐름
  - 현재 게임 상태 판단과 요청 방식
  - 미들웨어와 metadata
  - ToolNode와 현재 구현의 tool 사용 범위
  - fallback 처리
  - 입력/출력 JSON 예시
  - 시연/디버깅 체크 포인트

## 완료 기준

- 현재 코드 기준으로 operator_guide가 어떻게 구성되어 있는지 한 문서에서 설명된다.
- "현재 상태는 백엔드가 직접 읽는가, Unreal이 보내는가"가 명확히 설명된다.
- LangGraph ToolNode와 service 내부 CurrentGameStateTool의 차이가 구분된다.
- 발표/포트폴리오에서 그대로 설명 가능한 문장으로 정리된다.

## 작업 로그

- 2026-06-16: `AgentPipeline`, `OperatorGuideAgent`, leaf agents, `ManualQAService`, `ContextNeedClassifier`, `ManualQAPromptBuilder`, `ManualRagRetriever`, `MultiQuestionRagRetriever`, `ToolNode`, LLM fallback 코드를 확인했다.

## 트러블슈팅 로그

- 2026-06-16: 기존 문서가 "LLM/RAG/pgvector/player_state 미사용"이라고 되어 있어 현재 구현과 충돌했다. 최신 구현 기준으로 전체 문서를 교체하기로 했다.
