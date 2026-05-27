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