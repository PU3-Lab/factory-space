# ROUTING_UNAVAILABLE 에러 디버깅 리뷰

**날짜:** 2026-06-05  
**브랜치:** feat/openai-sdk-env-prod  
**상태:** **In Progress** — R1·R2·R3 수정 완료(`91d862b`). 후속 검증에서 신규 에러 `INVALID_LLM_RESPONSE` 발생, 원인 분석 완료 (아래 "신규 에러 — INVALID_LLM_RESPONSE" 참조)  
**증상:** WebSocket `/ws/agent` 연결 시 모든 요청에서 `ROUTING_UNAVAILABLE` 에러 반환

---

## 증상

```json
{
  "agent": "operator_guide",
  "error": {
    "code": "ROUTING_UNAVAILABLE",
    "message": "Agent routing requires a valid orchestrator model decision."
  },
  "type": "agent.error"
}
```

---

## 에러 전파 경로 (역방향 추적)

```
ROUTING_UNAVAILABLE 기본 메시지 반환
  ← build_agent_error [runtime.py:440]
      state.get("error") == None → 기본 에러 사용
  ← route_selected_agent [graph_edges.py:131]
      selectedAgent = "" → "" not in TOP_LEVEL_AGENT_IDS → "error" 반환
  ← route_top_agent [runtime.py:162]
      selectedAgent = (routing_raw or "").strip() = ""
      routing_raw = None (LLM 호출 실패)
  ← routing_llm.invoke(routing_prompt) [runtime.py:158]
      OpenAILLMAdapter.invoke() → except Exception: return None
  ← routing_llm = llm_slots[0].adapter [runtime.py:95]
      OpenAILLMAdapter(model="gpt-5.4-nano", api_key=OPENAI_API_KEY)
```

---

## 근본 원인 및 조치 내역

### 원인 1 — routing_llm에 fallback 없음
- **이슈:** `routing_llm = self.llm or llm_slots[0].adapter`로 선언되어 라우팅에 default 슬롯 하나만 사용하여 default 실패 시 라우팅이 원천 차단됨.
- **조치 (Resolved):** [runtime.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/agents/pipeline/runtime.py)에 `_FallbackRoutingLLM` 래퍼 클래스를 구현하였습니다. 이제 라우팅 시에도 설정된 슬롯(`default` -> `fallback1` -> `fallback2`)을 순차적으로 시도하여 최초로 성공하는 응답을 리턴하도록 수정하여 단일 장애점을 극복했습니다.

### 원인 2 — OpenAILLMAdapter가 모든 예외를 무음 처리
- **이슈:** 예외를 전부 삼키기 때문에 예외 원인 파악 및 관측이 불가능했음.
- **조치 (Resolved):** [adapter.py](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/backend/src/llm/adapter.py)의 `OpenAILLMAdapter.invoke()` 예외 처리 블록에 `logger.warning("OpenAI LLM call failed: %s", exc)` 로깅을 추가하여 운영 시 실패 원인을 명확히 파악할 수 있도록 조치하였습니다.

### 트리거 (직접 원인)
- **이슈:** `.env.prod`에 기입된 모델명이 잘못되었거나 유효한 API Key가 없는 상태에서 라우팅 LLM이 무조건 실패했음.
- **조치 (Resolved):** `routing_llm`에 fallback 로직이 주입되었기 때문에, default(OpenAI) 호출이 실패하더라도 fallback1(Gemini) 또는 fallback2(local/gemma4 Ollama)를 이용해 안정적인 라우팅 수행이 가능하게 되었습니다.

---

## 영향 범위

- `routing_llm`을 공용으로 사용하는 `route_top_agent`, `route_operator_guide_sub_agent`, `route_quest_sub_agent` 세 라우팅 구간 모두 fallback 이점(안정적인 에이전트 라우팅)을 보장받습니다.

---

## 관련 파일

| 파일 | 역할 | 상태 |
|------|------|------|
| `backend/src/agents/pipeline/runtime.py` | routing_llm에 `_FallbackRoutingLLM` 적용 | **Resolved** |
| `backend/src/llm/adapter.py` | OpenAI/Google/Local 예외 로깅 + routing 따옴표 제거(`91d862b`) | **Resolved** |
| `backend/main.py` | 지연 임포트로 인한 E402 린트 예외 처리 | **Resolved** |
| `backend/.env.prod` | 모델/API 키 설정 | **Resolved** |

---

## 코드 리뷰 결과 (2026-06-05, `/code-review` high)

fallback 추가 PR을 리뷰한 결과, **fallback 자체는 동작하지만 prod 시나리오에서 여전히 실패할 수 있는** 잔여 결함 2건과 유지보수 이슈 1건이 확인되었다.

### 🔴 R1 — Gemini fallback도 routing 검증에 실패 (CONFIRMED, 미해결)

**파일:** `backend/src/llm/adapter.py` (`_google_generate_config`)

- `GoogleGenAiLLMAdapter`는 `response_mime_type="application/json"`으로 호출된다.
- routing 프롬프트는 맨 문자열(`operator_guide`)을 요구한다 (`orchestrator.py` OUTPUT_CONTRACT: "JSON, 따옴표, 코드블록 출력 금지").
- JSON 모드 강제 때문에 Gemini는 `"operator_guide"`(따옴표 포함)를 반환한다.
- `(routing_raw or "").strip()`은 따옴표를 제거하지 못하므로 `'"operator_guide"'`가 되어 `TOP_LEVEL_AGENT_IDS` 검증에 실패한다.

**영향:** default(OpenAI)가 실패하면 fallback1(Gemini)도 동일하게 `ROUTING_UNAVAILABLE`을 낸다. fallback2(local/Ollama)만 routing을 구제할 수 있다. **즉 이 PR이 의도한 "Gemini fallback"은 routing 경로에서 실질적으로 동작하지 않는다.**

**수정 방향:** routing 경로에서 JSON mime type을 끄거나, routing 결과에서 JSON 따옴표를 벗기는 후처리를 추가한다.

### 🟡 R2 — 예외 로깅이 OpenAI에만 비대칭 (PLAUSIBLE)

**파일:** `backend/src/llm/adapter.py`

이번 PR은 `OpenAILLMAdapter.invoke()`에만 `logger.warning(...)`를 추가했다. `GoogleGenAiLLMAdapter`와 `LocalLLMAdapter`는 여전히 예외를 조용히 삼킨다. Gemini/Local fallback이 실패해도 운영자는 원인을 볼 수 없으므로, 위 "원인 2 (Resolved)" 표기는 OpenAI 슬롯 한정으로만 유효하다.

**수정 방향:** Google/Local 어댑터의 `except` 블록에도 동일한 경고 로깅을 추가한다.

### 🟡 R3 — prod 스크립트가 run_server.py를 통째 복제 (CONFIRMED)

**파일:** `backend/scripts/run_prod_server.py`

`check_ollama_running`, `ensure_ollama_running`, `load_env_file` 세 함수가 `run_server.py`와 거의 동일하다(로그 메시지만 차이). Ollama 기동/env 로딩 로직 변경 시 두 파일을 모두 수정해야 한다.

**수정 방향:** 공통 모듈로 추출하여 dev/prod 스크립트가 공유하도록 한다.

### 기각된 후보
- tool_followup의 slot None 조회 → 이미 `if slot is None: return {}` 가드 존재 (REFUTED)
- 타입 힌트 / 빈 슬롯 가드 등 → 관측 가능한 영향 없음

---

## 수정 적용 및 재리뷰 (2026-06-05, commit `91d862b`)

R1·R2·R3 모두 수정 적용됨. 라우팅 관련 테스트 56개 통과(`test_message_router.py`, `test_pipeline_edges.py`, `test_websocket_endpoint.py`).

### ✅ R1 — 해결됨
`runtime.py`에 `_clean_routing_decision()` 헬퍼를 추가하고, 3개 라우팅 결정 지점(`route_top_agent`, `route_operator_guide_sub_agent`, `route_quest_sub_agent`)의 `(routing_raw or "").strip()`을 모두 이 헬퍼로 교체했다. 헬퍼는 앞뒤 공백과 바깥쪽 따옴표(`"`/`'`)를 제거한다.

- 검증: `'"operator_guide"'` → `operator_guide`, `"'quest_generator'"` → `quest_generator`, 따옴표 없는 정상 출력 `operator_guide` → 그대로 유지(회귀 없음).
- 수정 위치 적절성: `_google_generate_config`의 JSON mime type은 실제 에이전트 응답(JSON 필요)과 공유되므로 producer 쪽에서 끌 수 없다. routing consumer에서 따옴표를 벗기는 것이 올바른 레이어다.

### ✅ R2 — 해결됨
`GoogleGenAiLLMAdapter.invoke()`와 `_invoke_openai_compatible()`(Local)의 `except` 블록에 `logger.warning(...)`를 추가했다. 이제 OpenAI·Google·Local 세 provider의 실패 로깅이 대칭이다.

### ✅ R3 — 해결됨
`run_prod_server.py`의 중복 함수를 제거하고 `from scripts.run_server import ensure_ollama_running, load_env_file`로 재사용하도록 변경했다. `run_server.main()`이 `if __name__ == "__main__"` 가드 안에 있어 import 시 부작용 없음.

### ⚠️ 남은 사소한 잔여 사항 (블로커 아님)

1. **R1 잔여 리스크:** Gemini가 JSON **객체**(`{"agent": "operator_guide"}`)를 반환하면 바깥 따옴표 제거로는 잡지 못한다(객체 문자열 그대로 반환되어 검증 실패). 다만 프롬프트가 "맨 ID만 출력"을 강하게 제약하므로 JSON 모드에서도 JSON **문자열**(`"operator_guide"`)이 나올 가능성이 높아 실무상 충분하다. 추후 객체 응답이 관측되면 `json.loads` 후 ID 추출로 보강한다.

2. **R3 관측성 미세 회귀:** prod의 기존 `load_env_file`은 `.env.prod`가 없으면 경고를 출력했지만, 재사용하는 `run_server` 버전은 조용히 넘어간다(`if not env_file.exists(): return`). prod에서 env 파일 누락 시 경고 없이 기동된다. 필요 시 `prepare_environment`에서 파일 존재 여부를 명시적으로 확인해 경고를 출력한다.

---

## 신규 에러 — INVALID_LLM_RESPONSE (2026-06-05)

R1·R2·R3 수정 후 재기동 시 라우팅은 통과했으나 다음 에러 발생:

```json
{
  "code": "INVALID_LLM_RESPONSE",
  "message": "LLM response must be a JSON object."
}
```

### 상태: **Resolved — 최종 단순화 적용 완료** (2026-06-05)

- 기능: 프롬프트 개선 + 펜스 제거 파서로 에러 해소.
- 설계: 아래 "최종 설계"(API 레벨 강제 제거)까지 코드에 반영 완료. `json_mode`·`invoke_json`·`_invoke_internal`·Ollama 휴리스틱·`response_format`/`format:"json"` 전부 제거됨. Google는 `text/plain`으로 변경.
- 검증: 전체 테스트 143개 통과.

### 에러 전파 경로

```
INVALID_LLM_RESPONSE
  ← parse_llm_response [runtime.py:344-360]
      json.loads(raw) → JSONDecodeError (raw가 JSON이 아닌 자연어)
  ← llmRaw = LLM이 자연어 산문으로 응답
  ← leaf 에이전트 build_prompt가 JSON 출력을 지시하지 않음
```

### 근본 원인 — 프롬프트 ↔ 파서 계약 불일치

**파서는 JSON 객체를 강제**하지만 **leaf 에이전트 프롬프트는 JSON을 요구하지 않는다.**

leaf 에이전트 프롬프트 예시:
- `machine_help.py`: `"다음 설비 도움말 질문에 답변하세요: {payload}"`
- `troubleshooter.py`: `"다음 공장 문제를 진단하고 해결 순서를 제안하세요: {payload}"`

JSON 스키마도, 형식 지시도 없음 → LLM이 자연어로 답변 → `json.loads` 실패.

또한 OpenAI 어댑터에는 `response_format={"type":"json_object"}` 설정이 없고, Local(Ollama)에도 `format:"json"`이 없다.

### 해결 (초기 접근 → 최종 설계로 단순화)

INVALID_LLM_RESPONSE는 3계층으로 해결한다:

1. **프롬프트 개선 (근본)**: 각 리프 에이전트(`machine_help`, `troubleshooter`, `recipe_explainer`, `process_optimizer`, `new_material_generator`, `production_quest`, `economy_quest`)의 `build_prompt`에 JSON 출력 스키마와 "JSON 객체 하나만 출력, 마크다운 펜스·설명 금지" 지침을 명시했다. → **출력 형식을 프롬프트가 통제한다.**
2. **파서 방어 (안전망)**: `parse_llm_response`에서 응답이 ```` ```json ```` 펜스로 감싸진 경우 벗겨낸 뒤 `json.loads`를 수행한다.

라우팅 프롬프트는 기존대로 bare ID(`operator_guide`)를 요구하고, R1 수정의 `_clean_routing_decision`이 따옴표를 정리한다.

---

### ❌ 폐기된 중간 접근 — API 레벨 JSON 강제 (`json_mode` / `invoke_json`)

초기에는 어댑터 API 호출 자체에서 JSON을 강제하려 했다:
- v1: `invoke(prompt, *, json_mode=False)` — `Protocol → 어댑터 4개 → _FallbackRoutingLLM → invoke_llm_call_slot` 4계층을 관통하는 call-time 파라미터.
- v2: `invoke()` / `invoke_json()` 메서드 분리 + `_invoke_internal` 위임. OpenAI는 `response_format={"type":"json_object"}`, Google는 `response_mime_type`, Local은 `response_format` + Ollama 감지 휴리스틱으로 `format:"json"` 전송.

**폐기 사유:** 프롬프트(1)가 이미 출력 형식을 명시하고 파서(2)가 펜스를 벗기므로, API 레벨 강제는 **중복**이다. 또한 다음 복잡도를 끌어들인다:

- `json_mode`는 호출부에서 항상 고정값(라우팅=plain, 응답=JSON)이라 런타임 분기가 아님에도 4계층 파라미터로 노출됨.
- Ollama 감지 휴리스틱(`base_url`에 `11434` 포함 / 모델명에 `llama`·`gemma`·`mistral`·`phi` 포함)은 깨지기 쉬움 — 비표준 포트는 miss, 비-Ollama 서버에서 동일 모델명 사용 시 false positive로 `format:"json"`이 거부될 수 있고, `qwen`·`deepseek` 등은 miss.

### ✅ 최종 설계 — 출력 형식은 프롬프트가 통제, 어댑터는 `invoke()` 하나 (적용 완료)

어댑터에서 API 레벨 JSON 강제 장치를 모두 제거했다:

- `invoke_json()` 메서드 제거 (Protocol + 어댑터 4개) ✅
- `_invoke_internal` 위임 래퍼 제거 ✅
- `json_mode` 파라미터 제거 (`_invoke_openai_compatible`, `_google_generate_config`) ✅
- **Ollama 감지 휴리스틱 통째로 제거** ✅
- `response_format` / `format:"json"` 제거 (Protocol의 dead 파라미터 및 테스트 fake 미러 포함) ✅
- Google `response_mime_type`을 `text/plain`으로 변경 ✅

실제 최종 diff는 두 곳뿐이다: `adapter.py`의 Google `response_mime_type` 한 줄(`application/json` → `text/plain`), `runtime.py`의 `parse_llm_response` 펜스 제거. 나머지는 leaf 프롬프트 7개의 JSON 스키마 추가.

남는 계약:
- 어댑터마다 `invoke(prompt) -> str | None` 하나
- 라우팅 프롬프트 → bare string 요구 + `_clean_routing_decision`
- 응답 프롬프트 → JSON 스키마 요구 (적용 완료)
- 파서 → 펜스 제거 안전망

**트레이드오프:** API 강제가 없으므로 모델이 프롬프트를 무시하면 깨질 수 있다. 그러나 원래 결함(프롬프트가 JSON을 아예 요구하지 않음)은 프롬프트 개선으로 제거되었고, 펜스 제거 파서가 가장 흔한 일탈(```` ```json ```` 래핑)을 흡수한다. 실무상 충분하며, 향후 특정 모델이 반복적으로 일탈하면 그때 해당 provider 한정으로 API 강제를 재도입한다.

---

## 재발 — 로컬 dev 환경 `.env` 누락 (2026-06-14)

**상태:** **Resolved**
**증상:** `uv run python scripts/run_server.py`로 dev 서버 기동 후 `/agent-test` 콘솔의 "신물질 합성" 프리셋(`material_generation`) 요청이 모두 `ROUTING_UNAVAILABLE` 반환.

### 근본 원인 — 코드 결함 아님, **설정 누락**

이전 절들의 코드 수정(fallback 래퍼, 따옴표 정리)은 모두 적용된 상태였다. 이번 재발은 코드가 아니라 **환경 설정** 문제였다.

1. `scripts/run_server.py`의 `prepare_environment`는 `backend/.env`를 로드한다 ([run_server.py:101](file:///Users/kimkyungpyo/Workspaces/projests/factory-space/backend/scripts/run_server.py)).
2. 그러나 작업 트리에는 `.env`가 없고 `.env.example`만 존재했다 (`.env`는 `.gitignore` 대상).
3. `.env`가 없으면 `ENVIRONMENT`도, `FACTORY_LLM_*`도 셸에 설정되지 않는다.
4. `LLMSettings.from_env`의 `_provider_from_env`는 `default` 슬롯에서 provider 미지정 + `ENVIRONMENT != "development"`이면 provider를 `none`으로 확정한다 ([settings.py:94](file:///Users/kimkyungpyo/Workspaces/projests/factory-space/backend/src/llm/settings.py)).
5. 세 슬롯 모두 `none` → `NoopLLMAdapter.invoke()`가 항상 `None` 반환 → `routing_llm`(fallback 래퍼)도 `None` → `selectedAgent == ""` → `TOP_LEVEL_AGENT_IDS` 검증 실패 → `ROUTING_UNAVAILABLE`.

재현 확인:
- 빈 env(`LLMSettings.from_env(env={})`) → 세 슬롯 모두 `provider=none`.
- 로컬 Ollama `gemma4:e4b` 슬롯 → `invoke("...operator_guide")` → `'operator_guide'` 정상 반환.

> 추가 함정: `.env.example`의 기본 모델은 `llama3.1:8b`이지만 로컬 Ollama에는 `gemma4:e4b`/`gemma4:26b`만 pull되어 있어, 예제를 그대로 복사하면 모델 404로 또 `None`을 받아 동일 증상이 재현된다.

### 조치

1. `backend/.env` 생성: `ENVIRONMENT=development`, default 슬롯을 로컬 Ollama + **실제 pull된 모델** `gemma4:e4b`로 지정, fallback1/2는 `none`.

### 후속 차단 이슈 (별건, 함께 해소)

라우팅 복구 후 `material_generation` 경로에서 `SYNTHESIS_ERROR: no such table: generated_experiments` 발생.

- 원인: `factory_space.db`가 0바이트(빈 파일)였고 Alembic 마이그레이션(`0001`, `0002`)이 미적용 상태.
- 함정: `migrations/env.py`는 `DATABASE_URL`을 **필수**로 요구하지만([env.py:25](file:///Users/kimkyungpyo/Workspaces/projests/factory-space/backend/migrations/env.py)), 앱 엔진(`db/engine.py`)은 미설정 시 `sqlite:///./factory_space.db`로 **기본값**을 쓴다. 두 경로를 일치시켜야 한다.
- 조치: `DATABASE_URL="sqlite:///./factory_space.db" uv run alembic upgrade head` 실행, 그리고 앱·마이그레이션 일관성을 위해 `.env`에 `DATABASE_URL=sqlite:///./factory_space.db` 추가.

### 최종 검증

"신물질 합성" 프리셋을 파이프라인에 통과시킨 결과 `type: agent.response` 정상 반환(에러 envelope 아님). 단, `result_type: invalid_input`(=`unknown_item`)인데, 이는 갓 생성된 DB의 아이템 카탈로그가 비어 있어(`get_known_items` → 0건) 결정적 prevalidator가 `iron_ingot`/`copper_ingot`을 미등록 아이템으로 판정한 **정상 동작**이다. 시드 데이터 부재는 별도 데이터 과제이며 라우팅/DB 버그와 무관하다.

### 관련 파일 (이번 재발)

| 파일 | 역할 | 상태 |
|------|------|------|
| `backend/.env` | dev LLM 슬롯(local/gemma4:e4b) + `DATABASE_URL` (gitignore 대상, 신규 생성) | **Resolved** |
| (마이그레이션) `0001`, `0002` | `alembic upgrade head`로 적용 | **Resolved** |

### 운영 권고

`.env` 부재 + 빈 DB는 신규 클론/머신마다 재현된다. dev 온보딩을 매끄럽게 하려면 (1) `.env.example`의 기본 모델을 실제 배포 환경에서 pull하는 모델과 맞추거나 주석으로 명시, (2) `migrations/env.py`가 `DATABASE_URL` 미설정 시 `db.engine.get_database_url()`과 동일한 sqlite 기본값을 쓰도록 통일, (3) 아이템 카탈로그 시드 스크립트 제공을 고려한다. (코드 변경이 필요하므로 이번 작업 범위 밖.)
