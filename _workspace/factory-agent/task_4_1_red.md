# Task 4.1 RED: LLM slot adapter factory

목표:

- `NoopLlmAdapter.invoke()`는 항상 `None`을 반환한다.
- provider slot에 따라 concrete adapter class를 선택한다.
- 이 단계에서는 provider별 네트워크 호출을 구현하지 않고 class/factory 계약만 고정한다.

RED 확인 명령:

```bash
uv run --extra dev pytest tests/test_llm_adapter.py -q
```

예상 실패:

- 아직 `NoopLlmAdapter`, `GoogleGenAiLlmAdapter`, `OpenAiLlmAdapter`, `LocalLlmAdapter`, `create_llm_adapter()`가 없다.
