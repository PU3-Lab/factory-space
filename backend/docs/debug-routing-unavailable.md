# ROUTING_UNAVAILABLE 에러 디버깅 리뷰

**날짜:** 2026-06-05  
**브랜치:** feat/openai-sdk-env-prod  
**상태:** **Partially Resolved** — fallback 로직은 추가됐으나 코드 리뷰에서 잔여 결함 2건 발견 (아래 "코드 리뷰 결과" 참조)  
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
| `backend/src/llm/adapter.py` | OpenAILLMAdapter 예외 로깅 추가 | **Partial** (Google/Local 미적용) |
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
