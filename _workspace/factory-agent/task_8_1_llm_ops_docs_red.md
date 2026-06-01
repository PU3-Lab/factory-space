# Task RED - LLM provider 운영 문서 정리

## Scope

Sprint 8.1은 LLM provider 운영 규칙을 README, 결정 로그, 계획 문서에 맞춘다. Production code는 수정하지 않는다.

## Verification before edit

문서 전용 작업이므로 failing test 대신 구조 검증 기준을 먼저 기록한다.

- `backend/src/DECISION_LOG.md`에 `google-genai` 선택 이유가 남아야 한다.
- `backend/README.md`에 CI/test all-`none`, dev local LLM, provider fallback 예시가 있어야 한다.
- `backend/docs/plans/llm_implementation_sprint.md`의 Sprint 5.2와 Sprint 8.1 체크 상태가 실제 테스트/문서 상태와 맞아야 한다.
- `rg -n "OpenAI API Configuration|google-generativeai|langchain-google-genai" backend` 결과는 낡은 runtime 설정 문구를 찾지 않아야 한다.

## Acceptance

- 문서만 변경한다.
- local `.env` 같은 ignored 개인 설정은 커밋 대상에 포함하지 않는다.
- 검증 명령 결과를 리뷰 기록과 최종 보고에 남긴다.
