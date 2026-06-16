# operator_guide 에이전트 기획서 작성 계획

## 목표

Google Docs 기획서를 참고해 사용자가 담당하는 `operator_guide` 에이전트 부분을 팀 공유용 기획서로 정리한다.

현재 세션에서는 제공된 Google Docs 문서 원문을 직접 읽을 수 없으므로, 저장소 안에 이미 정리된 다음 문서를 기준으로 기획서를 작성한다.

- `backend/docs/plans/operator_guide_rag/operator_guide_rag_master_plan.md`
- `backend/docs/plans/operator_guide_rag/operator_guide_rag_architecture_summary.md`
- `backend/docs/plans/operator_guide_rag/operator_guide_unreal_question_guide_ui_contract.md`
- `backend/docs/plans/operator_guide_rag/operator_guide_rag_sprint_plan.md`
- `docs/01_planning/material_generation_agent.md`

## 작성 위치

최종 기획서는 팀 공유용 planning 문서 위치에 둔다.

```text
docs/01_planning/operator_guide_agent.md
```

## 포함 범위

- operator_guide 에이전트의 목적
- 플레이어 질문 처리 흐름
- Orchestrator와 operator_guide의 역할 분리
- Leaf Agent 분류
- RAG 검색 구조
- PostgreSQL + pgvector 저장 구조
- CSV 변경 시 ingestion 흐름
- 현재 게임 상태가 필요한 질문 판단
- LLM 답변 생성 정책
- prompt injection guardrail
- confidence / fallback / memory 정책
- Unreal 질문 가이드 UI 연동
- JSON 입력/출력 예시
- MVP 범위와 제외 범위
- 디버그/평가 운영 방식

## 제외 범위

- Google Docs 원문 직접 편집
- 실제 RAG runtime 코드 변경
- DB migration 또는 embedding 실행
- Unreal UI 구현

## 검증 기준

- 문서가 `operator_guide` 담당 범위를 초보자도 이해할 수 있게 설명한다.
- `material_generation_agent.md`와 유사한 기획서 형식을 따른다.
- Unreal, Backend, 발표/포트폴리오 관점에서 설명 가능한 구조를 갖춘다.
- 외부 Google Docs 접근 제한이 있었음을 문서 작업 로그에 남긴다.

## 작업 로그

- 2026-06-16: Google Docs 링크가 제공되었지만 현재 세션에서 원문을 직접 읽을 수 없어, 저장소 내 operator_guide RAG 문서들을 기준 자료로 삼기로 했다.
- 2026-06-16: operator_guide 담당 범위를 `docs/01_planning/operator_guide_agent.md`에 최종 기획서 형태로 정리하기로 했다.

## 트러블슈팅 로그

- 2026-06-16: Google Docs 원문 접근이 불가한 경우를 대비해, 기존 master plan / architecture summary / Unreal UI contract 문서 기반으로 기획서를 먼저 작성하고 이후 원문 공유 시 보정하는 방식으로 결정했다.
