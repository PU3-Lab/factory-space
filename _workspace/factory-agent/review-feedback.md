# Review Feedback

이 파일은 코드 리뷰에서 나온 문제와 후속 수정 대상을 별도로 추적한다.

## 2026-05-31 Pipeline Review

Status: open

### 1. Malformed envelope request correlation

- severity: high
- file: `backend/src/agents/pipeline.py`
- related decision: `backend/src/DECISION_LOG.md` section 15.1

문제:

- `AgentPipeline.run()`이 `AgentRequestEnvelope.model_validate()`를 먼저 호출한다.
- 잘못된 `type` 또는 object가 아닌 `payload`는 graph 내부 validation node에 도달하기 전에 `INVALID_ENVELOPE`로 끝난다.
- 이 경로에서는 raw message의 `request_id`, `session_id`, `client_id`, `agent`가 error envelope에 보존되지 않는다.

영향:

- 클라이언트가 어떤 요청의 오류인지 매칭하기 어렵다.
- `INVALID_MESSAGE_TYPE`, `INVALID_PAYLOAD` 분기가 dict 입력에서는 사실상 unreachable 상태다.

필요 작업:

- 잘못된 type과 invalid payload에서도 correlation field가 보존되는 실패 테스트를 먼저 추가한다.
- validation error builder 또는 envelope parsing flow를 수정한다.
- WebSocket 경유 오류 응답에서도 같은 보존 규칙이 적용되는지 확인한다.

### 2. Cache hit metadata loss

- severity: medium
- file: `backend/src/agents/pipeline.py`
- related decision: `backend/src/DECISION_LOG.md` section 15.2

문제:

- cache write는 `responsePayload`만 저장한다.
- cache hit 응답은 metadata를 `{"cache": "hit"}`로 새로 만든다.
- 첫 응답에 있던 `fallback: true`, `llm: used` 같은 metadata가 반복 요청에서 사라진다.

영향:

- 같은 요청의 첫 응답과 cache hit 응답이 metadata 기준으로 달라진다.
- fallback/LLM 사용 여부 추적이 불안정해진다.

필요 작업:

- cache entry에 payload와 metadata를 함께 저장한다.
- cache hit 응답은 원래 metadata를 유지하고 `cache: hit`만 추가한다.
- 첫 응답과 cache hit 응답의 metadata 일관성을 검증하는 실패 테스트를 먼저 추가한다.
