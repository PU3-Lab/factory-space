# factory-space

factory-space 프로젝트는 Python 3.12+ 환경에서 작동하는 파이썬 애플리케이션 및 패키지 템플릿입니다.

---

## 🛠️ 개발 환경 요구사항

- **Python**: `>= 3.12`
- **패키지 매니저**: `uv` (권장) 또는 `pip`

---

## 🚀 빠른 시작

### 1. 의존성 설치 및 가상환경 구성
본 프로젝트는 속도가 매우 빠른 Python 패키지 인스톨러인 `uv`를 사용하도록 권장합니다.

```bash
# 가상환경 생성 및 의존성 설치 (uv 사용 시)
uv sync

# 또는 기본 pip 사용 시
python3 -m venv .venv
source .venv/bin/activate
pip install -e .
```

### 2. 실행 방법
애플리케이션의 메인 진입점은 `main.py`입니다.

```bash
# uv를 통해 실행
uv run main.py

# 또는 가상환경 활성화 후 실행
.venv/bin/python main.py
```

### uvicorn으로 FastAPI 서버 실행

개발 중 WebSocket endpoint를 직접 확인하려면 `uvicorn`으로 FastAPI app을 실행할 수 있습니다.

```bash
uv run uvicorn app:app --host 127.0.0.1 --port 8000 --reload
```

실행 후 확인 주소:

```text
Health check: http://127.0.0.1:8000/health
WebSocket: ws://127.0.0.1:8000/ws/agent
```

Postman이나 Unreal 클라이언트에서는 HTTP POST가 아니라 WebSocket 연결로 `ws://127.0.0.1:8000/ws/agent`에 접속한 뒤 JSON 메시지를 전송합니다.

### Agent pipeline smoke test

서버를 실행한 뒤 smoke runner로 실제 WebSocket 경로를 확인할 수 있습니다.

`none` smoke는 LLM slot provider가 모두 `none`인 backend에서 외부 API key 없이 endpoint와 routing failure 경로만 검증합니다. 기본 개발용 `.env.example`은 local LLM을 기본값으로 두므로, `none` smoke에는 전용 env 예시를 사용합니다.

```bash
uv run --env-file smoke-none.env.example uvicorn app:app --host 127.0.0.1 --port 8000

uv run --env-file smoke-none.env.example python scripts/smoke_agent_pipeline.py none
```

local LLM이 OpenAI-compatible endpoint로 떠 있고 `.env`가 local LLM 설정을 담고 있으면 네 Agent 경로를 순서대로 확인합니다.

```bash
cp .env.example .env
uv run --env-file .env uvicorn app:app --host 127.0.0.1 --port 8000

uv run --env-file .env python scripts/smoke_agent_pipeline.py local
```

OpenAI/Gemini 같은 외부 provider smoke는 명시적으로 opt-in할 때만 실행합니다. CI 기본 검증에는 넣지 않습니다.

```bash
FACTORY_SMOKE_EXTERNAL_PROVIDER=1 uv run --env-file .env python scripts/smoke_agent_pipeline.py providers
```

서버 주소를 바꾸려면 `--base-url` 또는 `FACTORY_SMOKE_BASE_URL`을 사용합니다.

---

## 🧹 코드 스타일 및 품질 관리 (Ruff)

이 프로젝트는 파이썬 린터 및 포맷터로 **Ruff**를 사용합니다.

- **설정 파일**: [pyproject.toml](./pyproject.toml)에 정의되어 있습니다.
- **룰셋**: Pyflakes(`F`), Pycodestyle(`E`), Isort(`I`), Pyupgrade(`UP`), Flake8-Annotations(`ANN`), PEP8-Naming(`N`) 등이 적용되어 있습니다.

### 린트 및 포맷팅 명령어

```bash
# 린트 및 스타일 체크
uv run ruff check .

# 코드 스타일 자동 수정
uv run ruff check --fix .

# 코드 포맷팅 (블랙 스타일 포맷팅)
uv run ruff format .
```

---

## 💻 VS Code 연동 및 자동 설정

본 레포지토리에는 VS Code용 작업 환경 설정이 포함되어 있습니다. ([.vscode/settings.json](./.vscode/settings.json))

### 권장 익스텐션
- **Ruff** (`charliermarsh.ruff`): VS Code 마켓플레이스에서 반드시 설치하시기 바랍니다.

### VS Code 주요 기능
- **저장 시 자동 수정 및 포맷팅**: 파일을 저장할 때 자동으로 임포트 정렬(isort)과 코드 린트 수정(`--fix`), 코드 포맷팅이 작동합니다.
