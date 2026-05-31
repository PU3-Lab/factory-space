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

## 5. 오케스트레이터는 몇 개인가?

결정: 서버 전체 오케스트레이터는 1개만 둔다. 다만 `manual_qa/agent.py`와 `quest_generator/agent.py`는 각 도메인 내부에서 서브 에이전트를 조율하므로 도메인 서브 오케스트레이터로 부른다.

현재 서버 전체 오케스트레이터:

- `agents/orchestrator.py`: 전체 요청 의도를 보고 최상위 전문 Agent를 선택한다.

현재 도메인 서브 오케스트레이터:

- `manual_qa/agent.py`: 매뉴얼 Q&A 도메인 내부에서 서브 에이전트를 선택하고 결과를 정규화한다.
- `quest_generator/agent.py`: 퀘스트 생성 도메인 내부에서 서브 에이전트를 선택하고 결과를 정규화한다.

오케스트레이터가 아닌 것:

- `agents/pipeline.py`: LangGraph 실행 흐름을 소유하는 공통 실행 파이프라인이다.
- `agents/router.py`: agent id를 구현체에 매핑하는 registry/router다.
- LangGraph의 `route_top_agent`, `manual_qa.route_sub_agent`, `quest_generator.route_sub_agent` 노드는 Agent 파일이 아니라 오케스트레이터 내부의 실행 단계다.

정리:

- 서버 전체 오케스트레이터: 1개
- 도메인 서브 오케스트레이터: 2개
- 공통 실행 파이프라인: 1개
- LangGraph 라우팅 노드: 실행 단계이며 Agent 개수에 포함하지 않는다.

## 6. `quest_generator.route_sub_agent`는 무슨 역할인가?

결정: 서브 에이전트를 구분하는 주체는 도메인 서브 오케스트레이터인 `agents/quest_generator/agent.py`다. `quest_generator.route_sub_agent`는 별도 Agent가 아니라 `quest_generator` 서브 오케스트레이터 내부의 결정 함수 또는 메서드를 LangGraph 노드 이름으로 표현한 것이다.

역할:

- `route_top_agent`가 최상위 Agent로 `quest_generator`를 선택한 뒤 실행된다.
- 퀘스트를 직접 생성하지 않는다.
- `quest_generator` 서브 오케스트레이터가 요청 payload와 context를 보고 어떤 퀘스트 서브 에이전트가 처리할지 결정한다.
- 결과는 LangGraph state의 `selectedSubAgent`에 기록한다.

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
- 역할 이름은 도메인 서브 오케스트레이터다.
- LangGraph 노드 이름은 `quest_generator.route_sub_agent`처럼 `도메인 Agent.내부 결정 단계` 형식으로 둔다.
- 이 이름은 실행 trace를 읽기 쉽게 하기 위한 것이며, 별도 파일이나 별도 Agent 개수를 의미하지 않는다.

제약:

- WebSocket message를 직접 읽거나 쓰지 않는다.
- LLM 호출을 직접 소유하지 않는다.
- cache key 생성, retry, fallback, response envelope 생성은 `agents/pipeline.py`가 담당한다.
- routing 실패나 판단 불가 상태는 `agent.error`가 아니라 기본 sub-agent 선택 정책으로 복구하는 것을 우선한다.

## 7. 각 Agent 역할은 어디에 기록하는가?

결정: Agent별 세부 책임은 `AGENT_ROLES.md`에 별도 문서로 기록한다.

이유:

- `FOLDER_ROLES.md`는 폴더와 모듈 책임을 설명하는 문서로 유지한다.
- Agent별 입력, 출력, 판단 기준, 금지 책임은 별도 문서가 더 읽기 쉽다.
- 오케스트레이터, 도메인 서브 오케스트레이터, leaf Agent, 실행 구성요소를 한 문서에서 비교할 수 있어야 한다.

기록 대상:

- 서버 전체 오케스트레이터
- 도메인 서브 오케스트레이터
- 공정 최적화 Agent
- 신물질 생성 Agent
- 매뉴얼 Q&A 서브 에이전트
- 퀘스트 생성 서브 에이전트
- Agent가 아닌 실행 구성요소인 `pipeline.py`, `router.py`, `base.py`

## 8. `agents/orchestrator.py`에서 leaf Agent까지 바로 분기해도 되는가?

결정: 기술적으로는 가능하지만 기본 구조로 채택하지 않는다.

채택하지 않는 이유:

- 서버 전체 오케스트레이터가 모든 leaf Agent를 알게 되면 도메인 내부 정책까지 소유하게 된다.
- `manual_qa`와 `quest_generator`는 서브 에이전트 선택 기준이 다르다.
- leaf Agent가 추가될 때마다 `agents/orchestrator.py`를 수정하면 변경 영향 범위가 커진다.
- 도메인별 fallback, payload 정규화, 선택 근거 metadata를 도메인 내부에 묶어두는 편이 유지보수에 유리하다.

현재 원칙:

- `agents/orchestrator.py`는 최상위 Agent만 선택한다.
- `manual_qa/agent.py`는 매뉴얼 Q&A 내부 서브 에이전트를 선택한다.
- `quest_generator/agent.py`는 퀘스트 생성 내부 서브 에이전트를 선택한다.

허용 가능한 예외:

- leaf Agent가 하나뿐인 단순 도메인은 별도 서브 오케스트레이터 없이 `agents/orchestrator.py`에서 바로 선택해도 된다.
- 여러 도메인에 공통으로 적용되는 safety block, 권한 차단, 긴급 fallback은 `agents/orchestrator.py`에서 처리할 수 있다.

## 9. 오케스트레이션 구조 최종 방향

결정: 원래 방향대로 계층형 오케스트레이션 구조를 유지한다.

최종 구조:

- `agents/orchestrator.py`: 서버 전체 오케스트레이터. 최상위 Agent만 선택한다.
- `agents/manual_qa/agent.py`: 매뉴얼 Q&A 도메인 서브 오케스트레이터. 매뉴얼 서브 에이전트를 선택한다.
- `agents/quest_generator/agent.py`: 퀘스트 생성 도메인 서브 오케스트레이터. 퀘스트 서브 에이전트를 선택한다.

채택하지 않는 구조:

- `agents/orchestrator.py`에서 모든 leaf Agent까지 직접 분기하는 중앙 집중형 구조

이유:

- 도메인별 세부 분기 정책을 각 도메인 내부에 유지한다.
- 서버 전체 오케스트레이터의 책임을 최상위 도메인 선택으로 제한한다.
- leaf Agent 추가나 분기 기준 변경이 전체 오케스트레이터로 번지지 않게 한다.
