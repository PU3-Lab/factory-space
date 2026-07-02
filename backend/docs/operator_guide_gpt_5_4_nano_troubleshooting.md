# operator_guide GPT-5.4 nano 404 문제 해결 가이드

이 문서는 `operator_guide`가 OpenAI의 `gpt-5.4-nano` 대신 Gemini fallback으로
응답하거나 `model not found` 오류를 출력할 때 확인할 사항을 정리한다.

이번 장애의 실제 원인은 모델명이나 API 키가 아니었다. 운영 서버가
`.env.prod`를 읽은 다음 개발용 `.env`도 추가로 읽으면서, OpenAI 요청 주소가
로컬 Ollama 주소로 바뀐 환경 설정 충돌이었다.

## 1. 빠른 해결

`backend/.env.prod`에 다음 설정이 있는지 확인한다.

```env
FACTORY_ENV_FILE=.env.prod
FACTORY_LLM_DEFAULT_PROVIDER=openai
FACTORY_LLM_DEFAULT_MODEL=gpt-5.4-nano
FACTORY_LLM_DEFAULT_BASE_URL=https://api.openai.com/v1
```

API 키는 실제 값을 저장소나 문서에 기록하지 않는다. 로컬의 안전한 환경 파일
또는 배포 환경의 secret으로 주입한다.

설정 변경 후 서버를 완전히 종료하고 새 CMD에서 다시 실행한다.

```bat
cd C:\factory-space\backend
uv run --env-file .env.prod python scripts/run_prod_server.py
```

PowerShell에서도 같은 명령을 사용할 수 있다.

## 2. 정상 동작 확인

agent-test에서 다음 요청을 전송한다. 문제를 진단할 때는 자동 leaf-agent
라우팅을 제외하기 위해 `sub_agent`를 명시하는 편이 좋다.

```json
{
  "type": "agent.request",
  "request_id": "operator-guide-gpt-check-001",
  "session_id": "operator-guide-gpt-check-session",
  "client_id": "agent-test-console",
  "agent": "operator_guide",
  "payload": {
    "sub_agent": "operator_guide.machine_help",
    "question": "분쇄기가 뭐야? 어디에 써?"
  },
  "context": {
    "language": "ko",
    "mode": "agent_test"
  }
}
```

정상 응답의 `metadata.currentModel`은 다음과 같다.

```json
{
  "slot": "default",
  "provider": "openai",
  "model": "gpt-5.4-nano"
}
```

고정 snapshot을 사용한다면 `model`에는
`gpt-5.4-nano-2026-03-17`이 표시된다.

## 3. 장애 증상

서버 로그에 다음 오류가 출력되고 응답 모델이 Gemini로 표시될 수 있다.

```text
OpenAI LLM call failed: Error code: 404
model 'gpt-5.4-nano' not found
```

```json
{
  "llmSlot": "fallback1",
  "llmProvider": "google",
  "llmModel": "gemini-2.5-flash"
}
```

이때 Gemini 응답은 fallback 기능이 정상적으로 작동한 결과다. 하지만 기본
OpenAI 호출이 실패한 원인은 별도로 해결해야 한다.

## 4. 실제 근본 원인

운영 서버는 두 단계로 환경 파일을 읽는다.

```text
run_prod_server.py
-> backend/.env.prod 로드
-> Uvicorn에서 FastAPI app 시작
-> app lifespan의 _load_backend_env() 실행
-> FACTORY_ENV_FILE이 없으면 backend/.env 추가 로드
```

개발용 `backend/.env`에는 로컬 모델을 위한 다음 설정이 있을 수 있다.

```env
FACTORY_LLM_DEFAULT_BASE_URL=http://localhost:11434/v1
```

`.env.prod`에 `FACTORY_LLM_DEFAULT_BASE_URL`이 없으면 `app.py`의 환경 로더가
개발용 값을 채운다. 그 결과 설정 조합은 다음과 같이 잘못 구성된다.

```text
provider = openai
model = gpt-5.4-nano
base_url = http://localhost:11434/v1
```

OpenAI 모델 요청이 공식 OpenAI API가 아니라 Ollama로 전달되므로, Ollama는
해당 모델을 찾을 수 없다는 404를 반환한다.

반면 아래와 같은 단독 테스트는 FastAPI lifespan을 거치지 않는다.

```bat
uv run --env-file .env.prod python -c "from openai import OpenAI; r=OpenAI().responses.create(model='gpt-5.4-nano', input='안녕이라고만 답해'); print(r.output_text)"
```

따라서 단독 호출은 성공하지만 실제 서버 요청은 실패하는 차이가 발생했다.

## 5. 진단 순서

### 5.1 모델이 API 키에 노출되는지 확인

```bat
uv run --env-file .env.prod python -c "from openai import OpenAI; print([m.id for m in OpenAI().models.list().data if 'nano' in m.id])"
```

목록에 `gpt-5.4-nano`가 있다면 모델 ID와 기본 API 접근은 유효하다.

### 5.2 OpenAI Responses API를 직접 호출

```bat
uv run --env-file .env.prod python -c "from openai import OpenAI; r=OpenAI().responses.create(model='gpt-5.4-nano', input='안녕이라고만 답해'); print(r.output_text)"
```

`안녕`이 출력되면 API 키, 모델명, OpenAI SDK의 기본 호출은 정상이다.

### 5.3 slot 전용 API 키 충돌 확인

코드는 `FACTORY_LLM_DEFAULT_API_KEY`를 `OPENAI_API_KEY`보다 우선한다. 비밀값을
출력하지 않고 두 키가 다른지만 확인한다.

```bat
uv run --env-file .env.prod python -c "import os; a=os.getenv('OPENAI_API_KEY'); b=os.getenv('FACTORY_LLM_DEFAULT_API_KEY'); print({'slot_key_set':bool(b), 'same_key':(a==b) if b else True})"
```

정상적인 예:

```text
{'slot_key_set': False, 'same_key': True}
```

### 5.4 프로젝트 adapter를 직접 확인

```bat
uv run --env-file .env.prod python -c "from agents.operator_guide.machine_help import MachineHelpAgent; from agents.base import AgentContext; from llm.settings import LLMSettings; from llm.adapter import create_llm_adapter; a=MachineHelpAgent(); c=AgentContext(request_id='diag',session_id='diag',client_id='diag',metadata={'language':'ko','mode':'gameplay'}); m=a.build_prompt_messages({'question':'분쇄기가 뭐야? 어디에 써?'},c); print(create_llm_adapter(LLMSettings.from_env().default).invoke_messages(m))"
```

이 호출이 성공하면 `operator_guide` prompt와 OpenAI adapter도 정상이다.

### 5.5 FastAPI startup 이후 base URL 확인

다음 명령은 실제 서버 startup과 동일하게 추가 환경 파일을 읽은 후의 주소를
보여준다.

```bat
uv run --env-file .env.prod python -c "import os; from app import _load_backend_env; _load_backend_env(); print({'default_base_url':os.getenv('FACTORY_LLM_DEFAULT_BASE_URL'), 'env_file':os.getenv('FACTORY_ENV_FILE')})"
```

정상:

```text
default_base_url = https://api.openai.com/v1
env_file = .env.prod
```

문제가 있는 예:

```text
default_base_url = http://localhost:11434/v1
env_file = None
```

## 6. alias와 snapshot 선택

두 모델 ID 모두 사용할 수 있다.

```env
# OpenAI가 최신 snapshot으로 갱신하는 alias
FACTORY_LLM_DEFAULT_MODEL=gpt-5.4-nano
```

```env
# 동일한 모델 버전을 계속 사용하는 고정 snapshot
FACTORY_LLM_DEFAULT_MODEL=gpt-5.4-nano-2026-03-17
```

개발 중 최신 개선을 자동으로 받으려면 alias가 편하다. 평가 결과와 응답 품질의
재현성이 중요하면 snapshot 고정이 더 적합하다. 이번 404의 근본 원인은 이 두
모델 ID의 차이가 아니라 잘못된 `base_url`이었다.

## 7. fallback 동작 이해

현재 기본 순서는 다음과 같다.

```text
default   -> OpenAI
fallback1 -> Google Gemini
fallback2 -> Local Ollama
```

기본 OpenAI 호출이 빈 응답을 반환하거나 예외를 처리하면 Pipeline은 다음 slot을
호출한다. 따라서 Gemini가 답했다는 사실은 Gemini가 기본 모델이라는 뜻이 아니다.
응답의 다음 필드로 실제 사용 모델을 확인한다.

```text
payload.metadata.llmSlot
payload.metadata.llmProvider
payload.metadata.llmModel
payload.metadata.currentModel
```

`sub_agent`가 없는 자유 질문에서는 모델 호출이 두 번 발생할 수 있다.

```text
1. operator_guide 내부 leaf-agent 선택
2. 최종 답변 생성
```

진단 중에는 `sub_agent`를 명시하면 첫 번째 호출을 제외할 수 있다.

## 8. RAG 문제와 구분하기

다음 오류는 LLM 모델 404와 별개의 문제다.

```text
operator_guide RAG retrieval failed
psycopg.errors.ConnectionTimeout
```

이 오류는 PostgreSQL/pgvector 연결을 확인해야 한다.

```bat
cd C:\factory-space
scripts\setup_rag_db.bat
```

RAG만 제외하고 LLM을 테스트할 때는 다음 값만 설정한다.

```bat
set FACTORY_RAG_RUNTIME_MOCK=true
```

테스트가 끝나면 제거한다.

```bat
set FACTORY_RAG_RUNTIME_MOCK=
```

다음 설정은 모든 LLM을 끄므로 실제 GPT 테스트에 사용하면 안 된다.

```bat
set FACTORY_LLM_DEFAULT_PROVIDER=none
set FACTORY_LLM_FALLBACK1_PROVIDER=none
set FACTORY_LLM_FALLBACK2_PROVIDER=none
```

## 9. 자주 혼동하는 항목

### `VIRTUAL_ENV` 경고

```text
VIRTUAL_ENV=... does not match the project environment path ...
```

`uv`가 현재 활성화된 상위 가상환경 대신 `backend/.venv`를 사용한다는 경고다.
이번 OpenAI 404의 원인은 아니었다.

### 요청의 `temperature`

요청 JSON의 `context.temperature`는 모델을 선택하지 않는다. 기본 모델과
fallback 순서는 환경 설정으로 정한다.

### 서버 재시작

환경변수는 `AgentPipeline` 생성 시 읽힌다. `.env.prod`를 수정한 뒤에는 기존
서버를 완전히 종료하고 새 프로세스로 시작해야 한다.

### CMD에 남은 환경변수

CMD에서 `set KEY=value`로 지정한 값은 같은 창에서 `.env.prod`보다 우선할 수
있다. 진단이 끝난 뒤 `set KEY=`로 제거하거나 새 CMD에서 서버를 시작한다.

## 10. 재발 방지 체크리스트

- [ ] 운영 서버는 `FACTORY_ENV_FILE=.env.prod`를 사용한다.
- [ ] OpenAI default slot의 base URL은 `https://api.openai.com/v1`이다.
- [ ] 개발용 `.env`의 Ollama 주소가 운영 설정에 섞이지 않는다.
- [ ] 실제 API 키를 문서, 로그, Git에 추가하지 않는다.
- [ ] 설정 변경 후 서버를 완전히 재시작한다.
- [ ] agent-test 응답의 `metadata.currentModel`을 확인한다.
- [ ] GPT 진단 시 LLM provider를 `none`으로 설정하지 않는다.
- [ ] RAG 오류와 LLM 모델 오류를 별도로 판단한다.

## 11. 코드 차원의 후속 보완

현재는 `.env.prod`의 `FACTORY_ENV_FILE`과 명시적인 OpenAI base URL로 충돌을
방지할 수 있다. 더 안전하게 만들려면 `run_prod_server.py`가 실행될 때
`FACTORY_ENV_FILE=.env.prod`를 기본값으로 지정하도록 보완할 수 있다.

이 보완을 적용하면 운영 실행 명령에서 `.env.prod`를 읽은 뒤 FastAPI가 다시
개발용 `.env`를 불러오는 실수를 구조적으로 차단할 수 있다.
