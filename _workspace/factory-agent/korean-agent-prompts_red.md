# korean-agent-prompts Red Check

## Command

Executed from `backend/`.

```bash
uv run --extra dev pytest tests/test_message_router.py tests/test_agent_leaf_behaviors.py
```

## Result

```text
12 failed, 10 passed
```

## Expected Failure

테스트 기대값을 한글 prompt 문구로 먼저 변경했기 때문에 기존 영어 prompt 코드에서 실패했다.

대표 실패:

- `서버 전체 오케스트레이터`가 기존 `You are the server-level orchestrator...` prompt에 없음
- `공장 snapshot에서 공정 병목`이 기존 `Find process bottlenecks...` prompt에 없음
- manual QA, quest leaf prompt의 한글 문구가 기존 영어 prompt에 없음
