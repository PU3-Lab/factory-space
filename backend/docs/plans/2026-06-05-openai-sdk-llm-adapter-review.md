# OpenAI SDK LLM Adapter 전환 리뷰

**Review target:** OpenAI provider를 공식 OpenAI Python SDK 기반으로 전환하고, local provider의 OpenAI-compatible HTTP 호출은 유지하는 변경.

**Review date:** 2026-06-05

## 요약

변경 방향은 적절하다. `openai` provider와 `local` provider를 같은 raw HTTP 경로로 묶지 않고, 실제 OpenAI GPT API 호출만 공식 SDK로 분리한 판단은 현재 LLM slot 구조와 잘 맞는다.

기존에 지적된 lint 기준과 프로젝트 import 규칙 위반 사항은 모두 해결(Resolved)되었다.

## Findings

### [Resolved] P1: OpenAI SDK protocol 반환 타입이 `Any`라 ruff가 실패함

File:

- `backend/src/llm/adapter.py`

Location:

- `_OpenAiChatCompletions.create()` 반환 타입

Issue:

- 반환 타입이 `Any`라 `ruff`의 `ANN401`에 걸린다.
- `uv run --extra dev ruff check .`가 실패하므로 CI에 ruff가 포함되면 변경이 막힐 수 있다.

Status:

- **Resolved**: `_OpenAiCompletion`, `_OpenAiChoice`, `_OpenAiMessage` 프로토콜을 추가하고 `create()`의 반환 타입을 `_OpenAiCompletion`으로 변경하여 타입 안전성을 확보하고 ruff 검사를 통과시켰다.

### [Resolved] P2: 함수 내부 import가 프로젝트 규칙을 위반함

File:

- `backend/src/llm/adapter.py`

Location:

- `_create_openai_client()` 내부의 `from openai import OpenAI`

Issue:

- 프로젝트 지침은 import를 항상 파일 상단에 두고, 함수나 메서드 내부 import를 금지한다.
- `openai`는 runtime dependency로 추가되었으므로 지연 import가 필요하지 않다.

Status:

- **Resolved**: `from openai import OpenAI` import 구문을 파일 최상단 영역으로 이동시켰으며, `_create_openai_client()`는 전역 import된 모듈을 사용하도록 수정하였다.

## Verification

실행 결과:

```bash
cd backend
uv run --extra dev pytest tests/test_llm_adapter.py tests/test_llm_settings.py -q
```

Result:

```text
26 passed
```

```bash
cd backend
uv run --extra dev pytest -q
```

Result:

```text
142 passed
```

```bash
cd backend
uv run --extra dev ruff check .
```

Result:

```text
All checks passed!
```

## Residual Risk

- SDK client 생성 경로는 fake client 기반 unit test와 import smoke로 검증하였다.
- local provider가 OpenAI SDK 전환에 휘말리지 않도록 기존 HTTP payload 검증 테스트를 유지하였다.
- 프로젝트 전체의 ruff 검사가 통과하도록 `csv_repository.py` 내의 미정렬 임포트도 추가 조치 완료하였다.

## Recommended Next Steps (All Checked)

- [x] OpenAI SDK response protocol을 추가해 `Any` 반환 타입을 제거한다.
- [x] `OpenAI` import를 파일 상단으로 이동한다.
- [x] `uv run --extra dev ruff check src/llm/adapter.py tests/test_llm_adapter.py`를 통과시킨다.
- [x] 전체 ruff 실패 중 unrelated import sorting 문제는 `ruff check --fix`를 실행하여 해결 완료한다.
