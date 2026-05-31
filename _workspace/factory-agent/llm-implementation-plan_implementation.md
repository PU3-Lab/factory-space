# LLM 구현 계획 작업 기록

## 변경 파일

- `backend/llm_implementation_plan.md`
- `backend/src/DECISION_LOG.md`

## 내용

- 현재 placeholder LLM adapter 상태를 기준으로 실제 provider 연결 계획을 작성했다.
- 현재 의존성에 이미 포함된 Google GenAI 계열을 1차 provider로 계획했다.
- 기본 provider는 `none`으로 두어 로컬/CI에서 외부 API 없이 fallback 경로가 동작하도록 했다.
- Agent routing은 기존 결정대로 prompt 기반 LLM 결정으로 유지하고, keyword/if-else fallback routing은 금지했다.
- 구현 단계, 테스트 계획, 커밋 단위, 보류 항목을 분리해 기록했다.

## Production code 변경

없음. 문서 전용 작업이다.
