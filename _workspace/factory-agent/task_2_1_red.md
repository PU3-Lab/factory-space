# Task 2.1 RED: LLM slot settings

목표:

- `default`, `fallback1`, `fallback2` slot 설정을 env에서 읽는다.
- env 미설정/CI 기본값은 모든 slot provider `none`이다.
- `ENVIRONMENT=development`는 default slot을 local provider로 사용한다.
- provider별 model/API key/base URL 규칙을 검증한다.

RED 확인 명령:

```bash
uv run --extra dev pytest tests/test_llm_settings.py -q
```

예상 실패:

- 아직 `llm.settings` 모듈과 `LLMSettings.from_env()`가 없다.
