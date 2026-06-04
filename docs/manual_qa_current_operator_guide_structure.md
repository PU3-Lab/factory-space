# 현재 operator_guide Manual Q&A 구조

## 한 줄 요약

현재 `operator_guide`는 LangGraph가 선택하는 안내 Agent이고, 내부 leaf agent fallback에서 CSV 기반 `ManualQAService`를 호출해 Manual Q&A 답변을 만든다.

## 전체 실행 흐름

```text
플레이어 질문
-> AgentPipeline / LangGraph
-> Orchestrator가 상위 Agent 선택
-> operator_guide 선택
-> OperatorGuideAgent가 leaf agent 선택
-> 선택된 leaf agent fallback 실행
-> ManualQAService.answer()
-> 질문 유형 분류
-> 5개 CSV 조회
-> 템플릿 답변 생성
-> agent.response 반환
```

## 1. LangGraph / Agent 선택 구조

LangGraph 기준으로는 먼저 상위 Agent를 고른다.

```text
Orchestrator
├─ process_optimizer
├─ operator_guide
├─ quest_generator
└─ new_material_generator
```

플레이어 질문이 장비, 자원, 레시피, 문제 해결과 관련되어 있으면 `operator_guide`가 선택된다.

## 2. operator_guide leaf agent 구조

`operator_guide` 안에는 현재 3개의 leaf agent가 있다.

```text
operator_guide
├─ operator_guide.machine_help
├─ operator_guide.recipe_explainer
└─ operator_guide.troubleshooter
```

각 leaf agent 역할은 다음과 같다.

| leaf agent | 담당 범위 |
|---|---|
| `operator_guide.machine_help` | 장비/설비 설명 질문 |
| `operator_guide.recipe_explainer` | 레시피, 생산 공정, 자원 생산/사용처 질문 |
| `operator_guide.troubleshooter` | 문제 해결, 고장, 막힘, 전력 부족 질문 |

중요한 점은 `resource_explainer` leaf agent가 따로 없다는 것이다.

프로토 단계에서는 자원 질문을 `recipe_explainer`가 함께 담당한다.  
예를 들어 `철괴는 어떻게 만들어?`는 자원 질문이지만 생산 공정과 연결되어 있으므로 `recipe_explainer` 경로로 처리해도 자연스럽다.

## 3. ManualQAService 내부 질문 유형

leaf agent는 3개지만, `ManualQAService` 내부 질문 유형은 5개다.

```text
equipment_question
resource_question
recipe_question
troubleshooting_question
unknown_question
```

즉 구조는 이렇게 나뉜다.

```text
LangGraph leaf agent
= 큰 담당자 선택

ManualQAService question_type
= 실제 매뉴얼 질문 유형 선택
```

예시:

```text
질문: 철괴는 어떻게 만들어?

LangGraph leaf agent:
operator_guide.recipe_explainer

ManualQAService question_type:
resource_question

조회 CSV:
resources.csv
recipes.csv
```

## 4. 파일 구조

현재 파일 구조는 다음과 같다.

```text
backend/src/agents/operator_guide/
├─ agent.py
├─ machine_help.py
├─ recipe_explainer.py
├─ troubleshooter.py
├─ service.py
├─ question_classifier.py
├─ csv_repository.py
├─ response_builder.py
├─ repository.py
├─ schemas.py
└─ __init__.py
```

## 5. 파일별 역할

### `agent.py`

`operator_guide`의 상위 진입점이다.

LangGraph에서 `operator_guide`가 선택된 뒤, 어떤 leaf agent가 맞는지 고르는 routing prompt를 만든다.

### `machine_help.py`

장비/설비 설명 질문을 담당하는 leaf agent다.

현재 fallback에서는 `ManualQAService`를 호출해 CSV 기반 답변을 반환한다.

### `recipe_explainer.py`

레시피, 생산 공정, 자원 생산/사용처 질문을 담당하는 leaf agent다.

자원 질문도 현재는 이 leaf agent 경로에서 처리한다.

### `troubleshooter.py`

문제 해결, 고장, 막힘, 전력 부족 질문을 담당하는 leaf agent다.

### `service.py`

Manual Q&A 처리 흐름을 연결한다.

```text
ManualQAService.answer()
-> question_classifier.py
-> csv_repository.py
-> response_builder.py
```

또한 `build_manual_qa_agent_result()`를 통해 `ManualQAResult`를 LangGraph가 반환할 수 있는 `AgentRunResult`로 바꾼다.

### `question_classifier.py`

질문을 아래 5개 유형 중 하나로 분류한다.

```text
equipment_question
resource_question
recipe_question
troubleshooting_question
unknown_question
```

### `csv_repository.py`

지정된 5개 CSV만 읽는다.

```text
data/game/equipment.csv
data/game/resources.csv
data/game/recipes.csv
data/game/troubleshooting_rules.csv
data/game/action_policy.csv
```

### `response_builder.py`

CSV 조회 결과를 바탕으로 사용자 답변, 출처, 추천 행동을 만든다.

### `schemas.py`

Manual Q&A 응답 구조를 정의한다.

주요 필드:

```text
final_answer
answer
sources
recommended_actions
confidence
question_type
```

### `repository.py`

기존 import 경로를 유지하기 위한 얇은 호환 파일이다.

## 6. 예시 흐름

### 질문: `제련기는 뭐야?`

```text
1. Orchestrator가 operator_guide 선택
2. OperatorGuideAgent가 machine_help 선택
3. machine_help.fallback() 실행
4. ManualQAService.answer("제련기는 뭐야?") 호출
5. question_classifier.py가 equipment_question으로 분류
6. csv_repository.py가 equipment.csv에서 equipment_smelter 조회
7. response_builder.py가 답변 생성
8. final_answer와 metadata 반환
```

### 질문: `철괴는 어떻게 만들어?`

```text
1. Orchestrator가 operator_guide 선택
2. OperatorGuideAgent가 recipe_explainer 선택
3. recipe_explainer.fallback() 실행
4. ManualQAService.answer("철괴는 어떻게 만들어?") 호출
5. question_classifier.py가 resource_question으로 분류
6. csv_repository.py가 resources.csv에서 resource_iron_ingot 조회
7. supporting source로 recipes.csv의 recipe_iron_ingot 조회
8. final_answer와 metadata 반환
```

### 질문: `제련기가 왜 안 돌아가?`

```text
1. Orchestrator가 operator_guide 선택
2. OperatorGuideAgent가 troubleshooter 선택
3. troubleshooter.fallback() 실행
4. ManualQAService.answer("제련기가 왜 안 돌아가?") 호출
5. question_classifier.py가 troubleshooting_question으로 분류
6. csv_repository.py가 troubleshooting_rules.csv의 issue_machine_stopped 조회
7. action_policy.csv에서 추천 행동 조회
8. final_answer와 metadata 반환
```

## 7. 응답 구조

현재 응답은 다음 형태를 가진다.

```json
{
  "final_answer": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다.",
  "text": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다.",
  "answer": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다.",
  "actions": [],
  "metadata": {
    "question_type": "equipment_question",
    "sources": [
      {
        "doc_id": "equipment_smelter",
        "type": "equipment",
        "title": "제련기"
      }
    ],
    "recommended_actions": [
      {
        "action_id": "action_explain_equipment_role",
        "label": "장비 역할 설명",
        "description": "장비가 맡는 역할과 연결되는 자원 및 레시피를 설명한다",
        "priority": 1
      }
    ],
    "selectedAgent": "operator_guide",
    "selectedLeafAgent": "operator_guide.machine_help"
  }
}
```

## 8. 현재 구조의 장점

- 기존 LangGraph / Orchestrator 구조를 바꾸지 않는다.
- `operator_guide` 내부에서만 Manual Q&A 로직을 관리한다.
- 프로토에서는 CSV를 사용하지만, 나중에 PostgreSQL repository로 교체하기 쉽다.
- leaf agent는 3개로 단순하게 유지하면서 내부 question_type은 5개로 세밀하게 관리한다.
- 자원 질문은 현재 `recipe_explainer`에서 처리하지만, 필요하면 나중에 `resource_explainer` leaf agent를 추가할 수 있다.

## 9. 다음 확장 방향

프로토 이후에는 다음처럼 확장할 수 있다.

```text
CSV 직접 조회
-> PostgreSQL Repository
```

```text
템플릿 답변
-> pgvector 검색 + LLM 근거 기반 답변
```

```text
질문만 보고 답변
-> question + player_state를 함께 보고 상황 맞춤 답변
```
