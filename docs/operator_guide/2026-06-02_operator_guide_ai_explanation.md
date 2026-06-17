# 최종 Manual Q&A Agent 설명

## 한 줄 소개

Manual Q&A Agent는 플레이어가 공장 운영 중 궁금한 점이나 막힌 상황을 질문하면, 게임 매뉴얼 지식과 현재 플레이 상태를 함께 보고 근거 있는 답변과 추천 행동을 제공하는 AI 안내 에이전트다.

## 이 에이전트가 필요한 이유

공장 자동화 게임에서는 플레이어가 자주 이런 질문을 하게 된다.

```text
제련기가 왜 안 돌아가?
철괴는 어떻게 만들어?
기어 만들려면 뭐가 필요해?
전력이 부족하면 뭘 확인해야 해?
컨베이어가 막혔을 때 어떻게 해야 해?
```

이 질문들은 단순한 잡담이 아니라, 게임 안의 장비, 자원, 레시피, 문제 해결 지식과 연결되어 있다.

Manual Q&A Agent는 이 정보를 구조화된 매뉴얼 지식으로 관리하고, 플레이어의 질문에 맞는 답을 찾아준다.

## 최종 목표 구조

최종 구조에서는 CSV만 직접 조회하지 않고, PostgreSQL과 pgvector를 사용한다.

```text
사용자 질문
-> operator_guide Agent
-> 질문 유형 분석
-> player_state 분석
-> PostgreSQL 구조화 데이터 조회
-> pgvector 기반 매뉴얼 문서 검색
-> LLM이 근거 기반 답변 생성
-> final_answer, sources, recommended_actions 반환
```

## Input

Unreal UI 또는 Front는 플레이어 질문을 `operator_guide` agent로 보낸다.

예시:

```json
{
  "type": "agent.request",
  "request_id": "req-001",
  "session_id": "session-001",
  "client_id": "unreal-ui-001",
  "agent": "operator_guide",
  "payload": {
    "question": "제련기가 왜 안 돌아가?",
    "player_state": {
      "selected_machine_id": "machine_smelter_01",
      "selected_equipment_id": "equipment_smelter",
      "power_status": "low",
      "input_inventory": {
        "resource_iron_ore": 0
      },
      "output_inventory_status": "not_full",
      "recipe_id": "recipe_iron_ingot"
    }
  },
  "context": {
    "screen": "factory-floor",
    "language": "ko"
  }
}
```

중요한 입력값은 다음과 같다.

```text
question: 플레이어가 실제로 입력한 질문
player_state: 현재 선택된 장비, 전력, 입력 자원, 출력 상태 등 게임 상태
context: 현재 화면, 언어, 세션 정보
```

## 내부 처리 로직

### 1. 질문 유형을 판단한다

먼저 질문이 어떤 종류인지 판단한다.

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

이 단계는 질문이 어느 매뉴얼 영역을 우선 검색해야 하는지 정하는 과정이다.

### 2. 현재 플레이 상태를 함께 본다

최종 버전에서는 질문만 보지 않는다.  
현재 게임 상태도 함께 본다.

예를 들어 플레이어가 이렇게 질문했다고 하자.

```text
제련기가 왜 안 돌아가?
```

이때 `player_state`에 아래 정보가 들어올 수 있다.

```text
선택된 장비: 제련기
전력 상태: low
입력 철광석 수량: 0
출력 공간: 막히지 않음
설정 레시피: 철괴 제작
```

그러면 Agent는 단순히 “전력, 입력, 출력, 컨베이어를 확인하세요”라고만 말하지 않는다.  
현재 상태를 근거로 더 구체적인 원인을 말할 수 있다.

### 3. PostgreSQL에서 구조화 데이터를 조회한다

PostgreSQL에는 장비, 자원, 레시피, 문제 해결 규칙 같은 구조화 데이터가 들어간다.

예시 테이블:

```text
equipment
resources
recipes
troubleshooting_rules
action_policy
manual_documents
manual_chunks
```

`equipment_smelter`를 기준으로 제련기의 역할, 입력 자원, 출력 자원, 관련 문제를 조회한다.

### 4. pgvector로 관련 매뉴얼 문서를 검색한다

Markdown 매뉴얼 문서는 작은 chunk로 나누고 embedding을 만들어 pgvector에 저장한다.

질문:

```text
제련기가 왜 안 돌아가?
```

검색될 수 있는 문서:

```text
장비가 멈췄을 때
전력 부족
입력 자원 부족
제련기 매뉴얼
철괴 제작 공정
```

이때 pgvector는 질문과 의미가 가까운 매뉴얼 chunk를 찾아준다.

### 5. LLM이 근거 기반 답변을 만든다

LLM은 아무렇게나 답하지 않는다.  
검색된 문서와 player_state를 근거로만 답한다.

시스템 지침은 대략 이런 방향이다.

```text
너는 Factory Space 게임의 Manual Q&A Agent다.
제공된 매뉴얼 문서와 player_state에 근거해서만 답변한다.
근거가 없으면 모른다고 말한다.
실제 Unreal 실행 action은 만들지 않는다.
추천 행동은 metadata.recommended_actions에만 넣는다.
```

## Output

최종 응답은 플레이어에게 보여줄 답변과, 프론트/Unreal이 활용할 수 있는 metadata를 함께 반환한다.

예시:

```json
{
  "type": "agent.response",
  "request_id": "req-001",
  "session_id": "session-001",
  "client_id": "unreal-ui-001",
  "agent": "operator_guide",
  "payload": {
    "final_answer": "제련기가 멈춘 가장 가능성 높은 이유는 입력 철광석이 부족하고 전력 상태도 낮기 때문입니다. 먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.",
    "text": "제련기가 멈춘 가장 가능성 높은 이유는 입력 철광석이 부족하고 전력 상태도 낮기 때문입니다. 먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.",
    "answer": "제련기가 멈춘 가장 가능성 높은 이유는 입력 철광석이 부족하고 전력 상태도 낮기 때문입니다. 먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.",
    "actions": [],
    "metadata": {
      "question": "제련기가 왜 안 돌아가?",
      "question_type": "troubleshooting_question",
      "confidence": "high",
      "diagnosis": {
        "primary_issue_id": "issue_no_input",
        "supporting_issue_ids": [
          "issue_no_power",
          "issue_machine_stopped"
        ],
        "reason": "player_state에서 제련기의 입력 철광석 수량이 0이고 전력 상태가 low로 확인됨"
      },
      "sources": [
        {
          "doc_id": "issue_no_input",
          "type": "troubleshooting",
          "title": "입력 자원 부족",
          "chunk_id": "chunk_issue_no_input_001",
          "score": 0.91
        },
        {
          "doc_id": "issue_no_power",
          "type": "troubleshooting",
          "title": "전력 부족",
          "chunk_id": "chunk_issue_no_power_001",
          "score": 0.86
        },
        {
          "doc_id": "equipment_smelter",
          "type": "equipment",
          "title": "제련기",
          "chunk_id": "chunk_equipment_smelter_001",
          "score": 0.82
        }
      ],
      "recommended_actions": [
        {
          "action_id": "action_check_input_resource",
          "label": "입력 자원 확인",
          "description": "제련기에 철광석이 공급되는지 확인한다",
          "priority": 1,
          "target": {
            "machine_id": "machine_smelter_01",
            "resource_id": "resource_iron_ore"
          }
        },
        {
          "action_id": "action_check_power",
          "label": "전력 상태 확인",
          "description": "발전기와 전력 연결 상태를 확인한다",
          "priority": 2,
          "target": {
            "machine_id": "machine_smelter_01"
          }
        }
      ],
      "retrieval": {
        "store": "postgresql_pgvector",
        "top_k": 5,
        "embedding_model": "text-embedding-3-small",
        "filters": {
          "manual_types": [
            "troubleshooting",
            "equipment"
          ],
          "equipment_id": "equipment_smelter"
        }
      }
    }
  },
  "streams": []
}
```

## Output 필드 설명

```text
final_answer
```

플레이어에게 실제로 보여줄 최종 답변이다.  
프로토, 알파, 베타, 최종 단계에서 계속 유지한다.

```text
text
```

기존 호환용 텍스트 필드다.  
초기 UI나 기존 테스트가 이 값을 읽을 수 있다.

```text
answer
```

Agent 내부 결과 모델에서 쓰는 답변 본문이다.  
`final_answer`와 같은 값으로 유지할 수 있다.

```text
actions
```

Unreal이 즉시 실행할 action 목록이다.  
Manual Q&A 프로토와 안전한 안내 단계에서는 빈 배열로 둔다.

```text
metadata.sources
```

답변에 사용된 근거 문서 또는 데이터다.  
사용자가 “왜 이렇게 답했는지” 추적할 수 있다.

```text
metadata.recommended_actions
```

실행 명령이 아니라 추천 행동이다.  
예를 들어 “입력 자원 확인”, “전력 상태 확인” 같은 안내가 들어간다.

```text
metadata.diagnosis
```

최종 버전에서 player_state를 분석해 판단한 원인이다.

```text
metadata.retrieval
```

pgvector 검색이 어떤 조건으로 수행됐는지 기록한다.

## 시나리오 설명

### 상황

플레이어가 공장에서 제련기를 선택했다.  
제련기가 멈춰 있고 철괴가 생산되지 않는다.

플레이어가 UI에 질문한다.

```text
제련기가 왜 안 돌아가?
```

### Agent가 보는 정보

Agent는 질문과 함께 현재 상태를 받는다.

```text
선택된 장비: 제련기
입력 철광석: 0개
전력 상태: 낮음
출력 저장 공간: 비어 있음
레시피: 철괴 제작
```

### Agent의 판단 과정

1. 질문에 “왜”, “안 돌아가”가 있으므로 문제 해결 질문으로 분류한다.
2. 선택된 장비가 제련기이므로 제련기 관련 매뉴얼을 우선 조회한다.
3. player_state에서 철광석이 0개인 것을 확인한다.
4. 전력 상태가 낮은 것도 확인한다.
5. pgvector에서 “입력 자원 부족”, “전력 부족”, “장비가 멈췄을 때” 문서를 찾는다.
6. LLM이 이 근거를 바탕으로 최종 답변을 만든다.

### 플레이어가 받는 답변

```text
제련기가 멈춘 가장 가능성 높은 이유는 입력 철광석이 부족하고 전력 상태도 낮기 때문입니다.
먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.
```

### 같이 반환되는 추천 행동

```text
1. 입력 자원 확인
2. 전력 상태 확인
```

## 프로토와 최종의 차이

### 프로토

```text
질문
-> 5개 CSV 직접 조회
-> 템플릿 답변 생성
```

프로토 답변:

```text
제련기가 멈췄다면 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인한다.
```

### 최종

```text
질문 + player_state
-> PostgreSQL 구조화 데이터 조회
-> pgvector 매뉴얼 검색
-> LLM 근거 기반 답변 생성
```

최종 답변:

```text
현재 제련기의 입력 철광석이 0개이고 전력 상태도 낮습니다.
가장 먼저 철광석 공급 라인을 확인하고, 그다음 전력 연결 상태를 확인하세요.
```

## 정리

Manual Q&A Agent는 단순히 질문에 답하는 챗봇이 아니다.

```text
게임 매뉴얼 지식
+ 현재 플레이 상태
+ 검색된 근거 문서
+ 추천 행동
```

이 네 가지를 연결해서 플레이어가 지금 무엇을 확인해야 하는지 알려주는 AI 안내 에이전트다.
