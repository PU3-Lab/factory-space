# operator_guide RAG 아키텍처 요약 문서 작성 계획

## 목표

`operator_guide_rag_master_plan.md` 내용을 바탕으로 발표/공유용 아키텍처 요약 문서를 만든다.

첨부 예시처럼 다음 3단 흐름으로 정리한다.

```text
Routing / Agent Selection
-> operator_guide RAG Runtime
-> Final Response
```

## 작성 범위

- Mermaid 기반 아키텍처 다이어그램
- 각 계층의 역할 요약
- 실제 플레이어 질문이 들어왔을 때의 처리 흐름
- 발표용 30초 요약
- 구현 포인트 요약

## 완료 기준

- master plan의 핵심 흐름이 빠지지 않는다.
- RAG, PostgreSQL/pgvector, Current Game State Tool, Context Need Classifier, Middleware, LLM 답변 생성이 한 장 구조로 보인다.
- 첨부 이미지처럼 routing 계층, operator_guide 내부 계층, final response 계층이 분리된다.

## 작업 로그

- 2026-06-11: master plan 기반 아키텍처 요약 문서 작성 계획을 추가했다.
- 2026-06-11: `operator_guide_rag_architecture_summary.md`에 Mermaid 구조도, 계층별 역할, 실제 질문 처리 흐름, 발표용 30초 요약을 작성했다.

## 트러블슈팅 로그

- 2026-06-11: 상세 sprint 계획 문서와 발표용 구조도가 섞이지 않도록 별도 summary 문서로 분리한다.
- 2026-06-11: 첨부 이미지처럼 routing/runtime/final response 계층이 보이도록 Mermaid subgraph를 3단 구조로 구성했다.
