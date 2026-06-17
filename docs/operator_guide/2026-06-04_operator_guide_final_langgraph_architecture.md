# 최종 operator_guide Agent LangGraph 구조

## 한 줄 소개

최종 `operator_guide` Agent는 플레이어 질문을 먼저 이해한 뒤, 필요한 경우에만 현재 게임 상태를 tool로 조회하고, LangGraph의 router와 tools를 통해 장비, 자원, 레시피, 문제 해결 매뉴얼을 검색해 근거 있는 답변과 추천 행동을 반환하는 AI 안내 에이전트다.

## 전체 흐름

```text
User Question
        |
        v
[LangGraph: Top-level Orchestrator Router]
        |
        |-- process_optimizer
        |-- operator_guide
        |-- quest_agent
        |-- new_material_agent
        |
        v
[operator_guide Agent]
        |
        v
[Input Normalization Middleware]
        |
        v
[Question Type Router]
        |
        |-- equipment_question
        |-- resource_question
        |-- recipe_question
        |-- troubleshooting_question
        |-- unknown_question
        |
        v
[State Requirement Router]
        |
        |-- no_state_needed
        |-- selected_machine_state_needed
        |-- inventory_state_needed
        |-- production_line_state_needed
        |
        v
[State Tool Calls only when needed]
        |
        |-- get_selected_machine_state
        |-- get_inventory_state
        |-- get_power_state
        |-- get_production_line_state
        |
        v
[Retrieval Filter Middleware]
        |
        v
[Tool Calls]
        |
        |-- equipment_lookup_tool
        |-- resource_lookup_tool
        |-- recipe_lookup_tool
        |-- troubleshooting_lookup_tool
        |-- action_policy_tool
        |-- player_state_analyzer_tool
        |-- manual_rag_search_tool
        |
        v
[LLM Answer Generator]
        |
        v
[Response Validation Middleware]
        |
        v
[Response Builder]
        |
        v
Final Answer JSON
```

## 1. Top-level Orchestrator Router

Top-level Orchestrator Router는 플레이어 질문을 보고 4개 Agent 중 어느 Agent가 처리할지 선택한다.

```text
질문: 제련기가 왜 안 돌아가?
판단: 장비 문제 해결 질문
선택: operator_guide
```

Orchestrator는 직접 답변하지 않는다.  
역할은 질문을 가장 알맞은 Agent로 보내는 것이다.

## 2. operator_guide Agent

`operator_guide`는 장비, 자원, 레시피, 문제 해결 질문을 처리하는 Manual Q&A Agent다.

최종 단계에서는 CSV를 직접 읽는 대신 아래 지식 기반을 사용한다.

```text
PostgreSQL 구조화 데이터
pgvector 기반 Markdown 매뉴얼 검색
필요한 경우에만 player_state 조회와 분석
LLM 근거 기반 답변 생성
```

## 3. Middleware

프로토에서는 미들웨어가 필수는 아니다.  
최종 단계에서는 요청과 응답을 안정적으로 만들기 위해 미들웨어를 사용할 수 있다.

### Input Normalization Middleware

사용자 질문과 요청 payload를 일정한 형태로 정리한다.

예:

```text
제련기 왜 안돌아감?
-> 제련기가 왜 안 돌아가?
```

```text
selectedMachineId
-> selected_machine_id
```

### Retrieval Filter Middleware

질문 유형과 필요한 경우 조회한 상태를 기준으로 tool 검색 범위를 제한한다.

예:

```text
question_type = troubleshooting_question
state_requirement = selected_machine_state_needed
selected_equipment_id = equipment_smelter
```

검색 필터:

```text
manual_type in ["troubleshooting", "equipment"]
equipment_id = "equipment_smelter"
```

### Response Validation Middleware

LLM이 만든 답변이 응답 계약을 지키는지 확인한다.

확인 항목:

```text
final_answer가 비어 있지 않은가?
sources가 포함되어 있는가?
recommended_actions가 action_policy에 존재하는가?
payload.actions가 안전하게 빈 배열인가?
근거 없는 장비, 자원, 레시피를 지어내지 않았는가?
```

## 4. Question Type Router

`operator_guide` 내부 router는 질문을 아래 5개 유형 중 하나로 분류한다.

```text
equipment_question
resource_question
recipe_question
troubleshooting_question
unknown_question
```

예:

```text
제련기는 뭐야?
-> equipment_question

철괴는 어떻게 만들어?
-> resource_question

기어 만들려면 뭐가 필요해?
-> recipe_question

제련기가 왜 안 돌아가?
-> troubleshooting_question

우주 엘리베이터는 어떻게 업그레이드해?
-> unknown_question
```

이 router는 어떤 tool을 우선 호출할지 결정하는 기준이 된다.

## 5. State Requirement Router

State Requirement Router는 질문에 답하기 위해 현재 게임 상태가 필요한지 판단한다.

핵심 원칙:

```text
모든 질문에서 player_state를 확인하지 않는다.
질문 유형을 먼저 판단한 뒤, 필요한 상태만 tool로 조회한다.
```

예:

```text
제련기는 뭐야?
-> equipment_question
-> no_state_needed

철괴는 어떻게 만들어?
-> resource_question
-> no_state_needed

기어 만들려면 뭐가 필요해?
-> recipe_question
-> no_state_needed

제련기가 왜 안 돌아가?
-> troubleshooting_question
-> selected_machine_state_needed

철괴가 왜 부족해?
-> troubleshooting_question
-> inventory_state_needed

기어 생산이 왜 느려?
-> troubleshooting_question
-> production_line_state_needed
```

이 구조를 쓰면 설명형 질문에는 불필요한 상태 조회를 하지 않고, 문제 해결 질문에서만 필요한 상태를 확인할 수 있다.

## 6. State Tools

State Tools는 State Requirement Router가 필요하다고 판단한 경우에만 호출한다.

### get_selected_machine_state

현재 플레이어가 선택한 장비의 상태를 조회한다.

예:

```text
selected_machine_id = machine_smelter_01
selected_equipment_id = equipment_smelter
power_status = low
input_inventory.resource_iron_ore = 0
output_inventory_status = not_full
recipe_id = recipe_iron_ingot
```

### get_inventory_state

특정 자원이나 전체 재고 상태를 조회한다.

예:

```text
resource_iron_ingot = 3
resource_iron_ore = 0
resource_gear = 1
```

### get_power_state

전력 생산량, 소비량, 부족 여부를 조회한다.

예:

```text
power_status = low
power_production = 50
power_consumption = 65
```

### get_production_line_state

특정 생산 라인의 병목, 입력, 출력, 장비 상태를 조회한다.

예:

```text
line_id = gear_line_01
bottleneck = resource_iron_ingot
slow_machine = machine_assembler_02
```

## 7. Knowledge Tools

Knowledge Tools는 PostgreSQL과 pgvector에서 매뉴얼 지식을 조회하거나 검색하는 역할을 한다.

### equipment_lookup_tool

PostgreSQL에서 장비 정보를 조회한다.

예:

```text
equipment_smelter
-> 이름: 제련기
-> 역할: 광석을 금속 자원으로 변환
-> 입력 자원: 철광석, 구리광석
-> 출력 자원: 철괴, 구리괴
```

### resource_lookup_tool

PostgreSQL에서 자원 정보를 조회한다.

예:

```text
resource_iron_ingot
-> 이름: 철괴
-> 생산 장비: 제련기
-> 사용처: 기어 제작, 기본 부품 제작
```

### recipe_lookup_tool

PostgreSQL에서 레시피와 생산 공정 정보를 조회한다.

예:

```text
recipe_gear
-> 입력: 철괴 2개
-> 출력: 기어 1개
-> 필요 장비: 조립기
```

### troubleshooting_lookup_tool

PostgreSQL에서 문제 해결 규칙을 조회한다.

예:

```text
issue_machine_stopped
-> 가능한 원인: 전력 부족, 입력 자원 부족, 출력 공간 부족
-> 확인 순서: 전력, 입력 자원, 출력 공간, 컨베이어, 레시피
```

### action_policy_tool

추천 행동 ID를 사용자에게 보여줄 행동 정보로 변환한다.

예:

```text
action_check_power
-> 전력 상태 확인
```

### player_state_analyzer_tool

State Tools로 가져온 현재 게임 상태를 분석한다. 상태가 필요 없는 질문에서는 호출하지 않는다.

예:

```text
선택 장비: 제련기
전력 상태: low
입력 철광석: 0
출력 공간: not_full
설정 레시피: recipe_iron_ingot
```

분석 결과:

```text
입력 자원 부족 가능성이 높음
전력 부족도 함께 확인 필요
출력 공간 문제 가능성은 낮음
```

### manual_rag_search_tool

질문을 embedding으로 변환하고, pgvector에서 의미가 가까운 Markdown 매뉴얼 chunk를 검색한다.

예:

```text
질문: 제련기가 왜 안 돌아가?
검색 결과:
- 입력 자원 부족 매뉴얼
- 전력 부족 매뉴얼
- 장비가 멈췄을 때 매뉴얼
- 제련기 매뉴얼
```

## 8. LLM Answer Generator

LLM은 tool 결과와 검색된 문서를 근거로 최종 답변을 만든다.

중요한 규칙:

```text
검색된 문서와 필요한 경우 조회한 player_state에 근거해서만 답변한다.
근거가 없으면 모른다고 말한다.
실제 Unreal 실행 action을 만들지 않는다.
추천 행동은 metadata.recommended_actions에만 넣는다.
```

## 9. Response Builder

Response Builder는 LLM 답변과 tool 결과를 최종 JSON으로 정리한다.

반환 필드:

```text
final_answer
text
actions
metadata.question_type
metadata.diagnosis
metadata.sources
metadata.recommended_actions
metadata.retrieval
```

## 최종 시나리오 예시

플레이어 질문:

```text
제련기가 왜 안 돌아가?
```

처리 흐름:

```text
1. Orchestrator Router
   -> operator_guide 선택

2. Question Type Router
   -> troubleshooting_question 선택

3. State Requirement Router
   -> selected_machine_state_needed

4. get_selected_machine_state
   -> 선택 장비: 제련기
   -> 전력 상태: low
   -> 철광석 입력: 0
   -> 출력 공간: not_full

5. player_state_analyzer_tool
   -> 입력 자원 부족 가능성이 높음
   -> 전력 부족도 함께 확인 필요
   -> 출력 공간 문제 가능성은 낮음

6. troubleshooting_lookup_tool
   -> issue_machine_stopped 조회
   -> issue_no_input 조회
   -> issue_no_power 조회

7. equipment_lookup_tool
   -> equipment_smelter 조회

8. manual_rag_search_tool
   -> 입력 자원 부족 문서 검색
   -> 전력 부족 문서 검색
   -> 제련기 매뉴얼 검색

9. action_policy_tool
   -> action_check_input_resource 조회
   -> action_check_power 조회

10. LLM Answer Generator
   -> 필요한 상태와 검색 근거를 바탕으로 답변 생성

11. Response Builder
   -> final_answer, sources, recommended_actions 반환
```

최종 답변 예시:

```text
현재 제련기는 입력 철광석이 없고 전력 상태도 낮아서 멈춘 가능성이 큽니다.
먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.
출력 저장 공간은 현재 막힌 것으로 보이지 않습니다.
```

## 프로토와 최종의 차이

| 구분 | 프로토 | 최종 |
|---|---|---|
| Agent 선택 | Orchestrator가 operator_guide 선택 | 동일 |
| 내부 분류 | leaf agent + 질문 유형 분류 | LangGraph router 기반 분류 |
| 데이터 | 5개 CSV 직접 조회 | PostgreSQL 구조화 데이터 |
| 문서 검색 | 없음 | pgvector 기반 Markdown RAG |
| Embedding | 없음 | 질문과 문서 chunk embedding |
| player_state | 거의 사용하지 않음 | 질문에 필요한 경우만 tool로 조회 |
| 답변 생성 | 템플릿 | LLM 근거 기반 생성 |
| 추천 행동 | CSV action_policy 기반 | action_policy + 상황 기반 우선순위 |
| 실제 action | 없음 | 안전 검증 전까지 없음 |

## 최종 성공 기준

```text
플레이어가 질문했을 때 현재 상황을 읽고, 장비, 자원, 레시피, 문제 해결 매뉴얼을 근거로 정확한 답변과 추천 행동을 반환한다.
```
