# korean-agent-prompts Verification

## Targeted Green

Executed from `backend/`.

```bash
uv run --extra dev pytest tests/test_message_router.py tests/test_agent_leaf_behaviors.py
```

Result:

```text
22 passed in 0.24s
```

## Full Verification

Executed from `backend/`.

```bash
uv run --extra dev pytest
```

Result:

```text
52 passed in 0.49s
```

```bash
uv run --extra dev ruff check .
```

Result:

```text
All checks passed!
```
