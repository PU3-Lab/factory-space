# Manual Q&A Agent 처리 흐름 설명

Manual Q&A Agent는 PostgreSQL과 pgvector 기반 매뉴얼 검색을 이용해 플레이어의 공장 운영 질문에 근거 있는 답변과 추천 행동을 제공하는 안내 에이전트다.

이 문서는 Manual Q&A Agent가 Unreal UI / Front에서 질문을 받았을 때 내부에서 어떤 순서로 처리되는지 쉽게 설명한다.

## 전체 흐름

```text
Unreal UI / Front
-> agent="operator_guide"으로 질문 전송
-> MessageRouter
-> AgentOrchestrator
-> AgentRegistry에서 operator_guide 찾기
-> QAChatbotAgent.process()
-> ManualQAService
-> ManualQAQuestionClassifier
-> CsvManualQARepository
-> ManualQAResponseBuilder
-> AgentResponsePayload 생성
-> MessageRouter를 통해 Unreal UI / Front로 응답 반환
```

중요한 점은 Unreal UI / Front가 직접 `service.py`를 호출하지 않는다는 것이다. 먼저 기존 WebSocket 메시지 처리 구조를 지나고, 그 다음 `QAChatbotAgent.process()` 안에서 Manual Q&A 내부 로직이 실행된다.

## Input / Output 정리

Manual Q&A Agent의 가장 기본 입력은 플레이어 질문이다. 출력은 Unreal UI / Front가 보여줄 답변과, UI에서 참고할 수 있는 metadata다.

### Agent Input

Unreal UI / Front에서 들어오는 요청은 기존 WebSocket message protocol을 따른다.

```json
{
  "type": "agent_request",
  "version": "1.0",
  "request_id": "req-manual-qa-001",
  "session_id": "session-001",
  "client_id": "unreal-client-01",
  "agent": "operator_guide",
  "payload": {
    "question": "제련기가 왜 안 돌아가?",
    "context": {
      "selected_object_id": "equipment_smelter_01"
    }
  }
}
```

핵심 field는 다음과 같다.

- `agent`: 호출할 agent ID다. Manual Q&A는 기존 `operator_guide` agent를 사용한다.
- `payload.question`: 플레이어가 입력한 질문이다.
- `payload.context`: 선택된 오브젝트나 화면 상태 같은 추가 정보다. 현재 프로토에서는 거의 사용하지 않지만, 최종 구조에서는 player context 판단에 사용할 수 있다.

현재 프로토 내부에서는 이 입력을 바탕으로 5개 CSV를 조회한다.

```text
question
-> equipment.csv
-> resources.csv
-> recipes.csv
-> troubleshooting_rules.csv
-> action_policy.csv
```

최종 구조에서는 같은 질문에 PostgreSQL 조회 결과와 pgvector 검색 결과가 함께 붙는다.

```json
{
  "question": "제련기가 왜 안 돌아가?",
  "context": {
    "selected_object_id": "equipment_smelter_01"
  },
  "structured_results": {
    "equipment": {
      "equipment_id": "equipment_smelter",
      "name": "제련기",
      "role": "광석을 금속 자원으로 변환하는 생산 장비"
    },
    "troubleshooting_rules": [
      {
        "issue_id": "issue_machine_stopped",
        "check_order": ["check_power", "check_input", "check_output"]
      }
    ]
  },
  "retrieved_chunks": [
    {
      "doc_id": "issue_machine_stopped",
      "title": "장비가 멈췄을 때",
      "text": "장비가 멈췄다면 전력, 입력 자원, 출력 공간을 순서대로 확인한다."
    }
  ]
}
```

### Agent Output

Manual Q&A Agent는 기존 `agent_response` 구조로 응답한다.

```json
{
  "type": "agent_response",
  "version": "1.0",
  "request_id": "req-manual-qa-001",
  "session_id": "session-001",
  "client_id": "unreal-client-01",
  "agent": "operator_guide",
  "payload": {
    "text": "제련기가 멈췄다면 먼저 전력 상태를 확인하세요. 그 다음 입력 자원과 출력 저장 공간을 순서대로 확인하는 것이 좋습니다.",
    "actions": [],
    "metadata": {
      "question": "제련기가 왜 안 돌아가?",
      "question_type": "troubleshooting_question",
      "sources": [
        {
          "doc_id": "issue_machine_stopped",
          "type": "troubleshooting",
          "title": "장비가 멈췄을 때"
        },
        {
          "doc_id": "equipment_smelter",
          "type": "equipment",
          "title": "제련기"
        }
      ],
      "recommended_actions": [
        {
          "action_id": "action_check_power",
          "label": "전력 상태 확인",
          "description": "발전기와 전력 연결 상태를 확인한다",
          "priority": 1
        },
        {
          "action_id": "action_check_input_resource",
          "label": "입력 자원 확인",
          "description": "장비에 필요한 입력 자원이 공급되는지 확인한다",
          "priority": 2
        }
      ],
      "confidence": "medium"
    }
  }
}
```

핵심 field는 다음과 같다.

- `payload.text`: Unreal UI / Front에 보여줄 자연어 답변이다.
- `payload.actions`: Unreal이 실제 실행할 action 목록이다. 현재 프로토에서는 기본적으로 `[]`다.
- `payload.metadata.question_type`: 질문 분류 결과다.
- `payload.metadata.sources`: 답변의 근거가 된 장비, 자원, 레시피, 문제 해결 문서다.
- `payload.metadata.recommended_actions`: UI에 보여줄 추천 확인 행동이다.
- `payload.metadata.confidence`: 답변 신뢰도다.

정리하면 다음과 같다.

```text
Input:
플레이어 질문 + context + 검색 근거

Output:
답변 text + 실행 actions + sources/recommended_actions/confidence metadata
```

## 파일별 역할

### 1. `agent.py`

`operator_guide` agent의 입구다.

Unreal UI / Front에서 온 요청이 기존 router와 orchestrator를 지나면 최종적으로 `QAChatbotAgent.process()`가 호출된다.

이 파일은 다음 일을 한다.

- payload에서 `question`을 꺼낸다.
- `service.py`에 질문 처리를 맡긴다.
- service 결과를 `AgentResponsePayload`로 포장한다.
- 최종적으로 `text`, `actions`, `metadata`를 반환한다.

### 2. `service.py`

Manual Q&A 내부 처리의 작업 관리자다.

직접 모든 판단과 답변 생성을 다 하는 파일은 아니고, 아래 파일들을 순서대로 연결한다.

```text
service.py
-> question_classifier.py
-> csv_repository.py
-> response_builder.py
```

### 3. `question_classifier.py`

질문이 어떤 질문인지 분류한다.

현재 프로토에서는 룰베이스로 5개 중 하나를 선택한다.

- `equipment_question`
- `resource_question`
- `recipe_question`
- `troubleshooting_question`
- `unknown_question`

예를 들어 `제련기가 뭐야?`라는 질문은 다음 기준으로 처리된다.

```text
질문 안에 "제련기"가 있음
-> equipment.csv에서 "제련기" 장비를 찾음
질문 안에 "뭐야"가 있음
-> 장비 설명 질문으로 판단
결과: equipment_question
```

즉, `제련기` 같은 대상 이름은 키워드 목록이 아니라 CSV 데이터에서 찾고, `뭐야`, `역할`, `설명` 같은 표현은 질문 의도를 판단하는 데 사용한다.

### 4. `csv_repository.py`

실제 CSV 파일을 읽고 데이터를 찾는 파일이다.

현재 프로토에서 읽는 CSV는 아래 5개뿐이다.

- `data/game/equipment.csv`
- `data/game/resources.csv`
- `data/game/recipes.csv`
- `data/game/troubleshooting_rules.csv`
- `data/game/action_policy.csv`

예를 들어 `제련기가 뭐야?`라면 `equipment.csv`에서 `제련기` row를 찾는다.

찾은 row에는 이런 정보가 있다.

```text
equipment_id = equipment_smelter
name = 제련기
role = 광석을 금속 자원으로 변환하는 생산 장비
input_resources = 철광석, 구리광석
output_resources = 철괴, 구리괴
power_required = 10
```

### 5. `repository.py`

현재는 실제 조회 로직이 거의 없다.

이 파일은 기존 import 경로를 유지하기 위한 얇은 연결 파일이다.

쉽게 말하면 다음과 같다.

```text
repository.py = csv_repository.py로 연결해주는 안내판
```

나중에 다른 코드가 아래처럼 import하더라도 깨지지 않게 하기 위해 둔다.

```python
from agents.operator_guide.repository import CsvManualQARepository
```

실제 CSV 조회는 `csv_repository.py`가 담당한다.

### 6. `response_builder.py`

CSV에서 찾은 데이터를 사람이 읽을 수 있는 답변으로 만든다.

예를 들어 `제련기가 뭐야?`에 대해서는 `equipment.csv`의 제련기 정보를 바탕으로 다음과 같은 답변을 만든다.

```text
제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다.
입력 자원은 철광석, 구리광석이고,
출력 자원은 철괴, 구리괴입니다.
필요 전력은 10입니다.
```

또한 답변 근거인 `sources`, 추천 행동인 `recommended_actions`, 신뢰도인 `confidence`도 함께 만든다.

### 7. `schemas.py`

데이터 모양을 정하는 파일이다.

답변을 직접 만드는 파일은 아니다. 대신 아래 값들이 어떤 구조를 가져야 하는지 정의한다.

- 질문 payload
- 질문 분류 결과
- source 구조
- 추천 행동 구조
- 최종 Manual Q&A 결과 구조

## 예시: `제련기가 뭐야?`

```text
1. Unreal UI / Front가 agent="operator_guide"으로 질문 전송
2. QAChatbotAgent.process() 호출
3. service.py가 처리 시작
4. question_classifier.py가 질문 분석
   - CSV에서 "제련기" 장비 발견
   - 질문에 "뭐야" 표현 발견
   - equipment_question으로 분류
5. csv_repository.py가 equipment.csv에서 제련기 정보 조회
6. response_builder.py가 템플릿 답변 생성
7. agent.py가 AgentResponsePayload로 포장
8. Unreal UI / Front로 응답 반환
```

응답 구조는 대략 다음과 같다.

```json
{
  "text": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다...",
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
        "label": "장비 역할 설명"
      }
    ],
    "confidence": "high"
  }
}
```

## 현재 프로토와 최종 구조 차이

현재 프로토는 룰베이스다.

```text
질문
-> 키워드와 CSV 이름 매칭으로 질문 유형 판단
-> CSV 직접 조회
-> 템플릿 답변 생성
```

최종 구조에서는 PostgreSQL과 pgvector를 사용할 수 있다.

```text
질문
-> PostgreSQL 구조 데이터 조회
-> pgvector로 관련 manual chunk 검색
-> 검색 근거를 LLM에 전달
-> LLM이 근거 안에서 질문 유형과 답변 판단
-> sources와 recommended_actions를 포함해 응답
```

최종에서도 중요한 원칙은 같다. LLM이 아무 근거 없이 답을 지어내는 것이 아니라, 검색된 데이터와 문서를 근거로 답해야 한다.

## 최종 아키텍처

최종 Manual Q&A Agent는 CSV를 직접 읽는 구조가 아니라 PostgreSQL과 pgvector를 함께 사용하는 구조로 확장한다.

```text
Unreal UI / Front
-> agent="operator_guide" 질문 전송
-> MessageRouter
-> AgentOrchestrator
-> AgentRegistry에서 operator_guide 조회
-> QAChatbotAgent.process()
-> ManualQAService
-> Hybrid Intent Router
-> PostgreSQL Repository
-> pgvector Retriever
-> Answer Builder / LLM Answer Generator
-> AgentResponsePayload
-> Unreal UI / Front 응답
```

### 1. PostgreSQL Repository

PostgreSQL은 장비, 자원, 레시피, 문제 해결 규칙, action policy처럼 구조가 명확한 데이터를 저장한다.

예상 테이블은 다음과 같다.

- `equipment`
- `resources`
- `recipes`
- `troubleshooting_rules`
- `action_policy`
- `manual_documents`
- `manual_chunks`

역할:

- 정확한 ID 기반 조회
- 장비/자원/레시피 이름 기반 조회
- 문제 해결 rule 조회
- 추천 행동 action policy 조회
- source ID와 문서 metadata 관리

예를 들어 `제련기가 뭐야?`라는 질문이 들어오면 PostgreSQL에서 `equipment_smelter` 장비 row를 조회한다.

### 2. pgvector Retriever

pgvector는 Markdown 매뉴얼이나 긴 설명 문서를 embedding vector로 저장하고, 질문과 의미가 가까운 manual chunk를 찾는 역할을 한다.

역할:

- 자연어 질문과 비슷한 매뉴얼 문단 검색
- CSV/DB 구조 데이터만으로 부족한 설명 보강
- 답변의 근거 source 제공
- unknown 질문인지 판단할 때 참고할 검색 결과 제공

예를 들어 플레이어가 `제련기가 자꾸 멈추는데 뭐부터 봐야 해?`라고 물으면, pgvector는 `machine_stopped`, `no_power`, `no_input`, `output_full` 관련 문단을 찾아올 수 있다.

### 3. Hybrid Intent Router

최종에서는 단순 키워드만으로 질문 유형을 고르지 않는다.

Hybrid Intent Router는 아래 근거를 함께 본다.

- PostgreSQL exact lookup 결과
- pgvector 검색 결과
- 질문 문장
- 선택된 장비나 현재 UI context
- 필요하면 최소한의 `player_state`

분류 결과는 현재 프로토와 같은 5개 유형을 유지한다.

- `equipment_question`
- `resource_question`
- `recipe_question`
- `troubleshooting_question`
- `unknown_question`

중요한 점은 LLM이 처음부터 마음대로 판단하는 것이 아니라, PostgreSQL과 pgvector 검색 결과를 근거로 판단한다는 것이다.

### 4. Answer Builder / LLM Answer Generator

최종 답변 생성은 두 단계로 나눌 수 있다.

```text
검색된 근거 정리
-> LLM에 근거 전달
-> LLM이 근거 안에서 답변 생성
-> Answer Builder가 응답 schema에 맞게 포장
```

LLM이 사용할 수 있는 정보는 다음으로 제한한다.

- PostgreSQL 조회 결과
- pgvector 검색 결과
- action_policy
- source metadata
- 필요한 경우 최소한의 context/player_state

LLM은 근거에 없는 장비, 자원, 레시피, 업그레이드 방법을 지어내면 안 된다.

### 5. 최종 응답 구조

최종에서도 Unreal로 반환하는 큰 구조는 유지한다.

```json
{
  "text": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다.",
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
        "priority": 1
      }
    ],
    "confidence": "high"
  }
}
```

`actions`와 `recommended_actions`는 계속 분리한다.

- `actions`: Unreal이 실제 실행할 명령
- `recommended_actions`: UI에 보여줄 추천 확인 행동

### 6. 현재 프로토에서 최종으로 넘어가는 단계

```text
1. CSV Repository
-> PostgreSQL Repository로 교체

2. question_classifier.py의 룰베이스 분류
-> Hybrid Intent Router로 확장

3. Markdown 문서
-> chunking 후 embedding 생성
-> pgvector에 저장

4. response_builder.py 템플릿 답변
-> 검색 근거 기반 LLM 답변 생성으로 확장

5. 단순 unknown 처리
-> 검색 결과 부족, source 부족, confidence 낮음 기준으로 안정화
```

이렇게 확장해도 외부 호출 구조는 바꾸지 않는다.

```text
Unreal UI / Front
-> agent="operator_guide"
-> 기존 MessageRouter
-> 기존 AgentOrchestrator
-> 기존 AgentRegistry
-> QAChatbotAgent.process()
```

최종 아키텍처의 핵심은 내부 검색과 답변 생성만 강해지고, Unreal과 주고받는 agent 계약은 안정적으로 유지하는 것이다.
# 최신 이름 기준

최신 `main` 구조에서는 Manual Q&A 도메인을 `operator_guide`이 아니라 `operator_guide` 이름으로 정리한다.

쉽게 말하면:

```text
이전 프로토 이름: operator_guide
현재 공식 도메인 이름: operator_guide
```

현재 Manual Q&A 프로토 코드는 아래 위치에 있다.

```text
backend/src/agents/operator_guide/
```

이 구조에서 `service.py`가 질문 처리 흐름을 연결하고, `question_classifier.py`가 질문 유형을 분류하며, `csv_repository.py`가 지정된 CSV를 조회하고, `response_builder.py`가 답변과 metadata를 만든다.

