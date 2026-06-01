# Middleware와 Tools 구조 설계

## 1. 목적

이 문서는 Agent pipeline에 붙일 middleware 구조와 LLM tool calling 구조를 정의한다.

이번 문서는 설계만 다룬다. 구현, 테스트 코드, pipeline graph 수정, provider adapter 수정은 별도 작업으로 분리한다.

## 2. 핵심 결정

- `middleware`는 FastAPI/ASGI middleware가 아니라 `AgentPipeline` 실행 middleware를 의미한다.
- `tools`는 Unreal action이 아니라 LLM generation 단계에서 사용할 backend read-only tool을 의미한다.
- 모든 Agent는 `agents.base.Agent` 계약상 `tools` tuple을 가질 수 있다. tool이 없으면 빈 tuple을 사용한다.
- v1 tool calling은 provider-native function calling이 아니라 provider-neutral JSON 계약으로 시작한다.
- OpenAI/Gemini native tool calling은 후속 확장 지점으로만 남긴다.
- routing 단계에는 tool calling을 적용하지 않는다.
- routing prompt 보강용 deterministic tool interface는 허용한다. 현재 구현은 `agents.agent_catalog.AgentCatalogTool`이며, 이는 LLM이 호출하는 tool이 아니라 오케스트레이터가 직접 호출하는 read-only routing support tool이다.
- generation 단계에서만 tool request를 감지하고 실행한다.
- v1 tool loop는 최대 1회만 허용한다.
- v1 tool catalog는 read-only context tool만 포함한다.
- agent middleware는 LangChain `create_agent`로 이관하지 않고 기존 LangGraph `StateGraph`의 공통 node/edge로 붙인다.
- fallback은 agent 내부에 숨기지 않고 실패 edge에서 fallback node로 분기하는 B 패턴을 유지한다.

## 3. 범위

포함:

- Agent pipeline middleware 개념과 위치
- provider-neutral tool request / tool result 계약
- read-only tool registry와 executor 구조
- LangGraph pipeline 연결 방향
- 실패 처리 정책
- 향후 구현 테스트 기준

제외:

- FastAPI middleware, CORS, auth, rate limit
- Unreal action catalog 실행
- routing 단계의 LLM tool calling
- OpenAI/Gemini native function calling schema 매핑 구현
- tool 결과 streaming
- write/update/delete tool
- multi-step tool loop

## 4. 목표 구조

```txt
backend/src/
 ├─ agents/
 │   └─ pipeline/
 │       ├─ runtime.py
 │       ├─ graph_edges.py
 │       ├─ state.py
 │       └─ tool_middleware.py      # 신규
 └─ tools/                          # 신규
     ├─ __init__.py
     ├─ schemas.py
     ├─ registry.py
     ├─ executor.py
     └─ context_tools.py
```

역할:

- `tools/schemas.py`: tool definition, call request, call result 타입을 정의한다.
- `tools/registry.py`: tool name으로 callable tool을 등록하고 조회한다.
- `tools/executor.py`: tool 실행과 error result 정규화를 담당한다.
- `tools/context_tools.py`: read-only factory/session/context 조회 tool을 둔다.
- `agents/pipeline/tool_middleware.py`: LLM raw output이 tool request인지 판별하고, tool 실행 결과를 follow-up prompt로 변환한다.

## 5. Middleware 위치

LangGraph `StateGraph`를 직접 쓰는 현재 구조에서는 별도 middleware stack API가 아니라 명시적인 node/edge가 정석이다. 따라서 middleware는 각 Agent 파일에 중복으로 붙이지 않고, 모든 selected leaf agent가 통과하는 공통 execution lane에 한 번만 붙인다.

일반 패턴:

```txt
resolve selected agent
 -> cache_lookup
    -> hit: build_cached_response -> build_agent_response
    -> miss:
        -> agent.middleware.before
        -> agent execution
        -> agent.middleware.after
        -> build_agent_response
```

fallback은 B 패턴으로 둔다.

```txt
agent execution
 -> success: agent.middleware.after
 -> failure: agent.middleware.fallback -> agent.middleware.after
```

이 설계의 v1 agent middleware node:

```txt
agent.middleware.before
agent.middleware.fallback
agent.middleware.after
```

역할:

- `agent.middleware.before`: selected leaf agent 실행 직전에 middleware log를 남긴다.
- `agent.middleware.fallback`: 모든 LLM slot 실패 후 기존 deterministic fallback을 실행하고 middleware log를 남긴다.
- `agent.middleware.after`: response schema 검증과 cache write 이후 middleware log를 남긴다.

제약:

- cache hit은 agent 실행이 아니므로 agent middleware를 타지 않는다.
- top-level routing과 sub-agent routing은 agent execution lane이 아니므로 agent middleware를 타지 않는다.
- 각 Agent 구현 파일에는 middleware를 붙이지 않는다.
- `agent.middleware.fallback`은 기존 `run_fallback()`과 Agent별 `fallback()` 계약을 재사용한다.
- middleware log에는 node/event와 selected agent id만 남기고 prompt 전문, LLM raw output, tool result data는 넣지 않는다.
- 현재 사용 모델 표시는 generation metadata에 `currentModel`로 남긴다.

현재 generation 흐름은 다음과 같다.

```txt
build_prompt
 -> call_llm.default
    -> raw 있음: parse_llm_response
    -> raw 없음: call_llm.fallback1
 -> call_llm.fallback1
    -> raw 있음: parse_llm_response
    -> raw 없음: call_llm.fallback2
 -> call_llm.fallback2
    -> raw 있음: parse_llm_response
    -> raw 없음: build_fallback
 -> validate_response_schema
 -> cache_write
 -> build_agent_response
```

tool middleware가 들어간 후 목표 흐름:

```txt
agent.middleware.before
 -> build_prompt
 -> call_llm.default / fallback1 / fallback2 중 raw를 반환한 generation slot
 -> inspect_tool_request
    -> no tool: parse_llm_response
    -> tool requested: execute_tool
 -> build_tool_followup_prompt
 -> call_llm.tool_followup
 -> parse_llm_response
 -> validate_response_schema
 -> cache_write
 -> agent.middleware.after
 -> build_agent_response
```

제약:

- `route_top_agent`, `operator_guide.route_sub_agent`, `quest_generator.route_sub_agent`에는 tool middleware를 붙이지 않는다.
- routing LLM output이 tool request 형태면 허용 agent id가 아니므로 기존처럼 `ROUTING_UNAVAILABLE`로 종료한다.
- tool follow-up 호출은 tool request를 반환한 generation slot과 같은 adapter를 1회 더 호출한다.
- 모든 generation slot 실패는 `agent.middleware.fallback`으로 분기한다.

## 6. Tool 계약

### 6.1 Tool request

LLM이 tool을 요청할 때는 다음 JSON object만 반환한다.

```json
{
  "tool_call": {
    "name": "factory_context.get_machines",
    "args": {}
  }
}
```

규칙:

- 최상위 key는 `tool_call` 하나다.
- `tool_call.name`은 비어 있지 않은 string이다.
- `tool_call.args`는 object다. 생략하면 `{}`로 본다.
- JSON이 아니거나 object가 아니면 기존 generation parsing 정책을 따른다.
- normal agent response JSON과 tool request JSON은 동시에 섞지 않는다.
- 최상위 object에 `tool_call` key가 있으면 tool parser가 반드시 소유한다.
- `tool_call` 외 top-level key가 함께 있으면 mixed output으로 보고 invalid tool request로 처리한다.
- `tool_call` shape이 잘못되면 정상 Agent response로 통과시키지 않고 실패 tool result로 정규화한다.
- invalid tool request의 result `name`은 `_invalid_tool_call`로 고정한다.

### 6.2 Tool result

tool executor는 성공과 실패를 모두 같은 shape으로 반환한다.

```json
{
  "name": "factory_context.get_machines",
  "ok": true,
  "data": {
    "machines": []
  },
  "error": null
}
```

실패 예시:

```json
{
  "name": "factory_context.get_machines",
  "ok": false,
  "data": {},
  "error": {
    "code": "UNKNOWN_TOOL",
    "message": "Tool is not registered."
  }
}
```

규칙:

- unknown tool, invalid args, execution error는 exception으로 pipeline 밖으로 새지 않는다.
- 실패도 tool result로 정규화해서 follow-up prompt에 전달한다.
- tool result 자체가 public WebSocket message로 나가지 않는다.

## 7. v1 Read-only Tool Catalog

초기 catalog는 context 조회만 포함한다.

```txt
factory_context.get_machines
factory_context.get_inventory
factory_context.get_recipes
factory_context.get_selected_object
```

각 tool은 현재 `AgentContext.metadata`와 request payload에서 읽을 수 있는 값만 반환한다.

예시:

- `get_machines`: `payload.machines` 또는 `payload.factory_state.machines`를 반환한다.
- `get_inventory`: `payload.inventory` 또는 `payload.factory_state.inventory`를 반환한다.
- `get_recipes`: `payload.recipes` 또는 `payload.factory_state.recipes`를 반환한다.
- `get_selected_object`: `context.metadata.selected_object` 또는 `payload.selected_object`를 반환한다.

제약:

- tool은 외부 API, database, file system, Unreal runtime을 호출하지 않는다.
- tool은 payload/context를 mutate하지 않는다.
- tool은 write action을 만들지 않는다.

## 8. Prompt 정책

### 8.1 Routing prompt 보강

Top-level orchestrator에는 generation tool catalog를 붙이지 않는다. 대신 공통 `AgentTool`의 routing specialization인 `RoutingSupportTool`을 `OrchestratorAgent.tools`에 연결한다. 오케스트레이터는 이 deterministic read-only tool을 직접 호출하고, 그 결과를 prompt section으로 주입한다.

현재 routing prompt에는 다음 섹션을 둔다.

```txt
[ALLOWED_AGENT_IDS]
- process_optimizer
- operator_guide
- quest_generator
- new_material_generator

[AGENT_CAPABILITIES]
- process_optimizer: ...
- operator_guide: ...
- quest_generator: ...
- new_material_generator: ...
```

규칙:

- routing support tool은 `invoke(payload, context) -> RoutingToolResult` 계약을 따른다.
- `RoutingToolResult`는 prompt section 이름과 content만 제공한다.
- `AgentCatalogTool`은 top-level Agent id, summary, when-to-use 설명만 제공한다.
- `AgentCatalogTool`은 database, file system, Unreal runtime을 읽지 않는다.
- LLM은 `AgentCatalogTool`을 tool로 호출하지 않는다. 오케스트레이터가 prompt 생성 전에 deterministic하게 호출한다.
- routing output 계약은 계속 허용된 agent id 문자열 하나다.
- routing LLM이 `tool_call` JSON을 반환하면 기존처럼 `ROUTING_UNAVAILABLE`이다.

### 8.2 Generation prompt tool 정책

Agent generation prompt에는 사용 가능한 tool catalog를 짧게 추가한다.
주입 위치는 pipeline 공통 prompt augmentation으로 고정한다. 각 agent의 `build_prompt()`는 기존 책임을 유지하고, generation LLM 호출 직전에 `tool_middleware.py`가 catalog와 output contract를 덧붙인다.

요구:

- 모델은 tool이 필요 없으면 기존 Agent response JSON을 바로 반환한다.
- 모델은 tool이 필요하면 `tool_call` JSON만 반환한다.
- tool result를 받은 follow-up prompt에서는 최종 Agent response JSON만 반환한다.
- follow-up에서 다시 `tool_call`을 반환하면 v1에서는 tool loop limit 초과로 보고 deterministic fallback으로 복구한다.

prompt 예시:

```txt
[AVAILABLE_TOOLS]
- factory_context.get_machines(args: object): 현재 요청의 machines snapshot을 조회합니다.
- factory_context.get_inventory(args: object): 현재 요청의 inventory snapshot을 조회합니다.

[TOOL_OUTPUT_CONTRACT]
도구가 필요하면 다음 JSON object만 반환하세요:
{"tool_call":{"name":"factory_context.get_machines","args":{}}}
```

## 9. Pipeline state와 metadata

`AgentGraphState`에는 다음 값이 추가될 수 있다.

```txt
middlewareLogs
currentModel
toolCallRequest
toolCallResult
toolCallCount
toolFollowupPrompt
toolCalls
```

최종 `agent.response.payload.metadata`에는 debug용으로 다음 값을 추가한다.

```json
{
  "currentModel": {
    "slot": "fallback1",
    "provider": "openai",
    "model": "gpt-5.5"
  },
  "middlewareLogs": [
    {
      "node": "agent.middleware.before",
      "event": "agent_started"
    }
  ],
  "toolCalls": [
    {
      "name": "factory_context.get_machines",
      "ok": true
    }
  ]
}
```

규칙:

- `currentModel`은 실제 response 생성에 사용된 마지막 generation slot 기준으로 기록한다.
- provider/model 값이 없으면 존재하는 key만 남긴다.
- `middlewareLogs`는 public debug metadata이며 prompt, raw response, tool data를 포함하지 않는다.
- tool result 전체 data를 metadata에 넣지 않는다.
- tool data는 모델 follow-up prompt에만 들어간다.
- cache key는 기존처럼 selected agent, selected leaf agent, payload, context를 포함한다. tool result는 payload/context에서 파생된 값이므로 별도 cache key에 추가하지 않는다.

## 10. 실패 정책

| 상황 | 처리 |
| --- | --- |
| routing output이 `tool_call` | `ROUTING_UNAVAILABLE` |
| generation output이 unknown tool | `UNKNOWN_TOOL` result를 follow-up prompt에 전달 |
| generation output이 mixed `tool_call` | `INVALID_TOOL_CALL` result를 follow-up prompt에 전달 |
| `tool_call` shape이 잘못됨 | `INVALID_TOOL_CALL` result를 follow-up prompt에 전달 |
| tool args가 object가 아님 | `INVALID_TOOL_ARGS` result를 follow-up prompt에 전달 |
| tool 실행 중 예외 | `TOOL_EXECUTION_ERROR` result를 follow-up prompt에 전달 |
| follow-up LLM이 `None` | deterministic fallback |
| follow-up LLM이 invalid JSON | `INVALID_LLM_RESPONSE` |
| follow-up에서 다시 `tool_call` | deterministic fallback |
| tool result 이후 final response schema 실패 | 기존 `INVALID_AGENT_RESPONSE` |

주의:

- provider 장애와 prompt/output 형식 오류는 계속 구분한다.
- tool execution failure는 provider 장애가 아니므로 LLM slot fallback을 바로 타지 않는다.
- tool follow-up LLM unavailable/`None`은 fallback slot 순서를 다시 타지 않고 deterministic fallback으로 복구한다.

## 11. Provider-native function calling 확장 지점

v1은 `LLMAdapter.invoke(prompt) -> str | None` 계약을 유지한다.

후속 단계에서 native function calling을 붙일 경우:

- `LLMAdapter`에 별도 method를 추가한다.
- provider별 tool schema 변환은 `llm` 패키지 안에 둔다.
- tool registry와 executor는 provider-neutral 구조를 유지한다.
- pipeline은 provider-native tool call과 provider-neutral `tool_call` JSON을 같은 `ToolCallRequest`로 정규화한다.

후속 후보:

```python
class LLMAdapter(Protocol):
    def invoke(self, prompt: str) -> str | None: ...
    def invoke_with_tools(
        self,
        prompt: str,
        tools: list[ToolDefinition],
    ) -> str | ToolCallRequest | None: ...
```

이 확장은 v1 구현 범위가 아니다.

## 12. LangChain built-in middleware/tools 참고

2026-06-01 기준 공식 문서 확인 결과:

- Python prebuilt middleware: https://docs.langchain.com/oss/python/langchain/middleware/built-in
- Python tools overview: https://docs.langchain.com/oss/python/langchain/tools

LangChain의 prebuilt middleware는 `create_agent(..., middleware=[...])` 구조에 붙는 provider-agnostic middleware다. 현재 설계는 기존 `AgentPipeline`, LangGraph `StateGraph`, `LLMAdapter.invoke(prompt) -> str | None` 계약을 유지하므로 LangChain middleware를 직접 채택하지 않는다. built-in middleware는 필수가 아니며, 이 문서의 v1 구현은 LangChain dependency를 추가하지 않는다. 다만 구현 시 개념과 테스트 기준은 아래 항목을 참고한다.

| LangChain 항목 | 용도 | v1 채택 여부 |
| --- | --- | --- |
| `SummarizationMiddleware` | context window 접근 시 대화 이력 요약 | 미채택. 현재 pipeline에는 장기 대화 이력 압축 요구가 없다. |
| `HumanInTheLoopMiddleware` | 특정 tool call 전 human approval interrupt | 미채택. v1 tool은 read-only라 approval 대상이 아니다. |
| `ModelCallLimitMiddleware` | run/thread 단위 model call 제한 | 참고. v1은 tool loop max 1과 기존 fallback slot 정책으로 제한한다. |
| Tool call limit middleware | tool call 수 제한 | 참고. v1은 `toolCallCount <= 1`로 자체 구현한다. |
| Model fallback middleware | primary model 실패 시 fallback model 사용 | 미채택. 기존 default/fallback1/fallback2 slot 정책을 유지한다. |
| PII detection middleware | 입력/출력의 개인정보 redact/mask/block | 미채택. 별도 보안 요구가 생기면 pipeline boundary에서 검토한다. |
| To-do list middleware | agent에게 task planning/tracking tool 제공 | 미채택. factory runtime response에는 불필요한 planning state를 노출하지 않는다. |
| LLM tool selector middleware | 많은 tool 중 관련 tool subset 선택 | 미채택. v1 catalog는 4개 read-only tool이라 selector가 과하다. |
| Tool retry middleware | 실패한 tool call exponential backoff 재시도 | 미채택. v1 tool은 local context read라 transient retry 대상이 아니다. |
| Model retry middleware | 실패한 model call exponential backoff 재시도 | 미채택. 기존 fallback policy와 provider 장애 구분을 우선한다. |
| LLM tool emulator middleware | test용 tool execution emulation | 미채택. unit/integration test에서 fake adapter와 fake registry로 검증한다. |
| Context editing middleware | tool result/history trim 또는 clear | 참고. v1은 tool result를 public metadata에 넣지 않는 방식으로 context 노출을 제한한다. |
| Shell tool middleware | agent에게 persistent shell 실행 tool 제공 | 제외. factory agent v1 범위와 보안 경계 밖이다. |
| File search middleware | glob/grep 기반 file search tool 제공 | 제외. v1 tool은 filesystem을 읽지 않는다. |
| Filesystem middleware | agent filesystem과 long-term memory 제공 | 제외. v1 tool은 payload/context만 읽는다. |
| Subagent middleware | subagent spawn/task delegation 제공 | 제외. 현재 agent routing 구조와 별도 설계 대상이다. |

LangChain tools 문서는 web search, code interpretation, database access 등 prebuilt tools/toolkits를 제공한다고 설명한다. 이 설계의 v1 tool catalog에는 이를 포함하지 않는다.

제외 이유:

- web search/code interpreter/database/shell/filesystem tool은 외부 side effect 또는 보안 경계가 커진다.
- provider built-in tool은 provider별 schema와 runtime 의미가 달라 v1 provider-neutral JSON 계약과 섞지 않는다.
- SQL/database tool은 factory request payload가 아니라 별도 persistence layer를 요구한다.
- 현재 필요한 정보는 `payload`와 `AgentContext.metadata`에서 결정적으로 읽을 수 있다.

후속 검토 조건:

- tool catalog가 10개 이상으로 늘어나면 LangChain의 LLM tool selector 패턴을 참고한다.
- write/update/delete tool을 도입하면 human-in-the-loop approval과 tool call limit을 먼저 설계한다.
- 외부 API 또는 database tool을 도입하면 tool retry, rate limit, audit log, timeout 정책을 함께 설계한다.
- 대화 이력 기반 agent가 필요해지면 summarization/context editing middleware 패턴을 검토한다.

## 13. 구현 단계 제안

1. RED artifact 작성
   - `_workspace/factory-agent/task_tool_middleware_red.md`
2. tool schema와 registry unit test 작성
3. read-only context tools unit test 작성
4. tool middleware parsing/execution/follow-up prompt test 작성
5. LangGraph pipeline integration test 작성
6. production code 구현
7. reviewer sub-agent 리뷰와 재리뷰
8. smoke 여부 판단

## 14. 테스트 기준

필수 unit test:

- registered tool lookup 성공
- unknown tool lookup 실패 result
- args가 object가 아닐 때 실패 result
- read-only tool이 payload/context를 mutate하지 않음
- context tool이 payload fallback 경로를 올바르게 읽음

필수 pipeline test:

- `agent.middleware.before`, `agent.middleware.fallback`, `agent.middleware.after`가 LangGraph node로 등록된다.
- cache miss는 `agent.middleware.before`를 1회 통과한 뒤 selected leaf agent prompt를 만든다.
- cache hit은 agent middleware를 통과하지 않는다.
- 모든 LLM slot이 실패하면 `agent.middleware.fallback`으로 분기해 deterministic fallback을 반환한다.
- successful generation과 deterministic fallback 모두 `agent.middleware.after`를 통과한다.
- metadata에 `middlewareLogs`와 `currentModel` summary가 남는다.
- generation LLM이 tool request를 반환하면 tool 실행 후 follow-up prompt가 호출된다.
- tool result 이후 final response JSON이 `agent.response`로 반환된다.
- metadata에 `toolCalls` summary가 남는다.
- routing LLM이 tool request를 반환하면 `ROUTING_UNAVAILABLE`이다.
- follow-up에서 다시 tool request가 나오면 deterministic fallback이다.
- tool follow-up LLM이 unavailable이면 deterministic fallback이다.
- fallback1이 tool request를 반환하면 tool follow-up도 fallback1 adapter만 1회 재사용한다.
- tool follow-up이 실패해도 default/fallback1/fallback2 slot 순서를 다시 실행하지 않는다.
- mixed `tool_call` output은 정상 Agent response로 통과하지 않고 invalid tool result로 follow-up된다.
- malformed `tool_call` output은 정상 Agent response로 통과하지 않고 invalid tool result로 follow-up된다.
- tool result `data`는 public response payload나 metadata에 직접 노출되지 않는다.

필수 검증:

```bash
uv run --extra dev pytest
uv run --extra dev ruff check .
```

## 15. 문서 업데이트 대상

구현 단계에서는 다음 문서도 함께 갱신한다.

- `backend/src/FOLDER_ROLES.md`: `tools/`와 `pipeline/tool_middleware.py` 책임 추가
- `backend/docs/message-protocol.md`: public protocol이 바뀌지 않는다는 점 명시
- `backend/docs/agent-development-guide.md`: Agent prompt에서 tool 사용 계약 추가
- `backend/docs/plans/llm_implementation_plan.md`: provider-native tool calling은 후속 확장임을 연결

## 16. 구현 기본값

구현자가 추가 결정 없이 진행할 수 있도록 v1 기본값은 다음으로 고정한다.

- tool follow-up 호출은 tool request를 반환한 generation slot과 같은 adapter를 1회 재사용한다.
- follow-up 호출에는 `default -> fallback1 -> fallback2` 순서를 다시 태우지 않는다.
- `toolCalls` metadata에는 `name`과 `ok`만 남긴다. error code와 tool data는 남기지 않는다.
- read-only context source는 request payload와 `AgentContext.metadata`로 제한한다.
- repository, database, external API, Unreal runtime 조회는 v1 tool source에 포함하지 않는다.
- tool loop limit 초과는 별도 public error code 없이 deterministic fallback으로 복구한다.
