# operator_guide RAG Sprint 8-3 Runtime Integration Plan

## 목표

Sprint 8-3에서는 Sprint 8-1/8-2에서 만든 multi-question RAG 검색 결과를 operator_guide의 prompt/service 흐름에 연결한다.

핵심 목표는 다음과 같다.

```text
player question
-> Question Decomposer
-> MultiQuestionRagRetriever
-> sub-question별 RAG 검색 결과
-> prompt context에 RAG context 포함
-> 최종 응답 metadata에 retrieval 정보 일부 포함
```

## 포함 범위

- `ManualQAService`에서 RAG runtime retriever를 선택적으로 받을 수 있게 한다.
- RAG runtime이 있으면 CSV context와 함께 RAG context를 prompt builder에 전달한다.
- prompt에는 RAG 검색 근거가 별도 섹션으로 포함된다.
- metadata에는 retrieval 요약을 포함한다.
- multi-question 여부, sub-question 수, confidence count를 metadata로 제공한다.
- 초보자용 한글 docstring을 보강한다.

## 제외 범위

- 실제 DB store와 embedding provider를 service에서 자동 생성하는 wiring
- prompt injection guardrail
- Unreal 최종 UI 표시
- 전체 WebSocket smoke test

위 제외 범위는 Sprint 8.5 이후에 진행한다.

## 테스트 전략

사용자와 합의한 기준을 따른다.

```text
개발 중:
Sprint 8-3 테스트만 실행

구현 완료 직후:
prompt/service + retriever/decomposer 주변 테스트 실행

커밋/PR 직전:
현재까지 연결된 RAG 테스트 묶음 실행
```

## 작업 로그

- 2026-06-15: Sprint 8-3 목표를 multi-question RAG 결과의 prompt/service 연결로 정했다.
- 2026-06-15: RED 단계에서 `ManualQAService(rag_runtime=...)` 인자를 아직 받지 않아 테스트가 실패하는 것을 확인했다.
- 2026-06-15: `ManualQAService`에 선택적 `rag_runtime` 주입 경계를 추가했다.
- 2026-06-15: RAG context와 retrieval metadata가 prompt builder에 전달되도록 `ManualQAPromptContext`를 확장했다.
- 2026-06-15: `ManualQAResult.to_metadata()`에 retrieval 요약을 포함했다.

## 트러블슈팅 로그

- 2026-06-15: 실제 DB/embedding provider 자동 wiring은 환경 의존성이 크므로 이번 단계에서는 의존성 주입 가능한 runtime 경계를 먼저 만든다.
- 2026-06-15: 기존 CSV-only 흐름을 깨지 않기 위해 `rag_runtime` 기본값을 `None`으로 두고, 주입된 경우에만 RAG prompt section을 추가했다.
