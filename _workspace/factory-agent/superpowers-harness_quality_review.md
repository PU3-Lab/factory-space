# superpowers-harness Quality Review

Status: pass

## Findings

- initial status: fix
- severity: correctness / boundary contract
- file: `backend/AGENTS.md`
- issue: documented Agent contract showed `build_prompt(self, context)` while runtime protocol requires `build_prompt(payload, context)` and `fallback(payload, context)`
- impact: future agent implementations could follow a stale contract and fail pipeline integration
- required fix: update the example to match `backend/src/agents/base.py`
- resolution: `backend/AGENTS.md` now shows `build_prompt(self, payload: dict[str, Any], context: AgentContext) -> str` and `fallback(self, payload: dict[str, Any], context: AgentContext) -> AgentRunResult`

## Test Gaps

- No automated test gap remains for this docs-only harness setup.
- Fresh verification was rerun after the documentation fix.

## Approval Notes

- No remaining blocking findings.
- The documented contract now matches the runtime `Agent` protocol.
- Fresh verification accepted: `pytest` 25 passed; `ruff check .` all checks passed.

## Follow-up Review Findings

Status: fix

### Malformed envelope request correlation

- severity: high
- file: `backend/src/agents/pipeline.py`
- issue: Pydantic validation rejects malformed dict input before the LangGraph validation nodes can preserve `request_id`, `session_id`, `client_id`, or `agent`.
- impact: clients cannot reliably correlate `agent.error` responses to malformed requests.
- required fix: add tests for wrong message type and invalid payload preserving correlation fields, then update validation error construction or parsing flow.

### Cache hit metadata loss

- severity: medium
- file: `backend/src/agents/pipeline.py`
- issue: cache write stores only `responsePayload`, and cache hit replaces metadata with `{"cache": "hit"}`.
- impact: fallback/LLM metadata from the original response is lost on repeated requests.
- required fix: cache response payload and response metadata together, and add a test that cache hit metadata preserves the original execution metadata plus `cache: hit`.
