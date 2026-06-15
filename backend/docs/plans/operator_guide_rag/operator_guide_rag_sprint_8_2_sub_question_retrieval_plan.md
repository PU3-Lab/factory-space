# operator_guide RAG Sprint 8-2 Sub-question Retrieval Plan

## 목표

Sprint 8-2에서는 Sprint 8-1에서 만든 `Question Decomposer` 결과를 RAG Retriever와 연결한다.

한 입력 안에 여러 질문이 들어오면 질문을 sub-question으로 나눈 뒤, 각 sub-question마다 RAG 검색을 실행한다.

예시:

```text
분쇄기가 뭐야? 그리고 철괴를 만들려면 어떻게 해야 돼?
```

처리 방향:

```text
original question
-> Question Decomposer
-> sub-question 1: 분쇄기가 뭐야?
-> sub-question 2: 철괴를 만들려면 어떻게 해야 돼?
-> 각 sub-question별 ManualRagRetriever.retrieve()
-> sub-question별 retrieval result를 하나로 묶음
```

## 포함 범위

- `MultiQuestionRagRetriever` 추가
- 단일 질문이면 기존 RAG Retriever를 1회 호출
- 여러 질문이면 sub-question 순서대로 RAG Retriever를 호출
- sub-question별 confidence와 retrieval metadata 유지
- LLM prompt에 넣기 쉬운 combined context 생성
- 초보자용 한글 docstring 추가

## 제외 범위

- LLM prompt builder 최종 연결
- operator_guide service 전체 runtime 연결
- Unreal 최종 응답 JSON schema 확장
- prompt injection guardrail 적용

위 항목은 Sprint 8 후속 runtime integration 또는 Sprint 8.5에서 처리한다.

## 검증 전략

사용자와 합의한 테스트 범위를 따른다.

```text
개발 중:
Sprint 8-2 테스트만 실행

구현 완료 직후:
question decomposer + rag retriever 주변 테스트 실행

커밋/PR 직전:
현재까지 연결된 RAG 테스트 묶음 실행
```

## 작업 로그

- 2026-06-15: Sprint 8-2를 sub-question별 RAG 검색 연결 단계로 분리했다.
- 2026-06-15: RED 단계에서 `multi_question_rag_retriever` 모듈이 없어 테스트가 실패하는 것을 확인했다.
- 2026-06-15: `MultiQuestionRagRetriever`를 추가해 sub-question마다 기존 RAG retriever를 호출하도록 구현했다.
- 2026-06-15: sub-question별 confidence count와 combined context를 metadata/context로 묶었다.

## 트러블슈팅 로그

- 2026-06-15: 전체 operator_guide service에 바로 연결하면 변경 범위가 커지므로, 먼저 독립 조합 모듈로 구현해 테스트 가능한 경계를 만들기로 했다.
- 2026-06-15: 이 단계에서는 LLM prompt builder와 Unreal 응답 JSON을 수정하지 않았다. 검색 결과 조합까지만 완성하고, 최종 runtime 연결은 다음 단계로 남겼다.
