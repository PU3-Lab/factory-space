# Backend 결정 로그

이 문서는 backend 구조를 정하는 과정에서 나온 질문과 답변을 결정 사항으로 기록한다.

## 1. `agents/pipeline.py`는 오케스트레이터 에이전트인가?

결정: `agents/pipeline.py`는 오케스트레이터 에이전트가 아니라 공통 실행 파이프라인이다.

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
- `agents/pipeline.py`: 공통 실행 흐름을 제어한다.
- `agents/process_optimizer.py`: 공정 최적화 Agent다.
- `agents/quest_generator/agent.py`: 퀘스트 생성 상위 Agent다.
- `agents/manual_qa/agent.py`: 매뉴얼 Q&A 상위 Agent다.
- `agents/new_material_generator.py`: 신물질 생성 Agent다.

후속 검토:

- `pipeline.py`라는 이름이 헷갈리면 `agent_executor.py`로 변경하는 편이 더 명확하다.

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

- `agents/pipeline.py`는 실행 흐름을 제어하는 인프라 계층이다.
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
- 상위 Agent는 서브 에이전트 선택과 공통 응답 정규화만 맡고, 세부 생성 정책은 서브 에이전트가 맡는다.

현재 구조:

- `agents/manual_qa/agent.py`: 매뉴얼 Q&A 상위 Agent다.
- `agents/manual_qa/recipe_explainer.py`: 레시피 설명 서브 에이전트다.
- `agents/manual_qa/machine_help.py`: 장비 도움말 서브 에이전트다.
- `agents/manual_qa/troubleshooter.py`: 문제 해결 서브 에이전트다.
- `agents/quest_generator/agent.py`: 퀘스트 생성 상위 Agent다.
- `agents/quest_generator/tutorial_quest.py`: 튜토리얼 퀘스트 서브 에이전트다.
- `agents/quest_generator/production_quest.py`: 생산 퀘스트 서브 에이전트다.
- `agents/quest_generator/exploration_quest.py`: 탐험 퀘스트 서브 에이전트다.
- `agents/quest_generator/economy_quest.py`: 경제 퀘스트 서브 에이전트다.

책임 경계:

- 전체 서버 오케스트레이터인 `agents/orchestrator.py`는 어떤 최상위 Agent를 쓸지 선택한다.
- `manual_qa/agent.py`는 매뉴얼 Q&A 내부에서 어떤 서브 에이전트를 쓸지 선택한다.
- `quest_generator/agent.py`는 퀘스트 생성 내부에서 어떤 서브 에이전트를 쓸지 선택한다.
- `agents/pipeline.py`는 여전히 공통 실행 흐름만 담당하고 서브 에이전트 정책을 소유하지 않는다.

서브 에이전트 목록:

- `manual_qa.recipe_explainer`
- `manual_qa.machine_help`
- `manual_qa.troubleshooter`
- `quest_generator.tutorial_quest`
- `quest_generator.production_quest`
- `quest_generator.exploration_quest`
- `quest_generator.economy_quest`

## 4. LangGraph는 어디에 적용하는가?

결정: LangGraph는 WebSocket transport가 아니라 `agents/pipeline.py`의 Agent 실행 흐름에 적용한다.

구조:

- WebSocket gateway는 raw message 수신과 response 송신만 담당한다.
- `agents/pipeline.py`가 검증된 `agent.request`를 LangGraph input으로 넣는다.
- LangGraph state는 envelope, context, selectedAgent, selectedSubAgent, typedPayload, cacheKey, prompt, llmRaw, responsePayload, streams, error를 가진다.
- 최종 graph output은 `agent.response` 또는 `agent.error` envelope다.

라우팅:

- `route_top_agent`는 최상위 Agent를 선택한다.
- `manual_qa.route_sub_agent`는 `recipe_explainer`, `machine_help`, `troubleshooter` 중 하나를 선택한다.
- `quest_generator.route_sub_agent`는 `tutorial_quest`, `production_quest`, `exploration_quest`, `economy_quest` 중 하나를 선택한다.

제약:

- WebSocket connection object는 LangGraph state에 넣지 않는다.
- streaming event는 graph output의 `streams` 목록으로 반환하고, 실제 송신은 WebSocket gateway가 담당한다.
- LLM 실패는 graph 안에서 fallback node로 복구한다.
- fallback schema 검증 실패나 validation error만 `agent.error`로 종료한다.
