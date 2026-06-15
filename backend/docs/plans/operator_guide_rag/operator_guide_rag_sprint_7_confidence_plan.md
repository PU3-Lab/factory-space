# operator_guide RAG Sprint 7 Confidence Plan

## 목표

Sprint 7에서는 RAG 검색 결과를 보고 backend가 `high`, `medium`, `low` confidence를 계산한다.

confidence는 LLM이 임의로 정하는 값이 아니다. 검색된 문서 수, top score, 질문과 문서 제목/id의 직접 매칭 여부처럼 코드가 확인할 수 있는 신호를 기준으로 계산한다.

## 포함 범위

- `ManualRagRetriever` 결과에 confidence metadata 추가
- 검색 결과가 없을 때 `low` 반환
- top score가 높고 질문이 title/doc_id/source_row_id와 직접 맞으면 `high` 반환
- 관련 문서는 있지만 직접 매칭이나 점수가 약하면 `medium` 또는 `low` 반환
- confidence reason을 사람이 읽을 수 있는 문자열로 제공
- 초보자용 한글 docstring 보강

## 제외 범위

- LLM prompt 최종 연결
- Unreal 응답 JSON 최종 schema 연결
- runtime middleware 전체 연결
- source type boost와 title boost의 검색 순위 보정

위 항목은 Sprint 8 이후 runtime integration 단계에서 연결한다.

## 구현 방향

```text
player question
-> query embedding
-> pgvector top-k search
-> confidence calculator
-> ManualRagRetrievalResult
   - confidence
   - confidence_reason
   - retrieval_metadata
```

## confidence 기준 초안

```text
high:
- 검색 결과가 있고
- top score가 0.85 이상이고
- 질문이 title/doc_id/source_row_id 중 하나와 직접 매칭된다.

medium:
- 검색 결과가 있고
- top score가 0.65 이상이다.

low:
- 검색 결과가 없거나
- top score가 0.65 미만이다.
```

## 검증 계획

- 테스트를 먼저 추가해서 confidence가 아직 없어서 실패하는 것을 확인한다.
- 최소 구현으로 테스트를 통과시킨다.
- 기존 retriever 테스트와 Sprint 1~6 RAG 테스트를 다시 실행한다.
- ruff check를 실행한다.

## 작업 로그

- 2026-06-15: Sprint 7 구현 시작 전 confidence 계산 범위와 제외 범위를 분리했다.
- 2026-06-15: `ManualRagRetrievalResult`에 `confidence`, `confidence_reason`, `retrieval_metadata`를 추가했다.
- 2026-06-15: `ManualRagRetriever`가 검색 결과를 받은 뒤 backend 기준으로 confidence를 계산하도록 연결했다.

## 트러블슈팅 로그

- 2026-06-15: 현재 브랜치에는 PR 범위 밖의 미커밋 파일이 남아 있으므로, Sprint 7 구현은 `operator_guide` RAG 관련 파일과 계획 문서에만 제한한다.
- 2026-06-15: 테스트 파일의 기존 한글 문자열이 콘솔에서 깨져 보여 patch anchor가 맞지 않았다. 새 테스트는 ASCII 질문/제목을 사용해 인코딩 영향 없이 검증되도록 작성했다.
- 2026-06-15: RED 단계에서 `ManualRagRetrievalResult`에 `confidence` 필드가 없어 3개 테스트가 실패했고, 이후 최소 구현으로 7개 retriever 테스트를 통과시켰다.
