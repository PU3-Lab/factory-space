# Agent Message TTS Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `operator_guide`와 `process_optimizer`가 플레이어에게 보여주는 주요 메시지에 TTS 음성을 붙여 Unreal UI에서 재생할 수 있게 한다. dev/local/test 환경은 `edge-tts`, prod/staging 환경은 ElevenLabs API를 사용한다.

**Architecture:** FastAPI 백엔드가 실행 환경에 맞는 TTS provider를 선택하고, agent 응답 payload의 플레이어 노출 문장을 TTS 입력으로 변환한다. dev/local/test는 API 키 없이 `edge-tts`로 빠르게 음성을 만들고, prod/staging은 서버에 보관된 ElevenLabs API 키로 음성을 만든다. 생성된 오디오는 서버 로컬 캐시에 저장한 뒤 정적 URL로 노출하고, Unreal 클라이언트는 기존 WebSocket 응답에서 `payload.tts` 메타데이터를 읽어 오디오를 다운로드/재생한다.

**Tech Stack:** Python 3.12, FastAPI, Pydantic, pytest, `edge-tts`, ElevenLabs Text to Speech HTTP API, Unreal Engine C++, WebSockets, `RuntimeAudioImporter` 또는 Unreal 기본 런타임 오디오 로딩 보조 모듈

**Current Worktree Note:** 이 문서는 현재 워크트리에 `backend/src/tts/`와 일부 TTS 테스트가 이미 존재하는 상태를 기준으로 하는 continuation plan이다. 아래 단계의 `Create`는 파일이 없을 때만 새로 만들고, 이미 있으면 같은 책임을 가진 기존 파일을 수정/검증한다. RED 단계의 기대 실패는 “모듈 부재”가 아니라 현재 구현이 아직 충족하지 못하는 새 요구사항 실패를 의미한다.

---

## 0. 범위와 결정

### 적용 대상

- `operator_guide`
  - `payload.final_answer`를 TTS 대상으로 사용한다.
  - 오류/진행 메시지는 1차 범위에서 제외한다. 이유: 진행 메시지는 짧고 자주 바뀌어 비용과 중복 재생 위험이 크다.
- `process_optimizer`
  - TTS는 반드시 Unreal 대화창에 표시되는 문장과 같은 문장을 읽는다.
  - 백엔드는 `payload.tts.text`에 실제 합성한 문장을 넣고, Unreal은 process_optimizer 응답에서 가능하면 이 문장을 대화창에 표시한다.
  - `payload.summary`가 있어도 Unreal이 다른 고정 문구를 표시할 응답이면 summary를 읽지 않는다.
  - `analyze` 응답의 긴 `suggestions` 전체는 읽지 않는다. 대화창에 표시할 짧은 `tts.text` 또는 `display_message`만 음성화한다.
  - `state_update` alert처럼 Unreal이 highlight만 처리하고 대화창 문구를 표시하지 않는 응답은 TTS를 생성하지 않는다.

### Provider 선택

- `FACTORY_TTS_PROVIDER=auto`를 기본값으로 둔다.
- 환경 이름은 `FACTORY_ENV`, `APP_ENV`, `ENVIRONMENT` 순서로 읽되, 둘 이상 설정되어 서로 다른 provider class(dev/local/test vs prod/staging)를 가리키면 TTS를 비활성화하고 `disabled_reason="conflicting_environment"`를 반환한다.
- 충돌이 없고 가장 우선순위가 높은 환경값이 `dev`, `development`, `local`, `test`이면 `edge_tts`를 사용한다.
- 충돌이 없고 가장 우선순위가 높은 환경값이 `prod`, `production`, `staging`이면 `elevenlabs`를 사용한다.
- `FACTORY_TTS_PROVIDER`는 `auto`, 또는 현재 환경에서 허용된 provider만 허용한다. prod/staging에서 `edge_tts`, dev/local/test에서 `elevenlabs`를 지정하면 TTS를 비활성화하고 `disabled_reason="invalid_provider_for_environment"`를 반환한다.
- dev/local 기본 voice는 `ko-KR-SunHiNeural`로 둔다. 필요하면 `EDGE_TTS_VOICE`로 바꾼다.
- prod ElevenLabs 구현은 HTTP `POST /v1/text-to-speech/{voice_id}/stream`을 사용한다.
- 공식 문서 기준 endpoint는 `https://api.elevenlabs.io/v1/text-to-speech/:voice_id/stream`이고, `text`, `model_id`, `voice_settings`, `output_format`를 요청할 수 있다. MVP는 저장 URL과 Unreal 재생 계약을 단순하게 유지하기 위해 `ELEVENLABS_OUTPUT_FORMAT`을 `mp3_*` 값으로 제한한다.
- ElevenLabs WebSocket TTS는 부분 텍스트 입력과 alignment가 필요할 때 적합하지만, 이 프로젝트는 현재 agent 최종 응답을 받은 뒤 UI에 표시하는 구조라 MVP에서는 과하다.
- `edge-tts`는 Python 패키지 `edge-tts`를 설치하고 `edge_tts.Communicate(text=text, voice=voice, rate=rate, volume=volume, pitch=pitch).save("output.mp3")` 형태로 파일을 생성한다.
- 참고:
  - https://github.com/rany2/edge-tts
  - https://elevenlabs.io/docs/api-reference/text-to-speech/stream
  - https://elevenlabs.io/docs/api-reference/text-to-speech/v-1-text-to-speech-voice-id-stream-input
  - https://elevenlabs.io/docs/eleven-api/quickstart

### 보안 원칙

- `ELEVENLABS_API_KEY`는 prod ElevenLabs provider에서만 필요하며, 백엔드 환경 변수로만 읽는다.
- WebSocket payload, Unreal config, 문서 샘플에 실제 키를 넣지 않는다.
- TTS 비활성 또는 실패 시 agent 응답은 정상 반환하고 `payload.tts.status`만 `disabled` 또는 `failed`로 둔다.
- 외부 API 호출은 타임아웃, 최대 입력 길이, 캐시 키 기반 중복 방지로 비용을 제한한다.
- MVP에서는 agent 응답 경로에서 짧은 timeout으로 TTS를 동기 시도한다. timeout 또는 provider 실패 시 즉시 `payload.tts.status="failed"`를 붙이고 텍스트 응답을 반환한다.

### 응답 계약

agent 응답 payload에 아래 필드를 추가한다.

```json
{
  "final_answer": "컨베이어가 멈췄다면 전력과 출력 저장 공간을 먼저 확인하세요.",
  "tts": {
    "status": "ready",
    "provider": "edge_tts",
    "audio_url": "/tts/operator_guide/sha256-key.mp3",
    "content_type": "audio/mpeg",
    "text": "컨베이어가 멈췄다면 전력과 출력 저장 공간을 먼저 확인하세요.",
    "text_hash": "sha256-key",
    "voice_id": "ko-KR-SunHiNeural",
    "model_id": "edge_tts",
    "cached": false
  }
}
```

prod에서는 같은 구조에서 `provider`가 `elevenlabs`, `voice_id`가 ElevenLabs voice ID, `model_id`가 `eleven_multilingual_v2`가 된다.

실패 시 payload 예시:

```json
{
  "tts": {
    "status": "failed",
    "provider": "edge_tts",
    "error_code": "TTS_PROVIDER_ERROR"
  }
}
```

## 1. 파일 구조

### Backend 상태 및 수정 대상

- `backend/src/tts/__init__.py`
  - TTS 패키지 공개 표면.
- `backend/src/tts/settings.py`
  - 환경 변수 파싱과 기본값 관리.
  - `FACTORY_TTS_PROVIDER`, `FACTORY_ENV`, `APP_ENV`, `ENVIRONMENT`, `ELEVENLABS_API_KEY`, `FACTORY_TTS_ENABLED`, `FACTORY_TTS_STORAGE_PATH`, `FACTORY_TTS_PUBLIC_BASE_URL`, `EDGE_TTS_VOICE`, `EDGE_TTS_RATE`, `EDGE_TTS_VOLUME`, `EDGE_TTS_PITCH`, `ELEVENLABS_VOICE_ID`, `ELEVENLABS_MODEL_ID`, `ELEVENLABS_OUTPUT_FORMAT`, `FACTORY_TTS_MAX_CHARS`, `FACTORY_TTS_TIMEOUT_SECONDS`.
- `backend/src/tts/schemas.py`
  - 현재 워크트리의 `TTSRequest`, `TTSMetadata` Pydantic 모델을 유지하거나, 구현 중 이름을 바꾸는 경우 모든 import와 테스트를 함께 갱신한다.
- `backend/src/tts/text_selection.py`
  - agent별 payload에서 읽을 문장 선택.
  - 긴 문장 trim, markdown 제거, 공백 정리.
- `backend/src/tts/storage.py`
  - 캐시 키 생성, mp3 파일 저장, 정적 URL 생성.
- `backend/src/tts/router.py`
  - `/tts/{agent}/{audio_key}.mp3` 제한 다운로드 endpoint.
  - agent allowlist와 SHA-256 filename regex를 통과한 MP3만 반환한다.
- `backend/src/tts/elevenlabs_client.py`
  - ElevenLabs HTTP 호출 담당.
  - `urllib.request` 또는 `http.client` 기반으로 구현해 새 런타임 의존성을 최소화한다.
- `backend/src/tts/edge_tts_client.py`
  - dev/local용 `edge-tts` 호출 담당.
  - `edge_tts.Communicate(...).save(...)`를 사용해 임시 파일에 MP3를 생성한다.
- `backend/src/tts/service.py`
  - 설정, 캐시, provider 호출을 조합하는 `TTSService`.

### Backend 수정

- `backend/src/app.py`
  - `tts.router`를 include한다.
  - `StaticFiles`로 `FACTORY_TTS_STORAGE_PATH` 전체를 mount하지 않는다.
- `backend/src/agents/pipeline/runtime.py`
  - `build_agent_response` 직전 또는 내부에서 `operator_guide`, `process_optimizer` payload에 `tts` 추가.
  - 주입 지점은 `metadata`를 붙이기 전 `responsePayload` 기준이 가장 단순하다.
- `backend/pyproject.toml`
  - `edge-tts>=7.0,<8.0` dependency를 추가한다.
  - ElevenLabs는 `urllib.request` 또는 `http.client` 기반으로 구현해 `requests`/`httpx` 의존성을 추가하지 않는다.

### Backend 테스트 생성/수정

- `backend/tests/test_tts_text_selection.py`
  - agent별 TTS 문장 선택 규칙.
- `backend/tests/test_tts_storage.py`
  - cache key, URL, 파일 경로 traversal 방지.
- `backend/tests/test_tts_static_route.py`
  - 유효한 `/tts/{agent}/{sha256}.mp3` 다운로드와 invalid agent, non-hash filename, traversal, `.env` 접근 차단 검증.
- `backend/tests/test_tts_service.py`
  - disabled, cache hit, provider success/failure.
- `backend/tests/test_pipeline_tts.py`
  - `AgentPipeline` 응답 payload에 `tts`가 붙는지 검증.

### Frontend 생성

- `frontend/Source/Wanted_Factory/Public/FactoryAgentTTSPlaybackSubsystem.h`
  - agent 응답의 `tts.audio_url`을 받아 재생하는 GameInstance subsystem.
- `frontend/Source/Wanted_Factory/Private/FactoryAgentTTSPlaybackSubsystem.cpp`
  - HTTP 다운로드, 임시 파일 저장, 오디오 재생.
  - 프로젝트에 런타임 MP3 decode 플러그인이 없으면 이 파일은 WAV/PCM 또는 플러그인 도입 전까지 no-op + log로 시작한다.

### Frontend 수정

- `frontend/Source/Wanted_Factory/Wanted_Factory.Build.cs`
  - HTTP 다운로드가 필요하면 `HTTP` 모듈 추가.
  - 런타임 오디오 importer 플러그인을 도입하면 해당 모듈을 별도 task에서 추가한다.
- `frontend/Source/Wanted_Factory/Private/FactoryAgentClientSubsystem.cpp`
  - `agent.response` payload를 기존 delegate로 계속 넘긴다. 가능하면 기존 계약은 유지한다.
- `frontend/Source/Wanted_Factory/UI/UI_DialogueBalloon.h`
  - `PlayTTSFromPayload` private helper 선언.
- `frontend/Source/Wanted_Factory/UI/UI_DialogueBalloon.cpp`
  - `HandleOnOperatorGuideResponse`, `HandleOnProcessOptimizerResponse`에서 대화창 표시 후 `payload.tts.audio_url` 재생 요청.

### 문서 수정

- `backend/docs/agent_request_contract.md`
  - `agent.response.payload.tts` 필드 설명 추가.
- `backend/docs/unreal_agent_json_examples.md`
  - operator_guide/process_optimizer 응답 예시에 `tts` 추가.

## 2. Task 1: Backend TTS 설정과 스키마

**Files:**
- Create: `backend/src/tts/__init__.py`
- Create: `backend/src/tts/settings.py`
- Create: `backend/src/tts/schemas.py`
- Test: `backend/tests/test_tts_service.py`

- [ ] **Step 1: Write failing tests for disabled/default settings**

```python
from __future__ import annotations

from tts.settings import TTSSettings


def test_tts_settings_dev_defaults_to_edge_tts_without_api_key(monkeypatch, tmp_path) -> None:
    monkeypatch.delenv("ELEVENLABS_API_KEY", raising=False)
    monkeypatch.delenv("FACTORY_TTS_PROVIDER", raising=False)
    monkeypatch.setenv("FACTORY_TTS_ENABLED", "true")
    monkeypatch.setenv("FACTORY_ENV", "development")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is True
    assert settings.provider == "edge_tts"
    assert settings.voice_id == "ko-KR-SunHiNeural"
    assert settings.model_id == "edge_tts"


def test_tts_settings_prod_elevenlabs_requires_api_key(monkeypatch, tmp_path) -> None:
    monkeypatch.delenv("ELEVENLABS_API_KEY", raising=False)
    monkeypatch.delenv("FACTORY_TTS_PROVIDER", raising=False)
    monkeypatch.setenv("FACTORY_TTS_ENABLED", "true")
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.provider == "elevenlabs"
    assert settings.disabled_reason == "missing_api_key"


def test_tts_settings_prod_uses_elevenlabs_defaults(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is True
    assert settings.provider == "elevenlabs"
    assert settings.model_id == "eleven_multilingual_v2"
    assert settings.output_format == "mp3_44100_128"
    assert settings.max_chars == 600


def test_tts_settings_staging_uses_elevenlabs(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_ENV", "staging")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is True
    assert settings.provider == "elevenlabs"


def test_tts_settings_local_uses_edge_tts(monkeypatch, tmp_path) -> None:
    monkeypatch.delenv("ELEVENLABS_API_KEY", raising=False)
    monkeypatch.setenv("FACTORY_ENV", "local")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is True
    assert settings.provider == "edge_tts"


def test_tts_settings_rejects_conflicting_environment_names(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("FACTORY_ENV", "development")
    monkeypatch.setenv("APP_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.disabled_reason == "conflicting_environment"


def test_tts_settings_rejects_edge_override_in_production(monkeypatch, tmp_path) -> None:
    monkeypatch.delenv("ELEVENLABS_API_KEY", raising=False)
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "edge_tts")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.provider == "edge_tts"
    assert settings.disabled_reason == "invalid_provider_for_environment"


def test_tts_settings_rejects_elevenlabs_override_in_development(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_ENV", "development")
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "elevenlabs")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.provider == "elevenlabs"
    assert settings.disabled_reason == "invalid_provider_for_environment"


def test_tts_settings_rejects_unknown_provider(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("FACTORY_TTS_PROVIDER", "unknown")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.disabled_reason == "invalid_provider"


def test_tts_settings_rejects_non_mp3_elevenlabs_output_format(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("ELEVENLABS_API_KEY", "test-key")
    monkeypatch.setenv("FACTORY_ENV", "production")
    monkeypatch.setenv("ELEVENLABS_OUTPUT_FORMAT", "pcm_44100")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    settings = TTSSettings.from_env()

    assert settings.enabled is False
    assert settings.disabled_reason == "invalid_output_format"
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cd backend
uv run pytest tests/test_tts_service.py -q
```

Expected: FAIL on the new/updated settings cases until provider validation and override behavior match this plan. In the current worktree, `backend/src/tts` already exists, so do not expect `ModuleNotFoundError`.

- [ ] **Step 3: Implement settings and schemas**

Implementation rules:

- Parse booleans case-insensitively: `1`, `true`, `yes`, `on`.
- Read environment names in precedence order: `FACTORY_ENV`, `APP_ENV`, then `ENVIRONMENT`.
- Classify `dev`, `development`, `local`, `test` as dev class and `prod`, `production`, `staging` as prod class.
- If more than one env var is set and they point to different classes, set `enabled=False` and `disabled_reason="conflicting_environment"`.
- Resolve `FACTORY_TTS_PROVIDER=auto` from the highest-precedence non-conflicting environment name: dev/local/test -> `edge_tts`, prod/staging -> `elevenlabs`.
- Reject cross-environment provider overrides with `enabled=False` and `disabled_reason="invalid_provider_for_environment"`.
- If the resolved provider is `edge_tts`, do not require `ELEVENLABS_API_KEY`.
- If the resolved provider is `elevenlabs`, require `ELEVENLABS_API_KEY`.
- If `FACTORY_TTS_ENABLED=false`, keep disabled even when API key exists.
- If ElevenLabs is enabled but API key is missing, set `enabled=False` and `disabled_reason="missing_api_key"`.
- Clamp `max_chars` to `100..1200`.
- Default timeout is `2.0` seconds for gameplay responsiveness.
- Clamp timeout parsed by `TTSSettings.from_env()` to `0.5..8.0` seconds. Direct test construction of `TTSSettings(timeout_seconds=...)` may use shorter values to exercise timeout behavior without slowing tests.

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
cd backend
uv run pytest tests/test_tts_service.py -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/src/tts backend/tests/test_tts_service.py
git commit -m "feat: add tts runtime settings"
```

## 3. Task 2: Agent별 TTS 문장 선택

**Files:**
- Create: `backend/src/tts/text_selection.py`
- Test: `backend/tests/test_tts_text_selection.py`

- [ ] **Step 1: Write failing tests**

```python
from __future__ import annotations

from tts.text_selection import select_tts_text


def test_operator_guide_uses_final_answer() -> None:
    text = select_tts_text(
        "operator_guide",
        {"final_answer": "분쇄기는 원석을 다음 단계 재료로 가공하는 장비입니다."},
    )

    assert text == "분쇄기는 원석을 다음 단계 재료로 가공하는 장비입니다."


def test_process_optimizer_uses_explicit_tts_text_before_summary() -> None:
    text = select_tts_text(
        "process_optimizer",
        {
            "tts": {"text": "발견한 문제를 강조 표시했습니다."},
            "summary": "제련기 출력이 막혀 생산량이 낮습니다.",
            "optimization_alert": {"recommended_action": "출력 저장 공간을 비우세요."},
        },
    )

    assert text == "발견한 문제를 강조 표시했습니다."


def test_process_optimizer_uses_display_message_before_summary() -> None:
    text = select_tts_text(
        "process_optimizer",
        {
            "display_message": "문제가 발견되지 않았습니다.",
            "summary": "전체 상태는 안정적입니다.",
        },
    )

    assert text == "문제가 발견되지 않았습니다."


def test_process_optimizer_skips_highlight_only_alert_without_display_text() -> None:
    text = select_tts_text(
        "process_optimizer",
        {
            "operation": "state_update",
            "optimization_alert": {
                "needed": True,
                "problem": "전력이 부족합니다.",
                "recommended_action": "발전기를 추가하세요.",
            }
        },
    )

    assert text is None


def test_text_selection_strips_markdown_and_limits_length() -> None:
    long_text = "**주의:** " + ("가" * 900)

    text = select_tts_text("operator_guide", {"final_answer": long_text}, max_chars=20)

    assert text == "주의: " + ("가" * 16)
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cd backend
uv run pytest tests/test_tts_text_selection.py -q
```

Expected: FAIL on the current-worktree behavior if `process_optimizer` still uses `summary` or `optimization_alert` text instead of only `payload.tts.text` / `payload.display_message`.
Existing current-worktree tests named `test_process_optimizer_uses_summary_first` and `test_process_optimizer_builds_short_alert_text` must be replaced by the new cases above; do not keep tests that require `summary` or raw `optimization_alert` fallback for `process_optimizer` TTS.

- [ ] **Step 3: Implement text selection**

Implementation rules:

- Return `None` when there is no player-facing text.
- For `process_optimizer`, select only `payload.tts.text` or `payload.display_message`.
- For `process_optimizer` highlight-only `state_update` responses without a display string, return `None` and do not synthesize audio.
- Strip `**`, backticks, repeated whitespace, and leading bullet markers.
- Do not include raw metadata, IDs, source titles, or middleware logs.
- Keep Korean punctuation as-is.

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
cd backend
uv run pytest tests/test_tts_text_selection.py -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/src/tts/text_selection.py backend/tests/test_tts_text_selection.py
git commit -m "feat: select agent text for tts"
```

## 4. Task 3: TTS storage cache

**Files:**
- Create: `backend/src/tts/storage.py`
- Test: `backend/tests/test_tts_storage.py`

- [ ] **Step 1: Write failing tests**

```python
from __future__ import annotations

import pytest

from tts.storage import TTSAudioStorage


def test_storage_key_is_stable_and_agent_scoped(tmp_path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")

    first = storage.build_key(
        agent="operator_guide",
        text="같은 문장",
        voice_id="voice-a",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
    )
    second = storage.build_key(
        agent="operator_guide",
        text="같은 문장",
        voice_id="voice-a",
        model_id="eleven_multilingual_v2",
        output_format="mp3_44100_128",
    )

    assert first == second
    assert "/" not in first


def test_storage_writes_mp3_under_agent_directory(tmp_path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")
    valid_key = "a" * 64

    result = storage.write_audio(
        agent="operator_guide",
        key=valid_key,
        audio_bytes=b"mp3-bytes",
        extension="mp3",
    )

    assert result.path == tmp_path / "operator_guide" / f"{valid_key}.mp3"
    assert result.path.read_bytes() == b"mp3-bytes"
    assert result.audio_url == f"/tts/operator_guide/{valid_key}.mp3"


def test_storage_rejects_invalid_agent(tmp_path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")

    with pytest.raises(ValueError):
        storage.write_audio(
            agent="unknown",
            key="a" * 64,
            audio_bytes=b"mp3-bytes",
            extension="mp3",
        )


def test_storage_rejects_traversal_like_key(tmp_path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")

    with pytest.raises(ValueError):
        storage.write_audio(
            agent="operator_guide",
            key="../secret",
            audio_bytes=b"mp3-bytes",
            extension="mp3",
        )


def test_storage_rejects_non_mp3_extension(tmp_path) -> None:
    storage = TTSAudioStorage(tmp_path, public_base_url="/tts")

    with pytest.raises(ValueError):
        storage.write_audio(
            agent="operator_guide",
            key="a" * 64,
            audio_bytes=b"wav-bytes",
            extension="wav",
        )
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cd backend
uv run pytest tests/test_tts_storage.py -q
```

Expected: FAIL on the current-worktree behavior if storage still permits an invalid agent directory, traversal-like key, or non-MP3 extension.

- [ ] **Step 3: Implement storage**

Implementation rules:

- Hash input as `sha256(agent + "\0" + voice_id + "\0" + model_id + "\0" + output_format + "\0" + text)`.
- Agent directory allowlist: `operator_guide`, `process_optimizer`.
- Reject any caller-provided key that is not a lowercase 64-character SHA-256 hex string.
- Reject any extension other than `mp3`.
- Use atomic-ish write: write to `.tmp` path, then replace target path.
- Build public URL without exposing absolute local path.

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
cd backend
uv run pytest tests/test_tts_storage.py -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/src/tts/storage.py backend/tests/test_tts_storage.py
git commit -m "feat: cache tts audio files"
```

## 5. Task 4: Provider clients and service

**Files:**
- Modify: `backend/pyproject.toml`
- Create: `backend/src/tts/edge_tts_client.py`
- Create: `backend/src/tts/elevenlabs_client.py`
- Create/Modify: `backend/src/tts/service.py`
- Test: `backend/tests/test_tts_service.py`

- [ ] **Step 1: Add failing service tests**

```python
from __future__ import annotations

import time

from tts.schemas import TTSRequest
from tts.service import TTSService
from tts.settings import TTSSettings


class FakeProvider:
    provider = "edge_tts"

    def __init__(self) -> None:
        self.calls: list[str] = []

    def synthesize(self, text: str) -> bytes:
        self.calls.append(text)
        return b"mp3-bytes"


def test_tts_service_returns_disabled_status(tmp_path) -> None:
    settings = TTSSettings(
        api_key=None,
        enabled=False,
        provider="edge_tts",
        storage_path=tmp_path,
    )
    service = TTSService(settings=settings, provider=FakeProvider())

    result = service.synthesize_for_payload(
        TTSRequest(agent="operator_guide", payload={"final_answer": "안내입니다."})
    )

    assert result.status == "disabled"
    assert result.error_code is None
    assert result.audio_url is None


def test_tts_service_disabled_status_preserves_reason(tmp_path) -> None:
    settings = TTSSettings(
        api_key=None,
        enabled=False,
        provider="edge_tts",
        disabled_reason="conflicting_environment",
        storage_path=tmp_path,
    )
    service = TTSService(settings=settings, provider=FakeProvider())

    result = service.synthesize_for_payload(
        TTSRequest(agent="operator_guide", payload={"final_answer": "안내입니다."})
    )

    assert result.status == "disabled"
    assert result.error_code == "conflicting_environment"
    assert result.audio_url is None


def test_tts_service_generates_and_caches_audio(tmp_path) -> None:
    provider = FakeProvider()
    settings = TTSSettings(
        api_key=None,
        enabled=True,
        provider="edge_tts",
        voice_id="ko-KR-SunHiNeural",
        model_id="edge_tts",
        storage_path=tmp_path,
    )
    service = TTSService(settings=settings, provider=provider)
    request = TTSRequest(
        agent="operator_guide",
        payload={"final_answer": "안내입니다."},
    )

    first = service.synthesize_for_payload(request)
    second = service.synthesize_for_payload(request)

    assert first.status == "ready"
    assert first.provider == "edge_tts"
    assert first.cached is False
    assert second.status == "ready"
    assert second.cached is True
    assert provider.calls == ["안내입니다."]


class SlowProvider:
    provider = "edge_tts"

    def synthesize(self, text: str) -> bytes:
        time.sleep(0.2)
        return b"late-bytes"


def test_tts_service_times_out_without_blocking_text_response_too_long(tmp_path) -> None:
    settings = TTSSettings(
        api_key=None,
        enabled=True,
        provider="edge_tts",
        voice_id="ko-KR-SunHiNeural",
        model_id="edge_tts",
        timeout_seconds=0.01,
        storage_path=tmp_path,
    )
    service = TTSService(settings=settings, provider=SlowProvider())

    started = time.perf_counter()
    result = service.synthesize_for_payload(
        TTSRequest(agent="operator_guide", payload={"final_answer": "안내입니다."})
    )
    elapsed = time.perf_counter() - started

    assert result.status == "failed"
    assert result.error_code == "TTS_TIMEOUT"
    assert result.audio_url is None
    assert elapsed < 0.1
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cd backend
uv run pytest tests/test_tts_service.py -q
```

Expected: FAIL until `TTSService` enforces provider timeout without waiting for the full slow provider duration. Do not satisfy this requirement with a provider that immediately raises `TimeoutError`; keep the blocking `SlowProvider` regression so service-level timeout enforcement is verified.

- [ ] **Step 3: Add edge-tts dependency**

Modify `backend/pyproject.toml`:

```toml
dependencies = [
    "... existing dependencies ...",
    "edge-tts>=7.0,<8.0",
]
```

Run:

```bash
cd backend
uv sync
```

Expected: dependency lock/install succeeds.

- [ ] **Step 4: Implement edge-tts client**

Implementation rules:

- Use `edge_tts.Communicate(text=text, voice=voice_id, rate=edge_rate, volume=edge_volume, pitch=edge_pitch)`.
- Save to a temporary `.mp3` file with `await communicate.save(temp_path)`.
- Read bytes from the temp file and delete the temp file.
- Provide a synchronous wrapper in `EdgeTTSClient.synthesize(text: str) -> bytes` that applies the same `settings.timeout_seconds` limit in both branches:
  - when no event loop is running, call `asyncio.run(asyncio.wait_for(_save_async(...), timeout=settings.timeout_seconds))`;
  - when an event loop is already running, create a short-lived thread that runs the async save and wait on the future with the same timeout.
- This timeout rule is required because the WebSocket gateway runs `pipeline.run` through `asyncio.to_thread`; the common request path can otherwise hit the no-running-loop branch and bypass the timeout.
- Convert `edge_tts.exceptions.NoAudioReceived`, `WebSocketError`, and `UnexpectedResponse` into `TTS_PROVIDER_ERROR`.
- Convert `asyncio.TimeoutError` and `concurrent.futures.TimeoutError` into `TTS_TIMEOUT` without wrapping away the timeout type/cause.
- Add focused provider/client tests that monkeypatch `edge_tts.Communicate(...).save(...)` to exceed the timeout in both no-running-loop and running-loop branches.
- Those provider/client tests must assert elapsed time is bounded by `settings.timeout_seconds` tolerance, not only that the final service result uses `error_code="TTS_TIMEOUT"`.
- Do not log full player text.

- [ ] **Step 5: Implement ElevenLabs client**

Implementation rules:

- URL:

```text
https://api.elevenlabs.io/v1/text-to-speech/{voice_id}/stream?output_format={output_format}
```

- Headers:

```text
xi-api-key: <api key>
Content-Type: application/json
Accept: audio/mpeg
```

- Keep `ELEVENLABS_OUTPUT_FORMAT` constrained to values starting with `mp3_` for this MVP.
- If a non-MP3 value is configured, disable TTS with `disabled_reason="invalid_output_format"` instead of saving a non-MP3 file behind a `.mp3` URL.
- Keep storage extension `mp3` and `content_type="audio/mpeg"` while this constraint is active.

- Body:

```json
{
  "text": "플레이어에게 읽어줄 문장",
  "model_id": "eleven_multilingual_v2",
  "voice_settings": {
    "stability": 0.5,
    "similarity_boost": 0.8,
    "speed": 1.0
  }
}
```

- Convert provider errors into `TTS_PROVIDER_ERROR`; never raise into the agent pipeline.
- Log status code and short reason only. Do not log API key or full player text.

- [ ] **Step 6: Implement service**

Implementation rules:

- Choose provider from settings:
  - `edge_tts` -> `EdgeTTSClient`
  - `elevenlabs` -> `ElevenLabsClient`
- If disabled, return `{"status": "disabled", "provider": settings.provider}` and include `error_code=settings.disabled_reason` when a disabled reason exists.
- If no selected text, return `{"status": "skipped", "provider": settings.provider}`.
- Run provider synthesis behind a bounded timeout using `settings.timeout_seconds`.
- On timeout, return `{"status": "failed", "provider": settings.provider, "error_code": "TTS_TIMEOUT"}` and do not block the agent text response beyond the configured timeout.
- Implement the timeout at the `TTSService` layer as well as provider clients, so custom providers and slow blocking provider calls cannot bypass the response budget.
- On cache hit, return `ready` with `cached=true`.
- On success, write file and return `ready` with `cached=false`.
- On provider/storage error, return `failed` with `error_code`, not exception.

- [ ] **Step 7: Run test to verify it passes**

Run:

```bash
cd backend
uv run pytest tests/test_tts_service.py tests/test_tts_storage.py tests/test_tts_text_selection.py -q
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add backend/pyproject.toml backend/uv.lock backend/src/tts backend/tests/test_tts_service.py
git commit -m "feat: synthesize agent speech with provider clients"
```

## 6. Task 5: Pipeline response integration

**Files:**
- Modify: `backend/src/agents/pipeline/runtime.py`
- Test: `backend/tests/test_pipeline_tts.py`

- [ ] **Step 1: Write failing pipeline tests**

```python
from __future__ import annotations

from agents.pipeline.runtime import AgentPipeline
from llm.adapter import LLMAdapter


class StubLLM(LLMAdapter):
    def __init__(self, responses: list[str | None]) -> None:
        self.responses = responses

    def invoke(self, prompt: str) -> str | None:
        if not self.responses:
            return None
        return self.responses.pop(0)


class FakeTTSService:
    def __init__(self) -> None:
        self.requests = []

    def synthesize_for_payload(self, request):
        self.requests.append(request)
        text = (
            request.payload.get("tts", {}).get("text")
            or request.payload.get("display_message")
            or request.payload.get("final_answer")
        )
        return {
            "status": "ready",
            "provider": "edge_tts",
            "audio_url": f"/tts/{request.agent}/fake.mp3",
            "content_type": "audio/mpeg",
            "text": text,
            "text_hash": "fake",
            "voice_id": "ko-KR-SunHiNeural",
            "model_id": "edge_tts",
            "cached": False,
        }


def test_operator_guide_response_includes_tts_metadata() -> None:
    llm = StubLLM(
        [
            "operator_guide.machine_help",
            '{"final_answer":"컨베이어를 확인하세요.","actions":[],"question":"막혔어","topic":"troubleshooting"}',
        ]
    )
    pipeline = AgentPipeline(llm=llm, tts_service=FakeTTSService())

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "tts-1",
            "agent": "operator_guide",
            "payload": {"question": "막혔어"},
        }
    )

    assert response["type"] == "agent.response"
    assert response["payload"]["tts"]["status"] == "ready"
    assert response["payload"]["tts"]["audio_url"] == "/tts/operator_guide/fake.mp3"


def test_process_optimizer_no_alert_response_gets_backend_display_message_before_tts() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    prepared = pipeline._prepare_process_optimizer_tts_payload(
        {
            "status": "preview",
            "summary": "전체 상태는 안정적입니다.",
            "optimization_alert": {"needed": False},
        }
    )
    response = pipeline._attach_tts(agent="process_optimizer", payload=prepared)

    assert response["display_message"] == "문제가 발견되지 않았습니다."
    assert response["tts"]["text"] == "문제가 발견되지 않았습니다."
    assert service.requests[0].payload["display_message"] == "문제가 발견되지 않았습니다."


def test_pipeline_process_optimizer_preview_prepares_display_message_before_tts() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    response = pipeline.run(
        {
            "type": "agent.request",
            "request_id": "process-tts-1",
            "agent": "process_optimizer",
            "payload": {
                "operation": "analyze",
                "goal": "balance",
                "factoryRevision": 23,
                "factory_state": {
                    "machines": [
                        {
                            "id": "smelter_v2",
                            "type": "smelter",
                            "status": "operating",
                            "inputs": [{"item_id": "iron_ore", "amount": 10.0}],
                        }
                    ],
                    "conveyors": [],
                    "power_grid": {"produced": 50.0, "consumed": 10.0},
                },
            },
        }
    )

    assert response["type"] == "agent.response"
    assert response["payload"]["display_message"] == "문제가 발견되지 않았습니다."
    assert response["payload"]["tts"]["text"] == response["payload"]["display_message"]
    assert service.requests[0].payload["display_message"] == "문제가 발견되지 않았습니다."


def test_process_optimizer_display_message_and_tts_text_stay_aligned() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    response = pipeline._attach_tts(
        agent="process_optimizer",
        payload={
            "status": "preview",
            "display_message": "문제가 발견되지 않았습니다.",
            "summary": "전체 상태는 안정적입니다.",
        },
    )

    assert response["tts"]["status"] == "ready"
    assert response["tts"]["text"] == "문제가 발견되지 않았습니다."
    assert service.requests[0].payload["display_message"] == "문제가 발견되지 않았습니다."


def test_process_optimizer_highlight_only_state_update_excludes_tts() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    response = pipeline._attach_tts(
        agent="process_optimizer",
        payload={
            "operation": "state_update",
            "optimization_alert": {
                "needed": True,
                "problem": "전력이 부족합니다.",
                "recommended_action": "발전기를 추가하세요.",
            },
        },
    )

    assert "tts" not in response
    assert service.requests == []


def test_process_optimizer_tts_text_hint_is_synthesized_not_skipped() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    response = pipeline._attach_tts(
        agent="process_optimizer",
        payload={
            "status": "preview",
            "tts": {"text": "발견한 문제를 강조 표시했습니다."},
            "summary": "제련기 출력이 막혀 생산량이 낮습니다.",
        },
    )

    assert response["tts"]["status"] == "ready"
    assert response["tts"]["audio_url"] == "/tts/process_optimizer/fake.mp3"
    assert response["tts"]["text"] == "발견한 문제를 강조 표시했습니다."


def test_process_optimizer_terminal_tts_metadata_is_preserved() -> None:
    service = FakeTTSService()
    pipeline = AgentPipeline(llm=StubLLM([]), tts_service=service)

    failed_tts = {
        "status": "failed",
        "provider": "edge_tts",
        "error_code": "TTS_TIMEOUT",
    }
    response = pipeline._attach_tts(
        agent="process_optimizer",
        payload={
            "status": "preview",
            "display_message": "문제가 발견되지 않았습니다.",
            "tts": failed_tts,
        },
    )

    assert response["tts"] == failed_tts
    assert service.requests == []
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cd backend
uv run pytest tests/test_pipeline_tts.py -q
```

Expected: FAIL until `AgentPipeline` both accepts `tts_service` and enforces the `process_optimizer` display/TTS alignment rules.

- [ ] **Step 3: Inject TTSService into AgentPipeline**

Implementation rules:

- Add optional constructor argument `tts_service: TTSService | None = None`.
- Lazily create default `TTSService.from_env()` when not provided.
- Add private helper `_prepare_process_optimizer_tts_payload(payload: dict[str, Any]) -> dict[str, Any]`.
- Add private helper `_attach_tts(agent: str, payload: dict[str, Any]) -> dict[str, Any]`.
- Return a new payload dict. Do not mutate `state["responsePayload"]`.
- Attach TTS only for `operator_guide` and `process_optimizer`.
- Before `_attach_tts`, call `_prepare_process_optimizer_tts_payload` for real `process_optimizer` responses.
- `_prepare_process_optimizer_tts_payload` must preserve existing `payload.tts.text` or `payload.display_message`.
- For no-alert `process_optimizer` analyze/preview responses, add `display_message="문제가 발견되지 않았습니다."` so the backend-selected display text and TTS text are the same.
- For highlight-only `state_update` alert responses, do not synthesize a display string just for TTS.
- For `process_optimizer`, do not call `tts_service` when `select_tts_text(...)` returns `None`.
- For `process_optimizer`, the backend-selected display string and `payload.tts.text` must stay identical when TTS is attached.
- If payload already has complete generated TTS metadata such as `status` plus `audio_url` or a terminal `failed`/`disabled` status, leave it untouched.
- If payload has only `tts.text`, treat it as an input text hint, synthesize audio, and replace/merge it with generated TTS metadata.

- [ ] **Step 4: Run focused tests**

Run:

```bash
cd backend
uv run pytest tests/test_pipeline_tts.py tests/test_pipeline_edges.py tests/test_websocket_endpoint.py -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add backend/src/agents/pipeline/runtime.py backend/tests/test_pipeline_tts.py
git commit -m "feat: attach tts metadata to agent responses"
```

## 7. Task 6: Static audio serving

**Files:**
- Create: `backend/src/tts/router.py`
- Modify: `backend/src/app.py`
- Test: `backend/tests/test_tts_static_route.py`

- [ ] **Step 1: Write failing route test**

```python
from __future__ import annotations

from fastapi.testclient import TestClient

from app import create_app


def test_tts_route_serves_cached_file(monkeypatch, tmp_path) -> None:
    audio_key = "a" * 64
    audio_dir = tmp_path / "operator_guide"
    audio_dir.mkdir(parents=True)
    (audio_dir / f"{audio_key}.mp3").write_bytes(b"mp3-bytes")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    with TestClient(create_app()) as client:
        response = client.get(f"/tts/operator_guide/{audio_key}.mp3")

    assert response.status_code == 200
    assert response.content == b"mp3-bytes"


def test_tts_route_rejects_invalid_agent(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    with TestClient(create_app()) as client:
        response = client.get(f"/tts/unknown/{'a' * 64}.mp3")

    assert response.status_code == 403


def test_tts_route_rejects_non_hash_filename(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    with TestClient(create_app()) as client:
        response = client.get("/tts/operator_guide/sample.mp3")

    assert response.status_code == 400


def test_tts_route_does_not_expose_unrelated_files(monkeypatch, tmp_path) -> None:
    (tmp_path / ".env").write_text("ELEVENLABS_API_KEY=secret", encoding="utf-8")
    monkeypatch.setenv("FACTORY_TTS_STORAGE_PATH", str(tmp_path))

    with TestClient(create_app()) as client:
        response = client.get("/tts/.env")

    assert response.status_code == 404
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cd backend
uv run pytest tests/test_tts_static_route.py -q
```

Expected: FAIL until the constrained `/tts/{agent}/{audio_key}.mp3` route exists.

- [ ] **Step 3: Implement constrained TTS router**

Implementation rules:

- Do not use `StaticFiles` to mount the entire `FACTORY_TTS_STORAGE_PATH`.
- Add `APIRouter` route `GET /tts/{agent}/{audio_key}.mp3`.
- Allow only `agent in {"operator_guide", "process_optimizer"}`.
- Allow only `audio_key` matching `^[a-f0-9]{64}$`.
- Resolve the candidate path as `settings.storage_path / agent / f"{audio_key}.mp3"`.
- Verify the resolved path stays under `settings.storage_path.resolve()`.
- Return `FileResponse` with `media_type="audio/mpeg"` only when the file exists.
- Return `403` for invalid agent or traversal attempts.
- Return `400` for invalid audio key format.
- Return `404` for missing files or paths that do not match the `/tts/{agent}/{audio_key}.mp3` route.

- [ ] **Step 4: Include router in app factory**

Implementation rules:

- Import `tts.router`.
- Call `app.include_router(tts_router)` inside `create_app()`.
- Keep directory creation in `TTSService`/storage; the router should not expose broad directories.

- [ ] **Step 5: Run tests**

Run:

```bash
cd backend
uv run pytest tests/test_tts_static_route.py tests/test_websocket_endpoint.py -q
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add backend/src/tts/router.py backend/src/app.py backend/tests/test_tts_static_route.py
git commit -m "feat: serve generated tts audio"
```

## 8. Task 7: Unreal TTS playback skeleton

**Files:**
- Create: `frontend/Source/Wanted_Factory/Public/FactoryAgentTTSPlaybackSubsystem.h`
- Create: `frontend/Source/Wanted_Factory/Private/FactoryAgentTTSPlaybackSubsystem.cpp`
- Modify: `frontend/Source/Wanted_Factory/Wanted_Factory.Build.cs`

- [ ] **Step 1: Add HTTP dependency**

Modify `Wanted_Factory.Build.cs` private dependencies:

```csharp
"HTTP",
```

- [ ] **Step 2: Create playback subsystem interface**

Required C++ public API:

```cpp
UCLASS()
class WANTED_FACTORY_API UFactoryAgentTTSPlaybackSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Factory Agent|TTS")
    void PlayFromUrl(const FString& AudioUrl);

    UFUNCTION(BlueprintCallable, Category = "Factory Agent|TTS")
    void StopCurrent();

    UFUNCTION(BlueprintPure, Category = "Factory Agent|TTS")
    bool IsPlaybackAvailable() const;
};
```

- [ ] **Step 3: Implement MVP behavior**

Implementation rules:

- Resolve relative `/tts/...` URL against the same agent server origin used by the WebSocket/client configuration. Use `http://127.0.0.1:18000` only as the local development default, not as a prod/staging hardcoded origin.
- Download with Unreal HTTP module.
- Save to `FPaths::ProjectSavedDir() / "AgentTTS"`.
- If runtime MP3 playback support is not available, log a clear warning and return without crashing.
- If a runtime audio plugin is approved, decode and play through `UGameplayStatics::PlaySound2D`.

- [ ] **Step 4: Compile Unreal module**

Run from the Unreal build environment:

```bash
./Engine/Build/BatchFiles/RunUAT.sh BuildCookRun -project=/absolute/path/to/Wanted_Factory.uproject -noP4 -build -skipcook
```

Expected: C++ compile succeeds.

- [ ] **Step 5: Commit**

```bash
git add frontend/Source/Wanted_Factory/Public/FactoryAgentTTSPlaybackSubsystem.h frontend/Source/Wanted_Factory/Private/FactoryAgentTTSPlaybackSubsystem.cpp frontend/Source/Wanted_Factory/Wanted_Factory.Build.cs
git commit -m "feat: add unreal agent tts playback subsystem"
```

## 9. Task 8: Dialogue UI integration

**Files:**
- Modify: `frontend/Source/Wanted_Factory/UI/UI_DialogueBalloon.h`
- Modify: `frontend/Source/Wanted_Factory/UI/UI_DialogueBalloon.cpp`

- [ ] **Step 1: Add helper declaration**

Add private helper:

```cpp
void PlayTTSFromPayload(const TSharedPtr<FJsonObject>& PayloadObject);
FString GetTTSTextFromPayload(const TSharedPtr<FJsonObject>& PayloadObject) const;
```

- [ ] **Step 2: Implement helper**

Behavior:

- Read `payload.tts.status`.
- Only play when status is `ready`.
- Read `payload.tts.audio_url`.
- Ignore empty URL.
- Get `UFactoryAgentTTSPlaybackSubsystem` from `GameInstance`.
- Call `PlayFromUrl(AudioUrl)`.
- `GetTTSTextFromPayload` reads `payload.tts.text`, trims it, and returns empty string when absent.

- [ ] **Step 3: Call helper for operator_guide**

In `HandleOnOperatorGuideResponse`, call `PlayTTSFromPayload(PayloadObject)` immediately after `ShowExternalDialogue(Answer)`.

- [ ] **Step 4: Use one display/TTS text for process_optimizer**

In `HandleOnProcessOptimizerResponse`, never play TTS for a different sentence than the one displayed.

Implementation rules:

- Read `const FString TTSDisplayText = GetTTSTextFromPayload(PayloadObject);`.
- When `TTSDisplayText` is non-empty, call `ShowExternalDialogue(TTSDisplayText)` and then `PlayTTSFromPayload(PayloadObject)`.
- When `TTSDisplayText` is empty, keep the current fallback text-only behavior:
  - `ShowExternalDialogue(TEXT("발견한 문제를 강조 표시했습니다"))`.
  - `ShowExternalDialogue(TEXT("문제가 발견되지 않았습니다"))`.
  - `ShowExternalDialogue(TEXT("문제의 해결을 확인했습니다"))`.
- Do not call `PlayTTSFromPayload` after fallback text-only messages.
- Add a backend pipeline test asserting `payload.tts.text` equals the process_optimizer display string selected by the backend for alert/no-alert responses.

- [ ] **Step 5: Compile Unreal module**

Expected: C++ compile succeeds without changing existing WebSocket delegate signatures.

- [ ] **Step 6: Commit**

```bash
git add frontend/Source/Wanted_Factory/UI/UI_DialogueBalloon.h frontend/Source/Wanted_Factory/UI/UI_DialogueBalloon.cpp
git commit -m "feat: play tts for agent dialogue messages"
```

## 10. Task 9: Docs and manual QA

**Files:**
- Modify: `backend/docs/agent_request_contract.md`
- Modify: `backend/docs/unreal_agent_json_examples.md`
- Optional Create: `docs/04_reviews/2026-07-06_agent_message_tts_review.md`

- [ ] **Step 1: Document environment variables**

Add:

```text
FACTORY_TTS_ENABLED=true
FACTORY_TTS_PROVIDER=auto
FACTORY_ENV=development
EDGE_TTS_VOICE=ko-KR-SunHiNeural
EDGE_TTS_RATE=+0%
EDGE_TTS_VOLUME=+0%
EDGE_TTS_PITCH=+0Hz
ELEVENLABS_API_KEY=<managed secret>
ELEVENLABS_VOICE_ID=JBFqnCBsd6RMkjVDRZzb
ELEVENLABS_MODEL_ID=eleven_multilingual_v2
ELEVENLABS_OUTPUT_FORMAT=mp3_44100_128
FACTORY_TTS_STORAGE_PATH=var/tts
FACTORY_TTS_PUBLIC_BASE_URL=/tts
FACTORY_TTS_MAX_CHARS=600
FACTORY_TTS_TIMEOUT_SECONDS=2
```

- [ ] **Step 2: Document response contract**

Add the `payload.tts` examples from this plan to both backend contract docs.

- [ ] **Step 3: Manual QA checklist**

```text
1. Start backend with FACTORY_TTS_ENABLED=false.
2. Ask operator_guide a question.
3. Confirm text answer appears and payload.tts.status is disabled.
4. Start backend with FACTORY_ENV=development and FACTORY_TTS_ENABLED=true.
5. Ask the same operator_guide question twice.
6. Confirm payload.tts.provider is edge_tts.
7. Confirm first response creates an mp3 under var/tts/operator_guide.
8. Confirm second response returns cached=true.
9. Start backend with FACTORY_ENV=production, FACTORY_TTS_PROVIDER=elevenlabs, ELEVENLABS_API_KEY, and FACTORY_TTS_ENABLED=true.
10. Confirm payload.tts.provider is elevenlabs and audio_url is still served from /tts.
11. Trigger process_optimizer state_update with an alert that only highlights a target.
12. Confirm no payload.tts.audio_url is returned for this highlight-only response.
13. Trigger a process_optimizer response with payload.tts.text/display_message.
14. Confirm Unreal displays that exact text and plays the matching audio_url.
15. In Unreal, confirm dialogue text still appears if audio playback is unavailable.
16. If runtime audio decode is available, confirm only one current TTS clip plays at a time.
```

- [ ] **Step 4: Commit**

```bash
git add backend/docs/agent_request_contract.md backend/docs/unreal_agent_json_examples.md
git commit -m "docs: document agent tts response contract"
```

## 11. Verification commands

### Backend

```bash
cd backend
uv run pytest tests/test_tts_text_selection.py tests/test_tts_storage.py tests/test_tts_service.py tests/test_pipeline_tts.py tests/test_tts_static_route.py -q
uv run pytest tests/test_pipeline_edges.py tests/test_websocket_endpoint.py -q
uv run ruff check src tests
```

### Backend smoke

```bash
cd backend
FACTORY_TTS_ENABLED=false FACTORY_ENV=development uv run uvicorn app:create_app --factory --host 127.0.0.1 --port 18000
```

Send:

```json
{
  "type": "agent.request",
  "request_id": "tts-smoke-operator-1",
  "session_id": "player-session-001",
  "client_id": "smoke-client",
  "agent": "operator_guide",
  "payload": {
    "question": "분쇄기가 뭐야?",
    "sub_agent": "operator_guide.machine_help"
  },
  "context": {
    "language": "ko",
    "mode": "gameplay"
  }
}
```

Expected:

```json
{
  "type": "agent.response",
  "agent": "operator_guide",
  "payload": {
    "final_answer": "...",
    "tts": {
      "status": "disabled",
      "provider": "edge_tts"
    }
  }
}
```

With dev TTS enabled:

```bash
cd backend
FACTORY_TTS_ENABLED=true FACTORY_ENV=development FACTORY_TTS_PROVIDER=auto uv run uvicorn app:create_app --factory --host 127.0.0.1 --port 18000
```

Expected:

- Response has `payload.tts.status == "ready"`.
- Response has `payload.tts.provider == "edge_tts"`.
- `payload.tts.audio_url` starts with `/tts/operator_guide/`.
- `GET http://127.0.0.1:18000{audio_url}` returns `200` and `audio/mpeg`.

With prod ElevenLabs key:

```bash
cd backend
FACTORY_TTS_ENABLED=true FACTORY_ENV=production FACTORY_TTS_PROVIDER=elevenlabs ELEVENLABS_API_KEY="$ELEVENLABS_API_KEY" uv run uvicorn app:create_app --factory --host 127.0.0.1 --port 18000
```

Expected:

- Response has `payload.tts.status == "ready"`.
- Response has `payload.tts.provider == "elevenlabs"`.
- `payload.tts.audio_url` starts with `/tts/operator_guide/`.
- `GET http://127.0.0.1:18000{audio_url}` returns `200` and `audio/mpeg`.

### Frontend

```text
1. Build the Unreal C++ module.
2. Connect to ws://127.0.0.1:18000/ws/agent.
3. Ask an operator guide question from the AI guide UI.
4. Confirm dialogue text appears as before.
5. Confirm no crash when tts.status is disabled, skipped, or failed.
6. Confirm audio plays when tts.status is ready and runtime decode support is installed.
```

## 12. Risk and fallback

- **Runtime MP3 playback in Unreal may need a plugin.**
  - Keep server response contract useful even before playback is complete.
  - First client pass may download and log audio path only.
- **edge-tts is suitable for dev/local but not a controlled production provider.**
  - Use it only for dev/local/test by default.
  - Keep prod/staging on ElevenLabs unless the team explicitly changes provider policy.
- **ElevenLabs latency/cost can affect UX.**
  - Cache by text/voice/model/output format.
  - Limit TTS text length.
  - Do not synthesize progress messages in MVP.
- **External API failure should not break gameplay.**
  - TTS service returns `failed`; agent response still returns text.
- **Secret exposure risk.**
  - API key stays server-side only.
  - No key in Unreal payload, saved response JSON, logs, or docs.
- **Repeated answer replay can annoy players.**
  - Client subsystem should stop current clip before playing new one.
  - Later option: add UI toggle for TTS on/off after MVP.

## 13. Self-review checklist

- Spec coverage: operator_guide final answers, process_optimizer user-facing messages, dev edge-tts integration, prod ElevenLabs integration, server-side secret handling, cache, Unreal playback, docs, tests are covered.
- Placeholder scan: no unfinished placeholder labels or unspecified “add tests” steps remain.
- Type consistency: `payload.tts.status`, `audio_url`, `voice_id`, `model_id`, `cached`, `provider` are consistently named.
- Scope check: material generation, quest generator, progress message TTS, and ElevenLabs WebSocket alignment are intentionally out of MVP scope.
