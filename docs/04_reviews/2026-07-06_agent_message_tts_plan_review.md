# 코드 리뷰: Agent Message TTS 구현 계획서

| 항목 | 내용 |
| --- | --- |
| 리뷰 일자 | 2026-07-06 |
| 리뷰 범위 | `docs/02_work_plans/2026-07-06-agent-message-tts-elevenlabs-plan.md` |
| 리뷰어 | Codex subagent `code-reviewer` |
| 리뷰 목적 | `operator_guide` / `process_optimizer` 메시지 TTS 계획의 보안성, 실행 가능성, 테스트 커버리지 검토 |
| 재리뷰 | 2026-07-06 Codex subagent `code-reviewer` |
| 2차 재리뷰 | 2026-07-06 Codex subagents `reviewer`, `docs-researcher`, `explorer` |
| 3차 재리뷰 | 2026-07-06 Codex subagent `reviewer` |
| 4차 재리뷰 | 2026-07-06 Codex subagents `reviewer`, `explorer` |
| 5차 재리뷰 | 2026-07-06 Codex subagent `reviewer` |
| 6차 재리뷰 | 2026-07-06 Codex subagent `reviewer` |
| 7차 재리뷰 | 2026-07-06 Codex subagent `reviewer` |
| 8차 재리뷰 | 2026-07-06 Codex subagent `reviewer` |

## 1. 변경 요약

- `operator_guide`와 `process_optimizer` agent 응답에 TTS 메타데이터를 붙이는 구현 계획을 검토했다.
- dev/local/test 환경은 `edge-tts`, prod/staging 환경은 ElevenLabs API를 사용하는 provider 분기 정책을 검토했다.
- 서버 측 secret 보관, 오디오 캐시, Unreal 재생 연동, 테스트 계획, 수동 QA 절차를 함께 점검했다.

---

## 2. 이슈 목록

### 2.1 High: `/tts` 정적 마운트의 파일 노출 위험

- 계획서가 `FACTORY_TTS_STORAGE_PATH` 전체를 `StaticFiles`로 `/tts`에 마운트하는 형태였고, storage path가 backend root나 project root로 잘못 설정되면 `.env` 등 비오디오 파일이 노출될 수 있었다.
- 조치:
  - `StaticFiles` 전체 마운트 방식을 제거했다.
  - `/tts/{agent}/{audio_key}.mp3` 제한 라우터로 변경했다.
  - agent allowlist, SHA-256 filename regex, traversal 차단, `.env` 접근 차단 테스트를 계획에 추가했다.

### 2.2 High: FastAPI 실행/테스트 대상 불일치

- 계획서의 테스트 예시는 `from app import app`, smoke command는 `uvicorn app:app`를 사용했지만, 현재 백엔드는 `create_app()` factory 패턴을 사용한다.
- 조치:
  - 테스트 예시를 `from app import create_app`와 `TestClient(create_app())`로 변경했다.
  - smoke command를 `uvicorn app:create_app --factory`로 변경했다.

### 2.3 High: `process_optimizer` 화면 문구와 음성 문구 불일치 가능성

- 백엔드가 `payload.summary`를 TTS 대상으로 합성하는 동안 Unreal은 “발견한 문제를 강조 표시했습니다” 같은 고정 문구를 보여줄 수 있어, 플레이어가 보는 문장과 듣는 문장이 달라질 수 있었다.
- 조치:
  - `process_optimizer`는 `payload.tts.text`를 표시/재생의 단일 source로 사용하도록 계획을 수정했다.
  - Unreal UI는 `tts.text`가 있으면 그 문장을 표시하고 같은 payload의 audio를 재생한다.
  - fallback 고정 문구에는 TTS를 재생하지 않도록 명시했다.

### 2.4 Medium: provider override 테스트 부족

- `FACTORY_TTS_PROVIDER=auto`, explicit override, dev/prod/staging/test 환경 분기 정책이 있었지만 테스트가 dev/prod 기본값만 검증했다.
- 조치:
  - `production + edge_tts override` 거부
  - `development + elevenlabs override` 거부
  - unknown provider
  - ElevenLabs missing key
  검증 테스트를 계획에 추가했다.

### 2.5 Medium: 동기 TTS 실패가 agent 응답을 지연시킬 수 있음

- TTS 합성을 agent pipeline 안에서 동기 수행하면 provider timeout까지 텍스트 응답이 지연될 수 있었다.
- 조치:
  - MVP 정책을 “짧은 bounded timeout으로 동기 시도”로 명확히 했다.
  - 기본 timeout을 2초로 낮추고 clamp 범위를 `0.5..8.0`초로 제한했다.
  - timeout 시 `payload.tts.status="failed"`, `error_code="TTS_TIMEOUT"`을 반환하는 테스트를 추가했다.

---

## 3. 우선순위 권고

- **High 이슈 3건은 구현 전 반드시 반영 필요**: 모두 계획서에 반영 완료.
- **Medium 이슈 2건은 회귀 방지용 테스트로 고정 필요**: 모두 계획서에 반영 완료.
- 구현 단계에서는 제한 라우터와 `process_optimizer` 표시 문구/TTS 문구 일치 테스트를 우선 검증해야 한다.

---

## 4. 검증 결과

- 계획서 placeholder 검색: 통과
- 계획서 텍스트 기준 기존 잘못된 표현 검색:
  - `app:app`: 제거 확인
  - `from app import app`: 제거 확인
  - `StaticFiles(` 직접 마운트 계획: 제거 확인
- 현재 워크트리에는 `backend/tests/test_tts_static_route.py`가 존재하므로, 계획서도 이 파일명을 유지하도록 수정했다.
- Markdown code fence 개수: 짝수 확인

---

## 5. 재리뷰 결과

재리뷰에서 다음 추가 문제가 확인되었다.

- `process_optimizer` state_update alert는 Unreal에서 highlight-only로 처리될 수 있으므로, 이 응답에 TTS를 붙이면 화면에 없는 문장이 재생될 수 있다.
- 현재 워크트리에 `backend/src/tts/`와 일부 TTS 테스트가 이미 존재하므로, 계획서의 “새 파일 생성 / ModuleNotFoundError 기대” 문구가 continuation 작업과 맞지 않았다.
- 현재 구현은 unknown provider를 `edge_tts`로 fallback할 수 있으므로, 계획서의 `invalid_provider` 정책과 어긋난다.
- timeout 테스트가 `TTS_TIMEOUT` 값만 확인하고 실제 짧은 시간 안에 반환되는지 검증하지 않았다.

조치:

- 계획서를 current worktree continuation plan으로 명시했다.
- `process_optimizer` highlight-only `state_update` 응답은 TTS를 생성하지 않도록 계획과 테스트를 수정했다.
- `test_tts_static_route.py` 파일명을 계획서에 맞췄다.
- unknown provider는 `invalid_provider`로 비활성화해야 한다는 테스트를 유지했다.
- timeout 테스트에 elapsed time assertion을 추가했다.

---

## 6. 최종 판단

2차 재리뷰에서 다음 추가 문제가 확인되었다.

- `process_optimizer` 표시 문구와 `payload.tts.text` 일치 검증이 Task 5의 실행 테스트에 충분히 들어가 있지 않았다.
- 텍스트 선택과 storage 단계에 아직 “missing module/function” 기대 문구가 남아 있어 current-worktree continuation plan과 충돌했다.
- ElevenLabs `output_format`은 non-MP3도 가능하지만, 계획서는 URL/Content-Type/storage를 MP3로 고정하고 있었다.
- 현재 구현의 `edge_tts` no-running-loop 경로가 timeout을 우회할 수 있어 WebSocket 실행 경로에서 응답 지연 위험이 남아 있었다.
- 현재 구현과 계획서의 TTS schema 이름이 `TTSResult`/`TTSErrorResult` vs `TTSMetadata`로 어긋날 수 있었다.

조치:

- Task 5에 `process_optimizer` display/TTS alignment 테스트와 highlight-only state_update TTS 제외 테스트를 추가했다.
- 남은 “missing module/function” 기대 문구를 current-worktree behavior 실패 기준으로 바꿨다.
- ElevenLabs output format은 MVP에서 `mp3_*`만 허용하고 non-MP3는 `invalid_output_format`으로 비활성화하도록 제한했다.
- `edge_tts` no-running-loop / running-loop 양쪽 모두 `settings.timeout_seconds`를 적용하도록 구현 규칙을 보강했다.
- schema 설명을 현재 워크트리의 `TTSRequest`, `TTSMetadata` 기준으로 수정했다.

3차 재리뷰에서 다음 추가 문제가 확인되었다.

- provider override 정책이 prod/staging=ElevenLabs, dev/local/test=edge-tts 요구사항과 충돌할 수 있었다.
- `process_optimizer` 테스트가 이미 `display_message`가 있는 payload만 다뤄, 실제 no-alert analyze/preview 응답에 표시 문구를 생성하는 backend 단계가 빠질 수 있었다.
- timeout clamp가 `from_env()` 기준인지 직접 생성한 `TTSSettings` 전체 기준인지 모호했다.

조치:

- cross-environment provider override는 `invalid_provider_for_environment`로 비활성화하도록 계획과 테스트를 수정했다.
- `process_optimizer` 실제 응답 경로에서 `_prepare_process_optimizer_tts_payload`를 호출해 no-alert analyze/preview 응답에 짧은 `display_message`를 생성하도록 계획과 테스트를 보강했다.
- timeout clamp는 `TTSSettings.from_env()` 파싱에 적용하고, 직접 생성 테스트는 짧은 timeout을 허용한다고 명확히 했다.

4차 재리뷰에서 다음 추가 문제가 확인되었다.

- 환경 변수 `FACTORY_ENV`, `APP_ENV`, `ENVIRONMENT`가 서로 다른 환경 class를 가리킬 때 provider 선택이 모호했다.
- `payload.tts.text`를 입력 힌트로 쓰는 계획과 “payload에 `tts`가 있으면 그대로 둔다”는 pipeline 규칙이 충돌했다.
- no-alert `process_optimizer` 응답 준비 테스트가 helper 직접 호출 중심이라 실제 `pipeline.run()` 경로를 충분히 검증하지 못했다.
- 현재 워크트리의 기존 text-selection 테스트가 `summary`/`optimization_alert` fallback을 고정하고 있는데, 계획서가 그 테스트를 교체하라고 명확히 말하지 않았다.
- 정적 TTS route의 status code 계약이 현재 구현/테스트의 `403`/`400`과 계획서의 `404` 기대값 사이에서 어긋났다.
- Unreal `/tts/...` URL origin이 prod/staging에서도 localhost로 고정될 여지가 있었다.

조치:

- env var precedence와 conflict rejection(`conflicting_environment`) 규칙 및 테스트를 추가했다.
- `tts.text` only payload는 입력 힌트로 보고 합성을 수행하며, 완성된 TTS metadata만 그대로 두도록 pipeline 규칙과 테스트를 수정했다.
- 실제 `pipeline.run()` 기반 process_optimizer preview TTS alignment 테스트를 추가했다.
- 기존 `test_process_optimizer_uses_summary_first`, `test_process_optimizer_builds_short_alert_text`는 새 정책 테스트로 교체하라고 명시했다.
- route 테스트와 구현 규칙을 현재 보안 계약인 invalid agent/traversal `403`, invalid key `400`, missing/not-routed path `404`로 맞췄다.
- Unreal은 WebSocket/client 설정의 agent server origin을 사용하고 localhost는 local default로만 쓰도록 수정했다.

5차 재리뷰에서 다음 추가 문제가 확인되었다.

- 현재 `TTSService` 구현은 provider를 직접 호출해 느린 blocking provider가 `settings.timeout_seconds`를 우회할 수 있었다.
- `EdgeTTSClient`가 timeout 예외를 일반 `RuntimeError`로 감싸면 최종 오류 코드가 `TTS_TIMEOUT` 대신 `TTS_PROVIDER_ERROR`가 될 수 있었다.
- 현재 `process_optimizer` text-selection 구현과 테스트에는 아직 `summary`/raw `optimization_alert` fallback이 남아 있어 계획의 display/TTS 일치 계약과 충돌했다.
- pipeline의 기존 TTS metadata 보존 규칙이 terminal `failed`/`disabled` 상태를 `audio_url` 없이 보존하지 못할 수 있었다.

조치:

- `SlowProvider` 기반 timeout 테스트는 즉시 `TimeoutError`를 던지는 provider로 대체하지 말라고 계획에 명시했다.
- service layer에서도 bounded timeout을 강제해야 한다는 구현 규칙을 추가했다.
- `edge-tts` timeout은 예외 타입/cause를 잃지 않고 최종 `TTS_TIMEOUT`으로 보존하도록 테스트/규칙을 추가했다.
- terminal `failed`/`disabled` TTS metadata 보존 테스트를 pipeline 계획에 추가했다.
- 현재 `summary`/raw `optimization_alert` fallback 테스트는 4차 조치대로 새 정책 테스트로 교체해야 한다는 점을 유지했다.

6차 재리뷰에서는 CRITICAL/HIGH blocker는 없었고, 다음 Medium 보강점이 확인되었다.

- `edge-tts` timeout 회귀 검증이 service final result 중심이라 provider의 no-running-loop/running-loop branch가 실제 elapsed time 안에 반환되는지 직접 고정하지 못했다.
- disabled service metadata가 `conflicting_environment`, `invalid_provider_for_environment` 같은 비활성화 사유를 `error_code`로 보존해야 한다는 계약이 테스트에 충분히 고정되지 않았다.

조치:

- `edge-tts` provider/client timeout 테스트는 no-running-loop/running-loop 양쪽 branch에서 elapsed time assertion을 포함하도록 계획을 보강했다.
- disabled service 결과는 `settings.disabled_reason`이 있으면 `error_code`로 보존하도록 테스트와 구현 규칙을 추가했다.

7차 재리뷰에서 다음 현재 구현 alignment blocker가 확인되었다.

- running-loop `edge-tts` timeout 경로가 `ThreadPoolExecutor` context manager cleanup에서 계속 대기할 수 있어, 6차에 추가한 elapsed-time provider 테스트를 실제로 통과해야 한다.
- 현재 `process_optimizer` text-selection 구현과 테스트는 아직 `summary`/raw `optimization_alert` fallback을 고정하고 있어, 계획서의 display/TTS 일치 계약에 맞게 교체해야 한다.
- storage API가 임의 key/extension을 허용할 수 있어, invalid agent, traversal-like key, non-MP3 extension을 직접 거부하는 테스트가 필요했다.

조치:

- storage 테스트 계획에 invalid agent, traversal-like key, non-MP3 extension 거부 케이스를 추가했다.
- storage 구현 규칙에 lowercase 64-character SHA-256 hex key와 `mp3` extension만 허용하도록 명시했다.
- `edge-tts` timeout과 `process_optimizer` text-selection은 5차/6차 계획에 이미 테스트와 구현 규칙이 들어 있으므로, 7차 리뷰 문서에는 구현 alignment blocker로 유지했다.

8차 재리뷰에서는 CRITICAL/HIGH blocker는 없었고, 다음 Medium 문서 불일치가 확인되었다.

- storage happy-path 테스트 예시가 `key="abc123"`을 사용해, 7차에서 강화한 lowercase 64-character SHA-256 hex key 규칙과 충돌했다.

조치:

- storage happy-path 테스트 예시를 `valid_key = "a" * 64`로 수정하고, 예상 path/audio URL도 해당 key를 사용하도록 맞췄다.
- 서브에이전트가 focused backend TTS suite `35 passed`를 보고했다.

초기 리뷰, 재리뷰, 2차 재리뷰, 3차 재리뷰, 4차 재리뷰, 5차 재리뷰, 6차 재리뷰, 7차 재리뷰, 8차 재리뷰 verdict는 모두 **WARNING**이었다. 최신 반영 후 계획서는 현재 워크트리 기준 continuation 작업으로 실행 가능하도록 다시 정리되었지만, 구현 단계에서는 위 테스트들이 실제로 실패-통과 사이클을 만드는지 먼저 확인해야 한다.

남은 주요 리스크는 Unreal 런타임 MP3 재생 플러그인/디코딩 방식이며, 계획서에서는 이 부분을 no-op + log fallback으로 시작하도록 제한했다.
