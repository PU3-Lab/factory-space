# 프로젝트 Agent 전체 구조 안내서

이 문서는 포트폴리오나 면접에서 프로젝트의 전체 Agent 구조를 먼저 설명한 뒤, 내가 담당한 `operator_guide` Agent로 자연스럽게 이어가기 위한 요약 문서다.

## 핵심 결론

전체 설계상 Agent는 4개다.

```text
material_generation
operator_guide
quest_generator
process_optimizer
```

현재 시연 가능하거나 구현 중심으로 설명할 수 있는 Agent는 다음 3개다.

```text
material_generation
operator_guide
quest_generator
```

`process_optimizer`는 설계와 routing 후보에는 포함되어 있지만, 아직 구현 완료된 Agent는 아니다. 따라서 발표에서는 **향후 확장 예정인 최적화 Agent**로 설명한다.

이렇게 말하면 된다.

```text
전체 구조는 4개의 Agent를 염두에 두고 설계했습니다.
소재 생성을 담당하는 material_generation, 공장 운영 질문을 답변하는 operator_guide,
퀘스트 생성을 담당하는 quest_generator, 그리고 향후 공정 병목과 생산 효율을 분석할 process_optimizer입니다.

현재 시연 가능한 구현은 material_generation, operator_guide, quest_generator이고,
process_optimizer는 catalog와 routing 후보에는 포함되어 있지만 확장 예정 영역입니다.
제가 담당한 부분은 operator_guide로, RAG와 현재 게임 상태를 이용해 플레이어 질문에 답변하는 Agent입니다.
```

## 발표 흐름

추천 발표 흐름은 다음과 같다.

```text
1. 프로젝트는 여러 Agent가 역할을 나눠 처리하는 구조다.
2. Orchestrator가 사용자 요청을 보고 적절한 Agent를 고른다.
3. 설계상 Agent는 material_generation, operator_guide, quest_generator, process_optimizer 4개다.
4. 현재 시연 가능 범위와 확장 예정 범위를 구분한다.
5. 그중 내가 담당한 operator_guide를 자세히 설명한다.
```

## 전체 Agent 후보

코드 기준 top-level Agent 후보는 `backend/src/agents/agent_catalog.py`에 정의되어 있다.

```text
process_optimizer
operator_guide
quest_generator
material_generation
```

관련 파일:

```text
backend/src/agents/agent_catalog.py
backend/src/agents/orchestrator.py
backend/src/agents/router.py
backend/src/agents/pipeline/runtime.py
```

## 전체 구조 한눈에 보기

```mermaid
flowchart TD
    A[Unreal / Web UI / agent-test] --> B[/ws/agent]
    B --> C[AgentPipeline]
    C --> D[Top-level Orchestrator]
    D --> E{selectedAgent}

    E -->|material_generation| F[Material Generation Agent]
    E -->|operator_guide| G[Operator Guide Agent]
    E -->|quest_generator| H[Quest Generator Agent]
    E -->|process_optimizer| I[Process Optimizer]

    F --> F1[전용 graph.py / nodes.py]
    G --> G1[공통 pipeline + leaf agent + RAG service]
    H --> H1[공통 pipeline + quest leaf agent + quest service]
    I --> I1[확장 예정: 공정 병목 / 생산 효율 분석]
```

## Agent별 역할 비교

| Agent | 역할 | 대표 질문/요청 | 현재 상태 | 구조 |
| --- | --- | --- | --- | --- |
| `material_generation` | 새로운 소재 후보 생성 | "이 조건에 맞는 신규 소재를 만들어줘" | 구현/시연 가능 | 전용 `graph.py`, `nodes.py`, `graph_state.py` |
| `operator_guide` | 장비, 자원, 레시피, 문제 해결 안내 | "분쇄기가 뭐야?", "철괴가 안 만들어져" | 구현/시연 가능, 담당 영역 | 공통 `AgentPipeline` + leaf agent + RAG |
| `quest_generator` | 튜토리얼/생산/경제 퀘스트 생성 | "초반 퀘스트를 만들어줘" | 구현/시연 가능 | 공통 `AgentPipeline` + quest leaf agent + quest service |
| `process_optimizer` | 공장 병목/공정 개선 제안 | "공정 병목을 찾아줘" | 미구현/확장 예정 | catalog와 routing 후보에 포함 |

## 공통 AgentPipeline

`operator_guide`와 `quest_generator`는 공통 AgentPipeline 위에서 실행된다.

주요 파일:

```text
backend/src/agents/pipeline/runtime.py
backend/src/agents/pipeline/graph_edges.py
backend/src/agents/pipeline/state.py
backend/src/agents/pipeline/middleware.py
backend/src/agents/pipeline/llm_fallback.py
backend/src/agents/pipeline/tool_node.py
```

공통 pipeline이 담당하는 일:

```text
- 요청 envelope 검증
- top-level agent routing
- leaf agent routing
- cache lookup / cache write
- middleware before / after
- LLM default / fallback 호출
- LLM 응답 파싱
- response schema validation
- 최종 agent.response 생성
```

공통 흐름:

```text
build_context
-> validate_envelope
-> route_top_agent
-> route_sub_agent
-> cache_lookup
-> middleware_before
-> build_prompt
-> call_llm.default
-> parse_llm_response
-> validate_response_schema
-> cache_write
-> middleware_after
-> build_agent_response
```

## material_generation 구조

`material_generation`은 전용 그래프형 Agent다.

파일:

```text
backend/src/agents/material_generation/graph.py
backend/src/agents/material_generation/nodes.py
backend/src/agents/material_generation/graph_state.py
backend/src/agents/material_generation/routing.py
backend/src/agents/material_generation/agent.py
```

특징:

```text
- 자기 폴더 안에 독립 StateGraph를 가진다.
- normalize, cache, recipe_match, prevalidate, classify, llm_propose, validate_result 같은 node가 있다.
- 소재 생성은 단계가 많고 전용 분기 흐름이 필요하기 때문에 독립 graph.py가 있다.
```

간단 흐름:

```text
입력 정규화
-> 캐시 확인
-> 기존 레시피 매칭
-> 사전 검증
-> 소재 유형 분류
-> 규칙 처리
-> 유사도 컨텍스트 확인
-> LLM 후보 생성
-> 결과 검증
-> 중복 제거
-> 소재 등록
```

발표용 한 문장:

```text
material_generation은 신규 소재를 생성하는 과정이 여러 검증 단계로 나뉘기 때문에 전용 LangGraph를 가진 Agent입니다.
```

## operator_guide 구조

`operator_guide`는 공통 AgentPipeline을 재사용하는 RAG 기반 안내 Agent다.

파일:

```text
backend/src/agents/operator_guide/agent.py
backend/src/agents/operator_guide/machine_help.py
backend/src/agents/operator_guide/recipe_explainer.py
backend/src/agents/operator_guide/troubleshooter.py
backend/src/agents/operator_guide/service.py
backend/src/agents/operator_guide/prompt_builder.py
backend/src/agents/operator_guide/system_prompt.py
backend/src/agents/operator_guide/rag_retriever.py
backend/src/agents/operator_guide/rag_store.py
backend/src/agents/operator_guide/session_memory.py
```

특징:

```text
- 별도 graph.py, nodes.py가 없다.
- 공통 pipeline의 routing, cache, middleware, fallback, validation을 사용한다.
- operator_guide 고유 로직은 agent.py, leaf agent, service.py, RAG/prompt 모듈에 있다.
```

leaf agent:

```text
operator_guide.machine_help
- 장비 설명
- 예: "분쇄기가 뭐야?"

operator_guide.recipe_explainer
- 제작법, 레시피 설명
- 예: "철괴는 어떻게 만들어?"

operator_guide.troubleshooter
- 고장 원인, 현재 상태 기반 문제 해결
- 예: "철괴가 안 만들어져. 왜 그래?"
```

operator_guide 처리 흐름:

```text
플레이어 질문
-> operator_guide 선택
-> leaf agent 선택
-> 질문 유형 분류
-> 현재 상태 필요 여부 판단
-> CSV evidence 구성
-> PostgreSQL + pgvector RAG 검색
-> current_game_state 필요 scope 추출
-> prompt 구성
-> LLM 답변 생성
-> answer sanitizer 후처리
-> JSON 응답 반환
```

발표용 한 문장:

```text
operator_guide는 공장 운영 질문을 CSV 기반 매뉴얼과 pgvector RAG 검색, 필요한 경우 현재 게임 상태까지 함께 사용해 NPC 말투로 답변하는 Agent입니다.
```

## quest_generator 구조

코드 폴더명은 `quest_generator`다.

포트폴리오에서는 팀에서 부르는 이름에 맞춰 `quest_agent(quest_generator)`처럼 병기해서 설명해도 된다.

파일:

```text
backend/src/agents/quest_generator/agent.py
backend/src/agents/quest_generator/service.py
backend/src/agents/quest_generator/production_quest.py
backend/src/agents/quest_generator/economy_quest.py
backend/src/agents/quest_generator/schemas.py
backend/src/agents/quest_generator/tools.py
```

특징:

```text
- operator_guide처럼 공통 AgentPipeline을 사용한다.
- 내부에서 production_quest, economy_quest 같은 leaf agent를 고른다.
- QuestAgentService가 준비된 quest 후보를 선택하고 JSON 응답 형태로 만든다.
```

leaf agent:

```text
quest_generator.production_quest
- 생산 목표형 퀘스트

quest_generator.economy_quest
- 경제/보상/진행 관련 퀘스트
```

간단 흐름:

```text
퀘스트 요청
-> quest_generator 선택
-> production_quest / economy_quest 선택
-> QuestAgentService
-> 퀘스트 후보 선택
-> QuestResponse JSON 반환
```

발표용 한 문장:

```text
quest_generator는 튜토리얼이나 생산 목표에 맞는 퀘스트를 생성해 게임 진행을 유도하는 Agent입니다.
```

## process_optimizer 구조

`process_optimizer`는 설계상 4번째 Agent다.

현재 상태:

```text
- top-level agent catalog에는 포함되어 있다.
- 공정 병목, 장비 가동률, 생산 효율 개선 제안을 담당할 예정인 Agent다.
- 현재 포트폴리오 시연 범위에서는 구현 완료 Agent로 설명하지 않는다.
```

관련 파일:

```text
backend/src/agents/process_optimizer.py
backend/src/agents/agent_catalog.py
```

예상 역할:

```text
- 생산 라인의 병목 지점 찾기
- 장비별 가동률 분석
- 자원 투입/출력 흐름 비교
- 우선 개선할 공정 추천
```

예상 질문:

```text
"지금 공장에서 어디가 병목이야?"
"생산 효율을 높이려면 뭘 먼저 바꿔야 해?"
"철괴 생산량을 늘리려면 어떤 장비를 추가해야 해?"
```

발표용 한 문장:

```text
process_optimizer는 아직 구현 완료 범위는 아니지만, 전체 설계상 생산 라인의 병목과 효율 개선을 담당할 확장 예정 Agent입니다.
```

## 왜 Agent마다 구조가 다른가?

세부 구조가 다른 이유는 각 Agent의 책임과 복잡도가 다르기 때문이다.

```text
material_generation
- 후보 생성과 검증 단계가 복잡하다.
- 전용 graph.py / nodes.py가 적합하다.

operator_guide
- 공통 pipeline 흐름은 그대로 쓰고, RAG와 prompt 구성에 집중한다.
- 별도 graph.py보다 service/RAG/prompt 분리가 적합하다.

quest_generator
- 현재는 준비된 quest 후보를 선택하고 JSON으로 반환하는 구조다.
- 공통 pipeline + service 구조가 충분하다.

process_optimizer
- 아직 확장 예정이다.
- 향후 상태 분석과 병목 진단 흐름이 복잡해지면 별도 graph를 가질 수도 있다.
```

즉, 모든 Agent가 반드시 `graph.py`, `nodes.py`를 가져야 하는 것은 아니다.

## operator_guide로 이어지는 설명

전체 구조를 설명한 뒤에는 이렇게 넘어가면 자연스럽다.

```text
이 중 제가 담당한 부분은 operator_guide입니다.
operator_guide는 단순 FAQ가 아니라, 플레이어가 공장 운영 중 막혔을 때 장비/레시피/문제 해결 매뉴얼을 찾아 답변하는 RAG 기반 안내 Agent입니다.

특히 단순 설명 질문은 RAG만 사용하고,
"철괴가 안 만들어져" 같은 문제 해결 질문은 Unreal이 보내준 current_game_state를 함께 사용해 원인을 좁힙니다.
```

그 다음 설명할 문서:

```text
docs/operator_guide/operator_guide_overall_structure.md
docs/operator_guide/operator_guide_graph_structure.md
docs/operator_guide/2026-06-16_operator_guide_system_guide.md
docs/operator_guide/2026-06-16_operator_guide_final_demo_portfolio_guide.md
```

## 발표용 30초 요약

```text
이 프로젝트는 여러 Agent가 역할을 나눠 처리하는 구조입니다.
먼저 Orchestrator가 사용자 요청을 보고 material_generation, operator_guide, quest_generator, process_optimizer 중 하나를 선택합니다.

material_generation은 신규 소재 생성처럼 단계가 복잡해서 전용 graph.py와 nodes.py를 가진 독립 그래프형 Agent입니다.
quest_generator는 퀘스트 후보를 선택하고 JSON으로 반환하는 공통 pipeline 기반 Agent입니다.
process_optimizer는 향후 공정 병목과 생산 효율을 분석할 확장 예정 Agent입니다.

제가 담당한 operator_guide는 공통 AgentPipeline 위에서 동작하면서,
장비 설명, 레시피 설명, 문제 해결 질문을 leaf agent로 나누고,
CSV 기반 매뉴얼과 PostgreSQL + pgvector RAG 검색, 필요한 경우 현재 게임 상태를 함께 사용해 플레이어에게 NPC 톤으로 답변합니다.
```

## 발표용 비교표

| 항목 | material_generation | operator_guide | quest_generator | process_optimizer |
| --- | --- | --- | --- | --- |
| 주요 목적 | 신규 소재 생성 | 공장 운영 안내 | 퀘스트 생성 | 공정 최적화 |
| 현재 상태 | 구현/시연 가능 | 구현/시연 가능 | 구현/시연 가능 | 미구현/확장 예정 |
| 구조 | 전용 LangGraph | 공통 AgentPipeline | 공통 AgentPipeline | 향후 결정 |
| graph.py | 있음 | 없음 | 없음 | 없음 |
| nodes.py | 있음 | 없음 | 없음 | 없음 |
| leaf agent | 전용 node 중심 | machine/recipe/troubleshooter | production/economy | 미정 |
| 외부 지식 | 레시피/소재 규칙 | CSV + pgvector RAG | quest 후보 데이터 | 향후 생산 상태/공정 지표 |
| LLM 사용 | 후보 생성/검증 | routing/답변 생성 | routing/응답 생성 | 향후 병목 판단/개선 제안 |
| 발표 포인트 | 생성 파이프라인 | RAG, 현재 상태, guardrail | 퀘스트 JSON 생성 | 확장 로드맵 |

## 주의해서 설명할 점

```text
- operator_guide에 graph.py가 없다고 구현이 빠진 것이 아니다.
- 공통 AgentPipeline을 재사용하는 설계다.
- material_generation은 전용 그래프가 더 적합해서 graph.py가 있다.
- process_optimizer는 catalog에는 있지만 현재 구현 완료 범위가 아니다.
- 미구현 영역은 숨기지 말고 확장 예정 로드맵으로 설명한다.
```
