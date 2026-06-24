# Operator Guide Troubleshooting Log

이 문서는 `operator_guide` 실행 중 반복될 수 있는 문제를 `문제 -> 원인 -> 해결 -> 개선 결과` 형태로 남긴 기록이다.

## 2026-06-24 - gpt-5.4-nano가 실패하고 Gemini fallback으로 내려가는 문제

### 문제

`agent-test` 또는 Unreal 연동에서 `operator_guide` 요청을 보내면 응답은 나오지만, metadata가 다음처럼 표시됐다.

```json
{
  "llmSlot": "fallback1",
  "llmProvider": "google",
  "llmModel": "gemini-2.5-flash"
}
```

서버 로그에는 다음 에러가 반복됐다.

```text
OpenAI LLM call failed: Error code: 404 - {'error': {'message': "model 'gpt-5.4-nano' not found", ...}}
```

### 원인

`backend/scripts/run_server.py`와 `backend/src/app.py`의 env loader는 `.env.prod` 값을 읽을 때 `os.environ.setdefault(...)`를 사용한다.

즉, Windows/터미널에 이미 `OPENAI_API_KEY` 또는 `FACTORY_LLM_DEFAULT_API_KEY`가 잡혀 있으면 `.env.prod` 값이 덮어써지지 않는다. 이 때문에 서버가 의도한 OpenAI 키가 아니라, `gpt-5.4-nano` 권한이 없는 기존 환경변수의 키로 모델을 호출했고 OpenAI가 404를 반환했다.

직접 테스트에서 `pong`이 성공했는데 서버에서는 실패한 이유도 이 차이 때문이다. 테스트와 서버 실행 터미널이 서로 다른 환경변수를 들고 있으면 결과가 달라진다.

### 해결

서버를 켜기 전에 같은 터미널에서 기존 키를 비우고, default slot 전용 키를 명시한다.

```cmd
cd C:\factory-space\backend

set OPENAI_API_KEY=
set FACTORY_LLM_DEFAULT_API_KEY=여기에_gpt-5.4-nano_성공한_OpenAI_API_KEY
set FACTORY_ENV_FILE=.env.prod

uv run --env-file .env.prod python scripts/run_prod_server.py --port 18000
```

서버 실행 전 단독 확인은 다음 명령으로 한다.

```cmd
uv run --env-file .env.prod python -c "from llm.settings import LLMSettings; from llm.adapter import create_llm_adapter; s=LLMSettings.from_env(); print(s.default.provider, s.default.model); print(create_llm_adapter(s.default).invoke('Say pong only.'))"
```

정상 결과:

```text
openai gpt-5.4-nano
pong
```

### 개선 결과

서버 재실행 후 `operator_guide` 응답 metadata가 다음처럼 표시되면 정상이다.

```json
{
  "llmSlot": "default",
  "llmProvider": "openai",
  "llmModel": "gpt-5.4-nano"
}
```

### 재발 시 체크리스트

1. 18000 포트에 오래된 서버가 떠 있지 않은지 확인한다.
2. 같은 터미널에서 `OPENAI_API_KEY`를 비우고 `FACTORY_LLM_DEFAULT_API_KEY`를 명시한다.
3. `pong` 테스트가 성공하는지 확인한다.
4. 같은 터미널에서 서버를 실행한다.
5. 응답 metadata의 `llmSlot`, `llmProvider`, `llmModel`을 확인한다.

### 주의

API key는 문서, PR, 채팅, 스크린샷에 남기지 않는다. 이미 노출된 키는 OpenAI/Gemini 대시보드에서 재발급한다.
