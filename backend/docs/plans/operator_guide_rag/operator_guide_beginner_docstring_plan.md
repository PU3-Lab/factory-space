# operator_guide 초보자용 docstring 보강 계획

## 목표

operator_guide agent의 RAG/LLM 연결 코드를 처음 읽는 팀원이 각 파일의 역할과 데이터 흐름을 빠르게 이해할 수 있도록 한글 docstring을 보강한다.

이번 작업은 코드 동작을 바꾸지 않고 설명만 추가한다.

## 포함 범위

- `rag_documents.py`
- `rag_embedding.py`
- `rag_ingestion.py`
- `rag_schema.py`
- `rag_store.py`
- `rag_upsert.py`
- `prompt_builder.py`
- `service.py`

## 제외 범위

- 로직 변경
- 테스트 데이터 변경
- CSV 내용 변경
- `.env`, `.env.prod` 변경
- 기존 unrelated 작업 파일 정리

## 작성 원칙

- 초보자가 이해할 수 있게 "무엇을 하는 파일/클래스/함수인지"를 먼저 설명한다.
- RAG 용어는 짧게 풀어쓴다.
- 자명한 줄마다 주석을 달지 않는다.
- 기존 함수명과 구조는 유지한다.
- 동작을 바꾸지 않기 위해 docstring과 필요한 최소 주석만 추가한다.

## 검증

- `ruff check`로 문법/스타일 문제가 없는지 확인한다.
- 문서/주석 변경이므로 unit test는 생략할 수 있다.

## 작업 로그

- 2026-06-15: 사용자 요청에 따라 operator_guide agent 코드에 초보자용 한글 docstring을 추가하는 계획을 작성했다.

## 트러블슈팅 로그

- 2026-06-15: 이번 요청은 설명 보강이 목적이므로 CSV, env, runtime 로직은 변경하지 않는 범위로 제한했다.
