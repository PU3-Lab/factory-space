# 최종 Manual Q&A Agent 설명

## 한 줄 소개

Manual Q&A Agent는 플레이어가 공장 운영 중 궁금한 점이나 막힌 상황을 질문하면, 게임 매뉴얼 지식과 현재 플레이 상태를 함께 보고 근거 있는 답변과 추천 행동을 제공하는 AI 안내 에이전트다.

## 현재 프로토 흐름

프로토 단계에서는 PostgreSQL, pgvector, Embedding, LLM 기반 판단, 복잡한 `player_state` 분석을 사용하지 않는다.

현재 목표는 `operator_guide`가 선택되었을 때 5개 CSV를 직접 조회해서 기본 답변 흐름을 검증하는 것이다.

```text
1. Unreal UI / Front가 질문을 보냄
2. AgentPipeline / LangGraph 경로로 들어감
3. Orchestrator가 operator_guide를 선택함
4. operator_guide 내부에서 leaf agent를 선택함
   - machine_help
   - recipe_explainer
   - troubleshooter
5. 선택된 leaf agent의 fallback에서 ManualQAService를 호출함
6. ManualQAService가 질문을 세부 유형으로 분류함
   - equipment_question
   - resource_question
   - recipe_question
   - troubleshooting_question
   - unknown_question
7. csv_repository가 지정된 5개 CSV를 직접 조회함
8. response_builder가 템플릿 기반 답변을 생성함
9. AgentPipeline이 final_answer, sources, recommended_actions를 Unreal UI / Front로 반환함
```

프로토에서 사용하는 CSV는 아래 5개다.

```text
data/game/equipment.csv
data/game/resources.csv
data/game/recipes.csv
data/game/troubleshooting_rules.csv
data/game/action_policy.csv
```

## 프로토 예시

플레이어 질문:

```text
제련기가 왜 안 돌아가?
```

프로토 처리:

```text
operator_guide 선택
-> troubleshooter leaf agent 선택
-> ManualQAService 호출
-> troubleshooting_question으로 분류
-> troubleshooting_rules.csv에서 issue_machine_stopped 조회
-> equipment.csv에서 equipment_smelter 조회
-> 템플릿 답변 생성
```

프로토 답변 예시:

```text
제련기가 멈췄다면 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인합니다.
```

프로토 응답 구조:

```json
{
  "final_answer": "제련기가 멈췄다면 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인합니다.",
  "text": "제련기가 멈췄다면 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인합니다.",
  "actions": [],
  "metadata": {
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
        "label": "전력 상태 확인"
      },
      {
        "action_id": "action_check_input_resource",
        "label": "입력 자원 확인"
      }
    ],
    "confidence": "high"
  }
}
```

프로토에서는 실제 Unreal 실행 action을 만들지 않는다.

```text
payload.actions = []
```

추천 행동은 실행 명령이 아니라 안내 정보이므로 아래에 넣는다.

```text
payload.metadata.recommended_actions
```

## 최종 목표 흐름

최종 단계에서는 질문만 보는 것이 아니라 현재 게임 상태까지 함께 본다.

```text
1. Unreal UI / Front가 질문과 player_state를 보냄
2. AgentPipeline / LangGraph 경로로 들어감
3. Orchestrator가 operator_guide를 선택함
4. operator_guide 내부에서 leaf agent를 선택함
   예: 제련기가 왜 안 돌아가? -> troubleshooter
5. leaf agent가 Manual Q&A Tool들을 호출함
6. 질문 유형을 분류함
7. player_state를 분석함
8. PostgreSQL에서 구조화 데이터를 조회함
9. 질문을 embedding으로 변환함
10. pgvector에서 관련 Markdown 매뉴얼 chunk를 검색함
11. LLM이 검색 결과, player_state, 구조화 데이터를 근거로 답변을 생성함
12. response_builder가 최종 응답 JSON을 정리함
13. AgentPipeline이 Unreal UI / Front로 응답을 반환함
```

## 최종 Input 예시

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

## 최종 판단 시나리오

플레이어가 이렇게 질문한다.

```text
제련기가 왜 안 돌아가?
```

최종 Agent는 질문과 함께 현재 상태를 본다.

```text
선택된 장비: 제련기
전력 상태: 낮음
입력 철광석: 0개
출력 공간: 막히지 않음
설정 레시피: 철괴 제작
```

그러면 Agent는 이렇게 판단한다.

```text
질문은 문제 해결 질문이다.
현재 선택된 장비는 제련기다.
철광석 입력이 0개다.
전력 상태도 낮다.
출력 공간은 막히지 않았다.
따라서 가장 가능성 높은 원인은 입력 자원 부족과 전력 부족이다.
```

최종 답변 예시:

```text
현재 제련기는 입력 철광석이 없고 전력 상태도 낮아서 멈춘 가능성이 큽니다.
먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.
출력 저장 공간은 현재 막힌 것으로 보이지 않습니다.
```

## 최종 Output 예시

```json
{
  "final_answer": "현재 제련기는 입력 철광석이 없고 전력 상태도 낮아서 멈춘 가능성이 큽니다. 먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.",
  "text": "현재 제련기는 입력 철광석이 없고 전력 상태도 낮아서 멈춘 가능성이 큽니다. 먼저 철광석 공급 라인을 확인하고, 그다음 발전기와 전력 연결 상태를 확인하세요.",
  "actions": [],
  "metadata": {
    "question_type": "troubleshooting_question",
    "confidence": "high",
    "diagnosis": {
      "primary_issue_id": "issue_no_input",
      "supporting_issue_ids": [
        "issue_no_power",
        "issue_machine_stopped"
      ],
      "reason": "player_state에서 철광석 입력이 0개이고 전력 상태가 low로 확인됨"
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
        "priority": 1
      },
      {
        "action_id": "action_check_power",
        "label": "전력 상태 확인",
        "description": "발전기와 전력 연결 상태를 확인한다",
        "priority": 2
      }
    ],
    "retrieval": {
      "store": "postgresql_pgvector",
      "embedding_model": "text-embedding-3-small",
      "top_k": 5
    }
  }
}
```

## 프로토에서 최종까지 고도화 계획

### 1. 프로토

목표:

```text
operator_guide가 선택되었을 때 5개 CSV 기반으로 기본 답변이 나오는지 검증한다.
```

구현 범위:

```text
질문 유형 분류
CSV 직접 조회
템플릿 답변 생성
sources 반환
recommended_actions 반환
final_answer 반환
```

제외 범위:

```text
PostgreSQL
pgvector
Embedding
Markdown RAG
LLM Judge
복잡한 player_state 분석
실제 Unreal 실행 action
```

성공 기준:

```text
대표 질문 5개가 operator_guide 경로에서 정상 응답한다.
payload.actions는 빈 배열이다.
metadata.sources와 metadata.recommended_actions가 포함된다.
```

### 2. 알파

목표:

```text
CSV로 정의한 지식을 PostgreSQL 구조화 데이터로 옮기고, Repository만 교체한다.
```

고도화 내용:

```text
equipment, resources, recipes, troubleshooting_rules, action_policy 테이블 설계
CSV Repository -> PostgreSQL Repository 교체
ManualQAService의 외부 계약 유지
final_answer, sources, recommended_actions 구조 유지
```

중요한 기준:

```text
서비스와 응답 구조는 크게 바꾸지 않는다.
데이터 저장소만 CSV에서 PostgreSQL로 바꾼다.
```

성공 기준:

```text
프로토와 같은 대표 질문이 PostgreSQL 데이터 기반으로 같은 구조의 응답을 반환한다.
```

### 3. 베타

목표:

```text
Markdown 매뉴얼 문서를 RAG 검색 대상으로 만들고 pgvector를 연결한다.
```

고도화 내용:

```text
Markdown 매뉴얼 문서 생성
문서 chunking
embedding 생성
pgvector 저장
질문 embedding 검색
sources에 chunk_id와 score 포함
```

중요한 기준:

```text
구조화 데이터는 PostgreSQL에서 조회한다.
설명과 상세 근거는 pgvector 검색 결과에서 가져온다.
```

성공 기준:

```text
질문과 의미가 가까운 매뉴얼 chunk가 검색된다.
응답 metadata.sources에 검색된 문서와 score가 포함된다.
```

### 4. 최종

목표:

```text
질문, player_state, 구조화 데이터, RAG 검색 결과를 함께 보고 상황 맞춤 답변을 생성한다.
```

고도화 내용:

```text
player_state 분석
LLM 기반 근거 답변 생성
진단 결과 diagnosis 추가
recommended_actions 우선순위화
근거 없는 내용은 답변하지 않는 시스템 프롬프트 적용
필요 시 LLM Judge 또는 평가 루틴 추가
```

중요한 기준:

```text
LLM은 검색된 문서와 player_state에 근거해서만 답변한다.
모르는 내용은 모른다고 말한다.
실제 Unreal 실행 action은 별도 안전 검증 전까지 만들지 않는다.
```

성공 기준:

```text
플레이어가 질문했을 때 현재 상황을 읽고, 질문 의도에 맞는 정확한 답변을 제공한다.
```

## 단계별 차이 요약

| 단계 | 데이터 | 검색 방식 | 답변 생성 | player_state | 목표 |
|---|---|---|---|---|---|
| 프로토 | CSV | 직접 조회 | 템플릿 | 거의 사용 안 함 | 기본 흐름 검증 |
| 알파 | PostgreSQL | 구조화 조회 | 템플릿 또는 간단 생성 | 제한적 사용 | 저장소 전환 |
| 베타 | PostgreSQL + Markdown | pgvector RAG | 근거 기반 생성 | 일부 사용 | 문서 검색 고도화 |
| 최종 | PostgreSQL + pgvector | 구조화 조회 + 의미 검색 | LLM 상황 맞춤 답변 | 적극 사용 | 실제 게임 상황 진단 |

## 최종 정리

프로토는 CSV로 “답변 흐름이 되는지” 확인하는 단계다.

최종은 플레이어 질문, 현재 게임 상태, PostgreSQL 구조화 데이터, pgvector 매뉴얼 검색 결과를 함께 보고 LLM이 근거 기반으로 답변하는 단계다.

핵심 성공 기준은 아래 한 문장이다.

```text
플레이어가 질문했을 때 현재 상황을 읽고, 그 질문에 맞게 정확히 답해주는 것.
```
