# Backend 결정 로그

이 문서는 backend 구조를 정하는 과정에서 나온 질문과 답변을 결정 사항으로 기록한다.

## 1. `agents/pipeline/`는 오케스트레이터 에이전트인가?

결정: `agents/pipeline/`는 오케스트레이터 에이전트가 아니라 공통 실행 파이프라인이다.

역할:

- request context 생성
- agent id 기반 실제 Agent 조회
- payload validation
- cache lookup/write
- LLM adapter 호출
- LLM 실패 시 fallback 전환
- response schema validation
- `agent.response` 또는 `agent.error` 매핑

구분:

- `agents/router.py`: agent id를 실제 Agent 구현체에 매핑한다.
- `agents/pipeline/`: 공통 실행 흐름을 제어한다.
- `agents/process_optimizer.py`: 공정 최적화 Agent다.
- `agents/quest_generator/agent.py`: 퀘스트 생성 도메인 오케스트레이터다.
- `agents/manual_qa/agent.py`: 매뉴얼 Q&A 도메인 오케스트레이터다.
- `agents/new_material_generator.py`: 신물질 생성 Agent다.

후속 검토:

- `pipeline/` 패키지 안에서는 `runtime.py`, `graph_edges.py`, `llm_fallback.py`, `state.py`, `utils.py`처럼 책임별 파일명을 사용한다.

## 2. 오케스트레이터 에이전트를 추가할 것인가?

결정: 오케스트레이터 에이전트를 별도 Agent로 추가한다.

파일:

- `agents/orchestrator.py`

역할:

- 사용자 요청 또는 모호한 `agent.request`의 의도를 분석한다.
- 요청을 처리할 전문 Agent를 선택한다.
- 필요하면 전문 Agent에 넘길 payload 형태를 정리한다.
- 라우팅 결정 근거를 metadata에 남긴다.

구분:

- `agents/pipeline/`는 실행 흐름을 제어하는 인프라 계층이다.
- `agents/orchestrator.py`는 어떤 전문 Agent를 쓸지 판단하는 Agent다.
- `agents/router.py`는 agent id를 구현체에 매핑하는 registry다.

제약:

- 오케스트레이터는 WebSocket connection을 직접 다루지 않는다.
- 오케스트레이터는 cache, LLM 호출 retry, response envelope 생성을 직접 소유하지 않는다.
- 오케스트레이터가 선택할 수 있는 전문 Agent는 `process_optimizer`, `quest_generator`, `manual_qa`, `new_material_generator`로 제한한다.

## 3. `manual_qa`, `quest_generator`를 서브 에이전트로 분리할 필요가 있는가?

결정: `manual_qa`와 `quest_generator`를 서브 에이전트 구조로 분리한다.

이유:

- `manual_qa`는 레시피 설명, 장비 도움말, 문제 해결이 서로 다른 prompt와 fallback 정책을 가질 가능성이 높다.
- `quest_generator`는 튜토리얼, 생산, 탐험, 경제 퀘스트가 서로 다른 생성 정책과 테스트 기준을 가질 가능성이 높다.
- 초기부터 패키지 구조를 분리하면 각 도메인별 prompt, schema, fallback을 독립적으로 키울 수 있다.
- 도메인 오케스트레이터는 서브 에이전트 선택과 공통 응답 정규화만 맡고, 세부 생성 정책은 서브 에이전트가 맡는다.

현재 구조:

- `agents/manual_qa/agent.py`: 매뉴얼 Q&A 도메인 오케스트레이터다.
- `agents/manual_qa/recipe_explainer.py`: 레시피 설명 서브 에이전트다.
- `agents/manual_qa/machine_help.py`: 장비 도움말 서브 에이전트다.
- `agents/manual_qa/troubleshooter.py`: 문제 해결 서브 에이전트다.
- `agents/quest_generator/agent.py`: 퀘스트 생성 도메인 오케스트레이터다.
- `agents/quest_generator/tutorial_quest.py`: 튜토리얼 퀘스트 서브 에이전트다.
- `agents/quest_generator/production_quest.py`: 생산 퀘스트 서브 에이전트다.
- `agents/quest_generator/exploration_quest.py`: 탐험 퀘스트 서브 에이전트다.
- `agents/quest_generator/economy_quest.py`: 경제 퀘스트 서브 에이전트다.

책임 경계:

- 전체 서버 오케스트레이터인 `agents/orchestrator.py`는 어떤 최상위 Agent를 쓸지 선택한다.
- `manual_qa/agent.py`는 매뉴얼 Q&A 내부에서 어떤 서브 에이전트를 쓸지 선택한다.
- `quest_generator/agent.py`는 퀘스트 생성 내부에서 어떤 서브 에이전트를 쓸지 선택한다.
- `agents/pipeline/`는 여전히 공통 실행 흐름만 담당하고 서브 에이전트 정책을 소유하지 않는다.

서브 에이전트 목록:

- `manual_qa.recipe_explainer`
- `manual_qa.machine_help`
- `manual_qa.troubleshooter`
- `quest_generator.tutorial_quest`
- `quest_generator.production_quest`
- `quest_generator.exploration_quest`
- `quest_generator.economy_quest`

## 4. LangGraph는 어디에 적용하는가?

결정: LangGraph는 WebSocket transport가 아니라 `agents/pipeline/`의 Agent 실행 흐름에 적용한다.

구조:

- WebSocket gateway는 raw message 수신과 response 송신만 담당한다.
- `agents/pipeline/`가 검증된 `agent.request`를 LangGraph input으로 넣는다.
- LangGraph state는 envelope, context, selectedAgent, selectedLeafAgent, typedPayload, cacheKey, prompt, llmRaw, responsePayload, streams, error를 가진다.
- 최종 graph output은 `agent.response` 또는 `agent.error` envelope다.

라우팅:

- `route_top_agent`는 최상위 Agent routing prompt를 호출하고, 모델 raw decision 문자열을 `selectedAgent` state에 기록한다.
- `route_selected_agent` conditional edge는 `selectedAgent` state가 `TOP_LEVEL_AGENT_IDS` 중 하나인지 검증하고 최상위 Agent 경로를 구분한다.
- `manual_qa.route_sub_agent`는 domain leaf routing prompt를 호출하고 모델 raw decision 문자열을 `selectedLeafAgent` state에 기록한다.
- `quest_generator.route_sub_agent`는 domain leaf routing prompt를 호출하고 모델 raw decision 문자열을 `selectedLeafAgent` state에 기록한다.
- `route_selected_leaf_agent` conditional edge는 `selectedLeafAgent` state가 선택된 top-level Agent의 허용 leaf Agent id인지 검증하고 다음 경로를 구분한다.

제약:

- WebSocket connection object는 LangGraph state에 넣지 않는다.
- streaming event는 graph output의 `streams` 목록으로 반환하고, 실제 송신은 WebSocket gateway가 담당한다.
- top-level routing LLM 실패는 generation fallback 전에 `agent.error` / `ROUTING_UNAVAILABLE`로 종료한다.
- Agent 실행 단계의 LLM slot 실패는 graph 안에서 generation fallback node로 복구한다.
- fallback schema 검증 실패, validation error, routing failure는 `agent.error`로 종료할 수 있다.

## 5. 오케스트레이터는 몇 개인가?

결정: 서버 전체 오케스트레이터는 1개만 둔다. 다만 `manual_qa/agent.py`와 `quest_generator/agent.py`는 각 도메인 내부에서 서브 에이전트를 조율하므로 도메인 오케스트레이터로 부른다.

현재 서버 전체 오케스트레이터:

- `agents/orchestrator.py`: 전체 요청 의도를 보고 최상위 전문 Agent를 선택한다.

현재 도메인 오케스트레이터:

- `manual_qa/agent.py`: 매뉴얼 Q&A 도메인 내부에서 서브 에이전트를 선택하고 결과를 정규화한다.
- `quest_generator/agent.py`: 퀘스트 생성 도메인 내부에서 서브 에이전트를 선택하고 결과를 정규화한다.

오케스트레이터가 아닌 것:

- `agents/pipeline/`: LangGraph 실행 흐름을 소유하는 공통 실행 파이프라인이다.
- `agents/router.py`: agent id를 구현체에 매핑하는 registry/router다.
- LangGraph의 `route_top_agent`, `manual_qa.route_sub_agent`, `quest_generator.route_sub_agent` 노드는 Agent 파일이나 오케스트레이터 내부 구현이 아니라 pipeline의 실행 node다.
- 각 node는 해당 Agent의 prompt 계약이나 parser를 호출할 수 있지만, node 자체의 소유자는 `agents/pipeline/`이다.

정리:

- 서버 전체 오케스트레이터: 1개
- 도메인 오케스트레이터: 2개
- 공통 실행 파이프라인: 1개
- LangGraph 라우팅 노드: 실행 단계이며 Agent 개수에 포함하지 않는다.

## 6. `quest_generator.route_sub_agent`는 무슨 역할인가?

결정: 서브 에이전트를 구분하는 주체는 도메인 오케스트레이터인 `agents/quest_generator/agent.py`다. `quest_generator.route_sub_agent`는 별도 Agent가 아니라 `quest_generator` 도메인 오케스트레이터가 제공하는 routing 동작을 LangGraph 노드 이름으로 표현한 것이다.

역할:

- `route_top_agent`가 최상위 Agent로 `quest_generator`를 선택한 뒤 실행된다.
- 퀘스트를 직접 생성하지 않는다.
- `quest_generator` 도메인 오케스트레이터가 요청 payload와 context를 보고 어떤 퀘스트 서브 에이전트가 처리할지 결정한다.
- 결과는 LangGraph state의 `selectedLeafAgent`에 기록한다.

선택 가능한 서브 에이전트:

- `tutorial_quest`: 초반 진행, 기능 안내, 온보딩 목적의 퀘스트
- `production_quest`: 생산량, 병목, 레시피, 설비 운용 목표 퀘스트
- `exploration_quest`: 신규 재료, 맵/지역, 탐색 이벤트 기반 퀘스트
- `economy_quest`: 비용, 수익, 거래, 재고, 효율 기반 퀘스트

판단 입력:

- 명시된 quest type
- 플레이어 진행도
- 현재 공장 상태
- 최근 이벤트
- 튜토리얼 필요 여부
- 경제/생산/탐험 중 현재 우선순위

구현 위치:

- `agents/quest_generator/agent.py`

이름 규칙:

- 코드 주체 이름은 `QuestGeneratorAgent`로 둔다.
- 역할 이름은 도메인 오케스트레이터다.
- LangGraph 노드 이름은 `quest_generator.route_sub_agent`처럼 `도메인 Agent.내부 결정 단계` 형식으로 둔다.
- 이 이름은 실행 trace를 읽기 쉽게 하기 위한 것이며, 별도 파일이나 별도 Agent 개수를 의미하지 않는다.

제약:

- WebSocket message를 직접 읽거나 쓰지 않는다.
- LLM 호출을 직접 소유하지 않는다.
- cache key 생성, retry, fallback, response envelope 생성은 `agents/pipeline/`가 담당한다.
- routing 실패나 판단 불가 상태는 기본 sub-agent 선택 정책으로 복구하지 않는다.
- 명시적으로 전달된 유효한 `sub_agent`만 검증 후 사용할 수 있다.
- LLM router가 허용된 `sub_agent`를 반환하지 못하면 `ROUTING_UNAVAILABLE` error로 종료한다.

## 7. 각 Agent 역할은 어디에 기록하는가?

결정: Agent별 세부 책임은 `AGENT_ROLES.md`에 별도 문서로 기록한다.

이유:

- `FOLDER_ROLES.md`는 폴더와 모듈 책임을 설명하는 문서로 유지한다.
- Agent별 입력, 출력, 판단 기준, 금지 책임은 별도 문서가 더 읽기 쉽다.
- 오케스트레이터, 도메인 오케스트레이터, leaf Agent, 실행 구성요소를 한 문서에서 비교할 수 있어야 한다.

기록 대상:

- 서버 전체 오케스트레이터
- 도메인 오케스트레이터
- 공정 최적화 Agent
- 신물질 생성 Agent
- 매뉴얼 Q&A 서브 에이전트
- 퀘스트 생성 서브 에이전트
- Agent가 아닌 실행 구성요소인 `pipeline/`, `router.py`, `base.py`

## 8. `agents/orchestrator.py`에서 leaf Agent까지 바로 분기해도 되는가?

결정: 기술적으로는 가능하지만 기본 구조로 채택하지 않는다.

채택하지 않는 이유:

- 서버 전체 오케스트레이터가 모든 leaf Agent를 알게 되면 도메인 내부 정책까지 소유하게 된다.
- `manual_qa`와 `quest_generator`는 서브 에이전트 선택 기준이 다르다.
- leaf Agent가 추가될 때마다 `agents/orchestrator.py`를 수정하면 변경 영향 범위가 커진다.
- 도메인별 fallback, payload 정규화, 선택 근거 metadata를 도메인 내부에 묶어두는 편이 유지보수에 유리하다.

현재 원칙:

- `agents/orchestrator.py`는 최상위 Agent만 선택한다.

## 9. `build_agent_graph`는 `AgentPipeline` 밖에 둘 필요가 있는가?

결정: graph 생성은 `AgentPipeline` 내부 책임으로 둔다.

이유:

- `AgentPipeline`이 `router`, `cache`, `llm`, `llm_settings`, `llm_adapter_factory`를 이미 소유한다.
- graph 생성 함수가 외부에 있으면 같은 의존성을 다시 인자로 넘겨야 해서 DI 경로가 중복된다.
- 현재 `build_agent_graph()` 직접 사용은 테스트의 compiled graph 확인뿐이므로 public API로 둘 필요가 약하다.

구조:

- `AgentPipeline.__init__()`은 `self._build_graph()`를 호출한다.
- `_build_graph()`는 `self`에 저장된 의존성을 사용한다.
- 외부 사용자는 `AgentPipeline` 또는 `run_agent_pipeline()`을 진입점으로 사용한다.

제약:

- graph node/edge 구현은 여전히 `agents/pipeline/runtime.py`, `graph_edges.py`, `llm_fallback.py`, `state.py`, `utils.py`로 기능별 분리한다.
- `AgentPipeline`이 도메인 판단을 직접 소유하지 않는다는 기존 원칙은 유지한다.

## 10. routing decision parser가 필요한가?

결정: Agent routing에는 별도 JSON decision parser를 두지 않는다. Global Orchestrator와 Domain Orchestrator의 routing prompt는 허용 id 중 하나의 문자열만 반환하게 하고, Agent 경로 구분은 LangGraph conditional edge가 담당한다.

구분:

- `OrchestratorAgent`: top-level routing prompt 계약만 소유한다.
- `route_top_agent`: LLM raw decision 문자열을 `selectedAgent` state에 기록한다.
- `route_selected_agent`: `selectedAgent`가 `TOP_LEVEL_AGENT_IDS` 중 하나인지 보고 실제 top-level Agent 경로를 구분한다.
- `ManualQaAgent`: manual Q&A domain leaf routing prompt 계약만 소유한다.
- `QuestGeneratorAgent`: quest generator domain leaf routing prompt 계약만 소유한다.
- `manual_qa.route_sub_agent`: LLM raw decision 문자열을 `selectedLeafAgent` state에 기록한다.
- `quest_generator.route_sub_agent`: LLM raw decision 문자열을 `selectedLeafAgent` state에 기록한다.
- `route_selected_leaf_agent`: `selectedLeafAgent`가 해당 top-level Agent의 허용 leaf Agent id 중 하나인지 보고 다음 실행 단계와 error 경로를 구분한다.

Pipeline 흐름:

- `route_top_agent`는 prompt 호출 결과 문자열을 `selectedAgent` state에 기록한다.
- `route_selected_agent` conditional edge가 `selectedAgent` state를 보고 실제 top-level Agent 경로를 구분한다.
- `manual_qa.route_sub_agent`와 `quest_generator.route_sub_agent`도 prompt 호출 결과 문자열을 `selectedLeafAgent` state에 기록한다.
- `route_selected_leaf_agent` 이후의 LangGraph edge가 다음 실행 단계로 진행할지 error로 갈지 구분한다.

이유:

- 최상위 Agent 선택 결과는 public envelope의 `agent` 개념과 대응한다.
- 서브 에이전트 선택 결과는 domain-internal `sub_agent` 개념과 대응한다.
- routing parser를 두면 prompt decision을 다시 중간 로직에 태우게 되어 LangGraph conditional edge 책임이 흐려진다.
- routing prompt와 generation prompt의 출력 계약을 분리한다. routing은 허용 id 문자열 하나, leaf generation은 Agent별 response JSON이다.

## 11. 최상위 Agent 구분은 코드 로직인가 LangGraph conditional인가?

결정: 최상위 Agent 구분은 LangGraph conditional edge가 담당한다. `route_top_agent` node는 prompt를 호출하고 `selectedAgent` state를 기록하는 역할만 맡는다.

오케스트레이터 역할:

- `OrchestratorAgent`는 최상위 Agent 판단을 위한 prompt 계약을 소유한다.
- 허용 가능한 최상위 Agent 후보는 `TOP_LEVEL_AGENT_IDS`로 제한한다.
- LLM에는 `TOP_LEVEL_AGENT_IDS` 중 하나만 반환하도록 요구한다.
- prompt는 `[ROLE]`, `[TASK]`, `[ALLOWED_AGENT_IDS]`, `[REQUEST_HINT]`, `[REQUEST_CONTEXT]`, `[REQUEST_PAYLOAD]`, `[OUTPUT_CONTRACT]` 섹션을 가진 structured prompt로 구성한다.
- output contract는 단일 Agent id 문자열만 허용하고 JSON, markdown, 설명, reason, 따옴표, 코드블록은 금지한다.
- `OrchestratorAgent`는 Agent 실행, cache, fallback, response envelope 생성, LangGraph edge 분기를 소유하지 않는다.
- 실제 경로 구분은 `route_selected_agent` conditional edge가 담당한다.

수정된 흐름:

- 명시 `agent`가 있어도 코드가 바로 선택하지 않는다.
- 명시 `agent` 값은 `OrchestratorAgent.build_routing_prompt()`의 `requested_agent` hint로만 전달한다.
- `route_top_agent`는 모델 응답 문자열을 `selectedAgent` state에 기록한다.
- 실제 분기는 `route_selected_agent` conditional edge가 `selectedAgent` 값을 보고 `process_optimizer`, `manual_qa`, `quest_generator`, `new_material_generator`, `error` 경로로 나눈다.

이유:

- Agent 선택 자체는 prompt 기반 orchestrator가 해야 한다는 원칙을 지킨다.
- 코드의 역할은 모델 출력을 검증 가능한 state로 정규화하고 LangGraph edge가 분기하도록 만드는 것이다.
- `if envelope.agent then select` 같은 shortcut은 prompt 기반 routing을 우회하므로 사용하지 않는다.

주의:

- routing model이 유효한 최상위 Agent 결정을 반환하지 못하면 명시 `agent`가 있어도 `ROUTING_UNAVAILABLE`로 종료한다.
- 이 변경으로 API key나 local routing model이 없는 기본 WebSocket `agent.request`는 deterministic fallback 전에 top-level routing 단계에서 실패할 수 있다.
- `manual_qa/agent.py`는 매뉴얼 Q&A 내부 서브 에이전트를 선택한다.
- `quest_generator/agent.py`는 퀘스트 생성 내부 서브 에이전트를 선택한다.

허용 가능한 예외:

- leaf Agent가 하나뿐인 단순 도메인은 별도 도메인 오케스트레이터 없이 `agents/orchestrator.py`에서 바로 선택해도 된다.
- 여러 도메인에 공통으로 적용되는 safety block, 권한 차단, 긴급 fallback은 `agents/orchestrator.py`에서 처리할 수 있다.

## 9. 오케스트레이션 구조 최종 방향

결정: 원래 방향대로 계층형 오케스트레이션 구조를 유지한다.

최종 구조:

- `agents/orchestrator.py`: 서버 전체 오케스트레이터. 최상위 Agent만 선택한다.
- `agents/manual_qa/agent.py`: 매뉴얼 Q&A 도메인 오케스트레이터. 매뉴얼 서브 에이전트를 선택한다.
- `agents/quest_generator/agent.py`: 퀘스트 생성 도메인 오케스트레이터. 퀘스트 서브 에이전트를 선택한다.

채택하지 않는 구조:

- `agents/orchestrator.py`에서 모든 leaf Agent까지 직접 분기하는 중앙 집중형 구조

이유:

- 도메인별 세부 분기 정책을 각 도메인 내부에 유지한다.
- 서버 전체 오케스트레이터의 책임을 최상위 도메인 선택으로 제한한다.
- leaf Agent 추가나 분기 기준 변경이 전체 오케스트레이터로 번지지 않게 한다.

## 10. Agent routing은 로직으로 구분하는가?

결정: Agent가 판단하는 routing은 코드 로직으로 구분하지 않고 prompt 기반 LLM 결정으로 처리한다.

원칙:

- `agents/orchestrator.py`는 routing prompt 계약을 만들고 LLM이 `TOP_LEVEL_AGENT_IDS` 중 하나의 문자열만 반환하도록 요구한다.
- `manual_qa/agent.py`와 `quest_generator/agent.py`도 같은 방식으로 domain leaf routing prompt를 만든다.
- 명시적으로 전달된 top-level `agent` 값은 prompt hint로만 사용하며 직접 선택하지 않는다.
- 명시적으로 전달된 `sub_agent` 값은 top-level Agent가 확정된 뒤 해당 도메인의 허용 목록으로 검증 후 사용할 수 있다.
- keyword, if/else, score table 같은 코드 로직으로 Agent나 sub-agent를 추론하지 않는다.
- LLM routing 결과가 없거나 허용 목록 밖이면 임의 fallback 선택을 하지 않고 routing error로 종료한다.

이유:

- 오케스트레이터 Agent의 역할은 규칙 기반 router가 아니라 의도 판단이다.
- routing 기준이 prompt에 있어야 모델 교체, prompt tuning, eval 설계가 가능하다.
- 코드 로직으로 분기하면 Agent를 쓰는 의미가 줄고, 도메인 정책이 다시 하드코딩된다.

## 11. WebSocket package 이름은 무엇으로 하는가?

결정: transport package 이름은 `websocket`이 아니라 `websocket_gateway`로 둔다.

이유:

- `websocket`은 Python 환경에서 외부 패키지 이름과 충돌할 수 있다.
- 실제 테스트에서 `from websocket.gateway import ...`가 외부 `websocket` package와 충돌해 import가 실패했다.
- `websocket_gateway`는 역할이 명확하고 충돌 가능성이 낮다.

현재 구조:

- `websocket_gateway/gateway.py`: `/ws/agent` endpoint와 JSON 송수신
- `websocket_gateway/connection.py`: WebSocket connection helper 영역

## 12. 명시된 `sub_agent`가 잘못된 경우 어떻게 처리하는가?

결정: 명시된 `sub_agent`는 authoritative input으로 보고, 허용 목록 검증에 실패하면 LLM routing으로 우회하지 않고 error로 종료한다.

원칙:

- `manual_qa` 요청의 `sub_agent`는 `manual_qa.*` 허용 목록에 있어야 한다.
- `quest_generator` 요청의 `sub_agent`는 `quest_generator.*` 허용 목록에 있어야 한다.
- `process_optimizer` 요청의 `sub_agent`는 없거나 `process_optimizer`여야 한다.
- `new_material_generator` 요청의 `sub_agent`는 없거나 `new_material_generator`여야 한다.
- 명시 `sub_agent`가 없을 때만 해당 도메인 오케스트레이터의 routing prompt를 호출한다.
- 명시 `sub_agent`가 있지만 현재 top-level Agent와 맞지 않거나 허용 목록 밖이면 `INVALID_SUB_AGENT`를 반환한다.

이유:

- 클라이언트가 잘못 보낸 명시 값을 LLM이 조용히 다른 선택으로 덮어쓰면 입력 오류가 숨는다.
- top-level `agent` 검증과 같은 방식으로 explicit routing input을 다룬다.

## 13. Agent response cache key는 무엇을 포함하는가?

결정: response cache key는 `agent`, `leaf_agent`, `payload`, 실행 context metadata를 포함한다.

원칙:

- cache key에는 `session_id`, `client_id`, `context.metadata`를 포함한다.
- `request_id`는 cache key에 넣지 않는다.
- Agent prompt는 request id에 의존하지 않는다.
- context에 따라 prompt나 응답이 달라질 수 있으므로 payload만으로 cache key를 만들지 않는다.

이유:

- 같은 payload라도 세션, 클라이언트, 선택 화면, 공장 구역 등 context가 다르면 응답이 달라질 수 있다.
- request id는 추적용 값이므로 prompt와 cache 의미에 들어가면 안 된다.

## 15. 현재 리뷰에서 발견된 pipeline 문제는 무엇인가?

결정: 다음 두 이슈는 구현 전에 테스트로 고정하고 수정해야 한다.

### 15.1 malformed envelope 오류에서 request correlation이 보존되지 않는다

문제:

- `AgentPipeline.run()`이 `AgentRequestEnvelope.model_validate()`를 먼저 호출한다.
- `type`이 `agent.request`가 아니거나 `payload`가 object가 아니면 Pydantic `ValidationError`가 먼저 발생한다.
- 이 경우 graph 내부의 `INVALID_MESSAGE_TYPE`, `INVALID_PAYLOAD` 분기까지 도달하지 않는다.
- `_build_validation_error()`는 raw message를 알지 못해서 `request_id`, `session_id`, `client_id`, `agent`를 error envelope에 보존하지 못한다.

영향:

- 클라이언트가 잘못된 요청에 대한 `agent.error`를 받아도 어떤 요청의 오류인지 매칭하기 어렵다.
- `validate_envelope()`에 존재하는 세부 error branch가 dict 입력 경로에서는 사실상 unreachable해진다.

수정 방향:

- 잘못된 type 또는 payload에서도 raw dict의 correlation field를 보존하는 테스트를 먼저 추가한다.
- validation error builder가 raw message에서 `request_id`, `session_id`, `client_id`, `agent`를 가능한 한 보존하도록 수정한다.
- 또는 envelope parsing을 느슨하게 분리해서 graph 내부 validation branch가 실제로 실행되도록 한다.

### 15.2 cache hit 응답에서 원래 response metadata가 사라진다

문제:

- cache write는 `responsePayload`만 저장한다.
- cache hit은 `responseMetadata`를 `{"cache": "hit"}`로 새로 만든다.
- 첫 응답에 있던 `fallback: true`, `llm: used` 같은 실행 metadata가 hit 응답에서 사라진다.

영향:

- 같은 요청의 첫 응답과 cache hit 응답이 metadata 관점에서 다른 의미를 가진다.
- 클라이언트나 디버깅 도구가 fallback/LLM 사용 여부를 일관되게 추적하기 어렵다.

수정 방향:

- cache entry에 response payload와 response metadata를 함께 저장한다.
- hit 응답에서는 기존 metadata를 유지하면서 `cache: hit`만 추가한다.
- 첫 응답과 cache hit 응답의 metadata 일관성을 검증하는 테스트를 추가한다.

## 16. Agent prompt는 어떤 언어로 작성하는가?

결정: backend agent와 orchestrator에 들어가는 prompt 본문은 한글로 작성한다.

대상:

- `agents/orchestrator.py`의 top-level routing prompt
- `agents/manual_qa/agent.py`의 manual Q&A domain leaf routing prompt
- `agents/quest_generator/agent.py`의 quest domain leaf routing prompt
- leaf agent의 `build_prompt()` prompt

원칙:

- agent id, sub_agent id, JSON key 이름은 protocol 계약이므로 영어 식별자를 유지한다.
- prompt 지시문, 역할 설명, 요청 설명은 한글로 작성한다.
- top-level routing prompt는 `TOP_LEVEL_AGENT_IDS` 중 하나의 id만 반환하도록 요구한다.
- domain leaf routing prompt도 해당 도메인의 허용 leaf Agent id만 반환하도록 요구한다.
- leaf Agent generation prompt만 Agent별 response JSON을 반환하도록 요구한다.
- prompt 변경은 테스트에서 한글 문구를 검증해 회귀를 막는다.

## 17. LLM provider 구현은 어떻게 시작하는가?

결정: 1차 실제 LLM provider는 `google-genai` SDK 기반으로 계획한다.

상세 구현 계획은 `backend/docs/plans/llm_implementation_plan.md`, sprint 체크리스트는 `backend/docs/plans/llm_implementation_sprint.md`에 둔다.

## 18. top-level Agent를 모두 오케스트레이터로 처리해야 하는가?

결정: top-level Agent라는 이유만으로 모두 오케스트레이터로 처리하지 않는다. 하위 Agent나 내부 실행 전략을 선택하는 책임이 있는 top-level Agent만 도메인 오케스트레이터로 처리한다.

용어 결정:

- 중간 계층에서 하위 Agent를 고르는 Agent는 `Domain Orchestrator`라고 부른다.
- 한국어 문서에서는 `도메인 오케스트레이터`를 공식 용어로 쓴다.
- `중간 에이전트`는 계층 위치만 말하고 책임이 드러나지 않으므로 공식 용어로 쓰지 않는다.
- `서브 오케스트레이터`는 대화에서는 허용하지만, 문서의 기준 용어는 `도메인 오케스트레이터`로 통일한다.

구분:

- Global Orchestrator: `agents/orchestrator.py`
- Domain Orchestrator: `agents/manual_qa/agent.py`, `agents/quest_generator/agent.py`
- leaf top-level Agent: `agents/process_optimizer.py`, `agents/new_material_generator.py`

원칙:

- top-level routing은 `OrchestratorAgent` prompt가 `TOP_LEVEL_AGENT_IDS` 중 하나를 고르고, `route_selected_agent` conditional edge가 다음 node를 선택한다.
- 선택된 top-level Agent가 내부 서브 에이전트를 다시 선택해야 하면 `manual_qa.route_sub_agent`, `quest_generator.route_sub_agent`처럼 도메인 오케스트레이터 node로 이동한다.
- 선택된 top-level Agent가 내부 분기 없이 직접 실행 가능하면 payload validation 후 `cache_lookup`과 Agent 실행 단계로 이동한다.
- leaf top-level Agent를 억지로 오케스트레이터 클래스로 감싸지 않는다.

이유:

- 모든 top-level Agent를 오케스트레이터로 만들면 실제 하위 선택이 없는 도메인에도 불필요한 routing prompt와 failure point가 생긴다.
- 오케스트레이터는 "상위"라는 위치가 아니라 "다른 실행 단위를 선택하는 책임"으로 정의하는 편이 명확하다.
- 향후 `process_optimizer`가 `bottleneck_detector`, `throughput_planner` 같은 내부 서브 에이전트로 나뉘면 그때 `process_optimizer/agent.py`를 도메인 오케스트레이터로 승격한다.

## 19. 도메인 오케스트레이터가 하위 Agent 선택 말고 다른 일을 하는가?

결정: 현재 도메인 오케스트레이터의 핵심 책임은 하위 Agent 선택이다. 그 외에는 선택을 가능하게 하는 도메인 공통 계약만 가진다. 다만 한 요청에서 여러 하위 Agent를 실행하고 결과를 합쳐야 하는 fan-out 구조가 생기면 도메인 오케스트레이터가 통합 계약을 소유한다.

허용하는 책임:

- 도메인 내부 하위 Agent 후보 목록 소유
- structured routing prompt 계약 생성
- routing prompt에 넣을 도메인 context 정리
- 하위 Agent id 출력 계약 정의
- 선택 결과를 pipeline state의 `selectedLeafAgent`로 넘길 수 있게 하는 계약 제공
- multi-agent 결과 통합이 필요한 경우 merge prompt, merge schema, 충돌 해결 기준, 결과 우선순위 제공

소유하지 않는 책임:

- leaf Agent의 최종 답변 생성
- LLM 호출 실행
- cache lookup/write
- retry/fallback node 제어
- public response/error envelope 생성
- leaf Agent response parsing
- child Agent 실행 scheduling 자체

통합이 필요한 경우:

- 여러 하위 Agent의 결과를 비교하거나 합성해야 하는 요청
- 검색/분석/생성을 서로 다른 하위 Agent가 맡고 최종 답을 하나로 합쳐야 하는 요청
- 도메인 내 충돌 해결 기준이 필요한 요청
- 하위 결과별 confidence, priority, source를 반영해야 하는 요청

통합이 필요 없는 경우:

- 하위 Agent 하나만 선택해 실행하는 현재 `manual_qa`, `quest_generator` 기본 흐름
- leaf Agent 결과를 그대로 public response로 사용할 수 있는 요청
- 결과를 단순 나열하는 수준이라 도메인 정책이 필요 없는 요청

이유:

- 도메인 오케스트레이터가 답변 생성까지 소유하면 leaf Agent와 책임이 겹친다.
- LLM 실행과 fallback을 소유하면 LangGraph pipeline 책임과 겹친다.
- 여러 하위 Agent 결과를 합치는 기준은 도메인마다 다르므로 pipeline 공통 로직보다 도메인 오케스트레이터 계약으로 두는 편이 낫다.
- 지금 단계에서는 `manual_qa`, `quest_generator` 모두 하위 Agent 선택이 주된 역할이므로 얇은 도메인 오케스트레이터로 유지한다.
- 도메인 공통 guardrail, payload normalization, shared retrieval, multi-agent result merge 같은 실제 공통 정책이 생기면 그때 도메인 오케스트레이터 책임으로 추가한다.

## 20. routing 용어를 어떻게 정확히 구분하는가?

결정: 코드와 public response metadata에서는 실행 대상을 `selectedLeafAgent`로 부른다. `selectedSubAgent`는 실제 의미가 부정확하므로 쓰지 않는다.

정확한 명칭:

- `selectedAgent`: Global Orchestrator가 고른 top-level Agent id다.
- `selectedLeafAgent`: 실제 prompt/generation/fallback 실행에 사용할 leaf Agent id다.
- `sub_agent`: public request payload에서 도메인 내부 leaf Agent를 명시하고 싶을 때 쓰는 입력 힌트 이름이다.
- `route_selected_agent`: `selectedAgent`가 허용된 top-level Agent인지 검증하고 다음 LangGraph node를 고르는 conditional edge predicate다.
- `manual_qa.route_sub_agent`, `quest_generator.route_sub_agent`: 도메인 내부 leaf Agent를 고르는 routing node 이름이다. public 입력 필드 `sub_agent`와 대응시키기 위해 node 이름에는 `sub_agent`를 유지한다.
- `route_selected_leaf_agent`: `selectedLeafAgent`가 선택된 top-level Agent의 허용 leaf Agent id인지 검증하고 다음 실행 단계 또는 error 경로를 고르는 conditional edge predicate다.

구분 기준:

- top-level Agent는 서버 전체 라우팅 단위다.
- Domain Orchestrator는 top-level Agent 중 내부 leaf Agent를 다시 선택하는 책임이 있는 Agent다.
- Leaf Agent는 실제 prompt, response schema, deterministic fallback 계약을 제공하는 실행 단위다.
- 도메인 문맥에서 "서브 에이전트"라고 부르는 파일들도 pipeline 실행 관점에서는 leaf Agent다.

사용 금지:

- `selectedSubAgent`: leaf top-level Agent까지 sub-agent처럼 보이게 하므로 state와 response metadata에서 쓰지 않는다.
- `route_sub_agent_result`: 공통 leaf 검증 edge인데 sub-agent 전용처럼 보이므로 쓰지 않는다.
