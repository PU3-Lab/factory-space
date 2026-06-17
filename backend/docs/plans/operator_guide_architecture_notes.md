# Operator Guide 아키텍처 정리: 미들웨어, 툴, 폴백 시스템

이 문서는 현재 `operator_guide` 에이전트 도메인에서 **미들웨어(Middleware)**, **도구(Tools)**, 그리고 **폴백(Fallback)** 시스템이 어떻게 동작하고 있는지 설명합니다.

---

## 1. 미들웨어 (Middleware)

미들웨어는 LangGraph 에이전트 파이프라인의 생명주기(Lifecycle) 및 다양한 로깅/메타데이터 수집 단계를 제어하는 역할을 합니다.

### 핵심 역할 및 정의
- **정의 파일**: [middleware.py](file:///c:/factory-space/backend/src/agents/pipeline/middleware.py)
- **주요 함수**:
  - [append_middleware_log](file:///c:/factory-space/backend/src/agents/pipeline/middleware.py#L13-L36): 에이전트 파이프라인의 각 실행 상태(`AgentGraphState`)를 받아 단계별 로그 정보를 생성하고 `middlewareLogs` 리스트에 누적합니다.
  - [build_current_model_metadata](file:///c:/factory-space/backend/src/agents/pipeline/middleware.py#L39-L54): 실제 응답이 정상적으로 반환될 때 사용된 LLM 모델, 프로바이더 등의 메타데이터를 정제하여 반환합니다.

### LangGraph 노드 연동
[runtime.py](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py)에서 다음과 같이 미들웨어 관련 노드를 정의하고 실행하고 있습니다:
1. **`agent.middleware.before` (에이전트 시작)**
   - 캐시 미스(Cache Miss)가 발생한 후 실제 프롬프트 빌드가 들어가기 직전에 실행됩니다.
   - 요청된 에이전트와 서브 에이전트 등의 정보를 미들웨어 로그로 기록합니다.
2. **`agent.middleware.fallback` (에이전트 폴백)**
   - 모든 LLM 슬롯 호출이 실패(또는 타임아웃)했을 때 실행되며, `deterministic_fallback` 이벤트를 기록합니다.
3. **`agent.middleware.after` (에이전트 종료)**
   - 모든 검증과 캐시 작성이 완료된 후 최종 응답 준비 단계로 가기 직전에 실행됩니다.
   - 세션 메모리에 최종 답변 내용을 저장하는 [OperatorGuideSessionMemory.remember](file:///c:/factory-space/backend/src/agents/operator_guide/session_memory.py#L161)를 연동합니다.

최종 축적된 미들웨어 로그(`middlewareLogs`)는 최종 Unreal/Front단으로 내려가는 응답 패키지의 `metadata.middlewareLogs` 필드에 포함되어 클라이언트단에서 디버깅 용도로 활용할 수 있게 설계되어 있습니다.

---

## 2. 도구 (Tools)

도구는 LLM이 질문을 해석하는 과정에서 데이터베이스 조회나 추가 정보 검색이 필요할 때 호출하는 실행체입니다.

### 동작 메커니즘
- **정의 파일**: [tool_node.py](file:///c:/factory-space/backend/src/agents/pipeline/tool_node.py)
- **도구 감지**: [is_tool_request](file:///c:/factory-space/backend/src/agents/pipeline/tool_node.py#L44-L50) 함수가 LLM 원문 응답에 `tool_call` 필드가 존재하는지 체크합니다.
- **도구 실행 흐름**:
  - LLM이 도구 호출을 요청하면 파이프라인은 `call_llm.default` -> `prepare_tool_call` -> `agent.tool_node` -> `build_tool_followup_prompt` -> `call_llm.tool_followup` 순으로 LangGraph를 타고 들어가 실제 툴을 동작시킵니다.
  - 실행된 툴의 결과는 `[TOOL_RESULT]` 형태로 기존 프롬프트 뒤에 덧붙여져 LLM에게 다시 전달되고, 최종 답변을 생성하도록 유도합니다.

### `operator_guide`에서의 사용 현황
- **현재 상황**: **도구를 사용하지 않습니다.**
- `operator_guide` 하위의 3개 리프 에이전트 클래스는 모두 도구 필드가 빈 튜플(`tools = ()`)로 정의되어 있습니다.
  - [TroubleshooterAgent](file:///c:/factory-space/backend/src/agents/operator_guide/troubleshooter.py#L19)
  - [RecipeExplainerAgent](file:///c:/factory-space/backend/src/agents/operator_guide/recipe_explainer.py#L19)
  - [MachineHelpAgent](file:///c:/factory-space/backend/src/agents/operator_guide/machine_help.py#L19)
- 따라서 현재의 운영자 가이드 시나리오에서는 자연어 질문을 intent 분류 및 RAG 기반 매뉴얼 chunk 검색 결과와 결합하여 LLM 단독 답변으로 도출하는 방식만을 사용하며, 추가적인 백엔드 액션 툴 호출 단계를 거치지 않습니다.

---

## 3. 폴백 (Fallback)

폴백은 LLM API 호출 장애(Timeout, API 오프라인, Rate limit 초과 등)가 발생했을 때 파이프라인의 에러 붕괴를 막고 유효한 응답을 복구해 내기 위한 다중 안전장치입니다.

### 다단계 LLM 대체 (LLM Fallback Cascading)
[runtime.py](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py)는 총 3단계의 LLM 어댑터를 차례로 호출하도록 설계되어 있습니다.
```text
[LLM Call 1: call_llm.default]
  │ (실패 시)
  ▼
[LLM Call 2: call_llm.fallback1]
  │ (실패 시)
  ▼
[LLM Call 3: call_llm.fallback2]
  │ (실패 시)
  ▼
[최종 확정적 폴백: agent.middleware.fallback]
```

### 결정론적 폴백 (Deterministic Fallback)
모든 LLM 모델 호출 단계가 실패하면 LangGraph의 흐름은 `agent.middleware.fallback` 노드로 전달됩니다.
1. **동작 진입**: [utils.py](file:///c:/factory-space/backend/src/agents/pipeline/utils.py)의 [run_fallback](file:///c:/factory-space/backend/src/agents/pipeline/utils.py#L18-L23) 함수를 통해 현재 선택된 리프 에이전트의 `fallback()` 메소드를 호출합니다.
2. **에이전트별 동작**: 각 리프 에이전트는 [service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py)의 [build_manual_qa_agent_result](file:///c:/factory-space/backend/src/agents/operator_guide/service.py#L153-L177) 함수를 최종 호출합니다.
3. **LLM을 사용하지 않는 대체 답변 조립**:
   - 질문 의도 분석 및 CSV 레포지토리 조회를 담당하는 `ManualQAService().answer()`를 수행하여 사용자가 질문한 장비/자원/레시피 관련 매뉴얼의 매칭 기록을 추출합니다.
   - 추출된 증거 데이터의 유무에 따라 결정론적인 텍스트 답변을 생성합니다.
     - **매칭 증거가 있을 때**: `"LLM answer unavailable. Please check the matched manual evidence."`
     - **매칭 증거가 없을 때**: `"LLM answer unavailable. No matching manual data was found."`
   - 비록 대화형 텍스트 답변은 생성하지 못하지만, 에이전트가 CSV에서 찾아낸 정보 자료 소스(`sources`)와 추천 행동 목록(`recommended_actions`)은 정상적으로 조립하여 응답 메타데이터에 담아 반환하므로, 클라이언트 UI(Unreal 등)에서는 최소한의 대안 데이터 가이드를 정상 노출할 수 있습니다.
