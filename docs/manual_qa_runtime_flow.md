# Manual Q&A Agent 처리 흐름 설명

Manual Q&A Agent는 게임 안에서 플레이어가 장비, 자원, 제작 공정, 문제 상황을 물어보면 매뉴얼 데이터를 근거로 답변과 추천 확인 행동을 제공하는 안내 에이전트다.

이 문서는 Manual Q&A Agent가 Unreal UI / Front에서 질문을 받았을 때 내부에서 어떤 순서로 처리되는지 쉽게 설명한다.

## 전체 흐름

```text
Unreal UI / Front
-> agent="qa_chatbot"으로 질문 전송
-> MessageRouter
-> AgentOrchestrator
-> AgentRegistry에서 qa_chatbot 찾기
-> QAChatbotAgent.process()
-> ManualQAService
-> ManualQAIntentRouter
-> CsvManualQARepository
-> ManualQAResponseBuilder
-> AgentResponsePayload 생성
-> MessageRouter를 통해 Unreal UI / Front로 응답 반환
```

중요한 점은 Unreal UI / Front가 직접 `service.py`를 호출하지 않는다는 것이다. 먼저 기존 WebSocket 메시지 처리 구조를 지나고, 그 다음 `QAChatbotAgent.process()` 안에서 Manual Q&A 내부 로직이 실행된다.

## 파일별 역할

### 1. `agent.py`

`qa_chatbot` agent의 입구다.

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
-> intent_router.py
-> csv_repository.py
-> response_builder.py
```

### 3. `intent_router.py`

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
from factory_space.agents.qa_chatbot.repository import CsvManualQARepository
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
1. Unreal UI / Front가 agent="qa_chatbot"으로 질문 전송
2. QAChatbotAgent.process() 호출
3. service.py가 처리 시작
4. intent_router.py가 질문 분석
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
