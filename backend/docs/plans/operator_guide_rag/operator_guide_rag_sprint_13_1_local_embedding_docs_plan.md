# operator_guide RAG Sprint 13.1 로컬 임베딩 설정 문서 보강 계획

## 목표

Sprint 13에서 구현한 local embedding provider를 팀원이 쉽게 재현할 수 있도록 문서와 환경 변수 예시를 정리한다.

이번 Sprint 13.1은 기능 로직을 바꾸는 작업이 아니라, 로컬 임베딩 실행 방법을 명확히 하는 보정 작업이다.

## 배경

Sprint 13에서 `FACTORY_EMBEDDING_PROVIDER=local`을 지원하도록 구현했다.

현재 코드에서는 다음 구성이 가능하다.

```text
OpenAI embedding
local OpenAI-compatible embedding
local Ollama native embedding
```

다만 `.env.example`에는 OpenAI embedding 예시만 있어, 로컬 임베딩을 처음 실행하는 사람이 어떤 값을 넣어야 하는지 바로 알기 어렵다.

## 작업 범위

- Sprint 13 리뷰 문서의 브랜치명을 실제 작업 브랜치명으로 보정한다.
- `.env.example`에 local embedding 예시를 추가한다.
- 기존 OpenAI embedding 예시는 유지한다.
- 실행 로직은 변경하지 않는다.

## 완료 기준

- `sprint_13_review.md`의 브랜치명이 현재 작업 브랜치와 일치한다.
- `.env.example`만 보고도 local embedding 기본 설정을 알 수 있다.
- 문서/환경 예시 변경이므로 기능 테스트는 별도로 추가하지 않는다.
- 변경 후 `uv run ruff check`를 실행해 코드 품질 상태를 확인한다.

## 작업 로그

- 2026-06-16: Sprint 13 리뷰 결과를 기준으로 Sprint 13.1 문서 보강 작업을 시작했다.

## 트러블슈팅 로그

- 2026-06-16: Sprint 13 구현은 통과했지만 `.env.example`에 local embedding 예시가 없어 팀 재현성이 떨어질 수 있어 보강 대상으로 분리했다.
