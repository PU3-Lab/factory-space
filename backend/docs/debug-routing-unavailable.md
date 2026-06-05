# ROUTING_UNAVAILABLE 에러 디버깅 리뷰

**날짜:** 2026-06-05  
**브랜치:** feat/openai-sdk-env-prod  
**상태:** **Resolved** (모든 수정 조치 완료)  
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
| `backend/src/llm/adapter.py` | OpenAILLMAdapter 예외 로깅 추가 | **Resolved** |
| `backend/main.py` | 지연 임포트로 인한 E402 린트 예외 처리 | **Resolved** |
| `backend/.env.prod` | 모델/API 키 설정 | **Resolved** |
