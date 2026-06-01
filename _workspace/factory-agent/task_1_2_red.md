# Task 1.2 RED: validation error correlation 보존

목표:

- Pydantic validation이 실패해도 raw message의 `request_id`, `session_id`, `client_id`, `agent`를 error envelope에 보존한다.
- 클라이언트가 malformed request에 대한 error response를 원래 request와 매칭할 수 있어야 한다.

RED 확인 명령:

```bash
uv run --extra dev pytest tests/test_pipeline_edges.py::test_pipeline_validation_error_preserves_raw_correlation_fields -q
```

예상 실패:

- 현재 `_build_validation_error()`는 `ValidationError`만 받아서 raw message의 correlation field를 복구하지 못한다.
- 결과적으로 `agent.error`의 `request_id`, `session_id`, `client_id`, `agent`가 `None`으로 반환된다.
