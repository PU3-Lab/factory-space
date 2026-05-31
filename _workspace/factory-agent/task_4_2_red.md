# Task 4.2 RED: Google Gen AI adapter

목표:

- `GoogleGenAiLlmAdapter`가 Google Gen AI client의 `models.generate_content()`를 호출한다.
- `response_mime_type="application/json"`, `max_output_tokens`, `temperature`, `timeout_ms`를 config로 전달한다.
- 성공 응답은 raw text를 반환하고, 빈 응답/provider 예외는 `None`을 반환한다.

RED 확인 명령:

```bash
uv run --extra dev pytest tests/test_llm_adapter.py::test_google_llm_adapter_returns_response_text -q
```

예상 실패:

- 현재 `GoogleGenAiLlmAdapter.invoke()`는 provider 호출 없이 항상 `None`을 반환한다.
