# operator_guide RAG Runtime Architecture 반영 계획

## 목표

인터뷰에서 확정한 operator_guide 최종 런타임 구조를 RAG 계획 문서에 반영한다.

핵심 메시지는 단순 RAG Q&A가 아니라, 게임 서버 안에서 동작하는 agent runtime 구조다.

```text
Player Question
-> Orchestrator
-> Middleware
-> operator_guide Agent
-> Leaf Agent
-> RAG Retriever Tool
-> PostgreSQL/pgvector
-> Source Formatter Tool
-> LLM
-> Final Answer
```

## 확정 결정

- Middleware는 관측 가능성 + 실행 제어 계층으로 설명한다.
- Tool은 `RAG Retriever Tool`과 `Source Formatter Tool`로 분리한다.
- Leaf Agent는 단계적으로 유지한다.
- confidence가 낮으면 확인 가능한 범위만 답하고 추가 질문을 반환한다.
- memory는 최근 3턴 원문 + session summary memory + confirmed facts 기반 retrieval로 설계한다.
- fallback은 retrieval fallback과 model fallback을 분리한다.
- 최종 응답 metadata에는 traceId, selected agent, selected leaf agent, retrieval, memory, fallback, latency 정보를 포함한다.
- master plan에는 전체 구조를, sprint plan에는 Sprint 8/9 실행 계획을 반영한다.

## 문서 반영 범위

- `operator_guide_rag_master_plan.md`
  - 전체 아키텍처를 agent runtime 기준으로 보강한다.
  - Middleware, Tool, Memory, Fallback, Metadata 구조를 추가한다.
- `operator_guide_rag_sprint_plan.md`
  - Sprint 8을 Runtime Middleware & Tool Integration으로 정리한다.
  - Sprint 9를 Conversation Memory & Fallback Runtime으로 정리한다.
  - 기존 Session Memory 항목은 Sprint 9 이후 검증/고도화 항목으로 조정한다.

## 검증 기준

- `middleware`, `RAG Retriever Tool`, `Source Formatter Tool`, `confirmed facts`, `traceId`가 문서에 포함된다.
- sprint plan에서 Sprint 8/9가 runtime 구조와 memory/fallback 구조를 명확히 설명한다.
- PDF 파일을 다시 생성한다.

## 작업 로그

- 2026-06-10: 인터뷰 결정사항을 기준으로 runtime architecture 반영 계획을 작성했다.
- 2026-06-10: `operator_guide_rag_master_plan.md`에 runtime middleware, tool 분리, leaf agent 유지 전략, memory, fallback, 최종 metadata 구조를 추가했다.
- 2026-06-10: `operator_guide_rag_sprint_plan.md`에서 Sprint 8을 Runtime Middleware & Tool Integration, Sprint 9를 Conversation Memory & Fallback Runtime으로 정리했다.
- 2026-06-10: LLM-based Context Need Classifier와 Current Game State Tool 흐름을 master/sprint plan에 추가했다.

## 트러블슈팅 로그

- 2026-06-10: 이전 PDF 생성 시 브라우저 file URL에서 경로를 못 찾는 문제가 있었다. PDF 파일은 실제로 생성되었지만, 확인용으로 짧은 경로 복사본을 제공했다.
- 2026-06-10: 현재 게임 상태 조회 여부가 rule-based keyword matching처럼 보이지 않도록, LLM structured JSON decision으로 판단한다고 명시했다.
