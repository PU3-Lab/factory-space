# Operator Guide Agent 시연용 예상 질문 리스트

이 문서는 시연(Demo) 시 오퍼레이터 가이드(`operator_guide`) 에이전트의 작동 검증을 극대화하기 위해 설계된 예상 질문 리스트와 각 시나리오별 포인트입니다.

현재 시스템에 적용된 CSV 매뉴얼 데이터베이스(`equipment.csv`, `recipes.csv`, `troubleshooting_rules.csv`, `tutorial.csv` 등)를 바탕으로 가장 자연스럽고 신뢰도 높은 답변을 도출하는 핵심 질문을 정리했습니다.

---

## 1. ⚙️ 장비 지식 시나리오 (Machine Help)

플레이어가 게임 내 설치된 장비의 기능, 필요 전력, 입출력 자원을 문의할 때 사용합니다.
* **담당 에이전트**: `operator_guide.machine_help`
* **참조 데이터**: `equipment.csv`

| 시연 질문 예시 | 테스트 목적 및 기대 결과 |
| :--- | :--- |
| **"분쇄기가 무슨 역할을 하는 장비야?"** | 장비의 기본 역할(`role`)과 카테고리(`category`)를 설명하는지 확인합니다. |
| **"정제소는 전력을 얼마나 소모해?"** | 필요 전력량(`power_required`) 수치가 올바르게 노출되는지 검증합니다. |
| **"분쇄기 다음에는 어떤 장비를 연결해야 해?"** | `connectable_equipment`에 정의된 연결 가능 장비 리스트를 올바르게 제안하는지 확인합니다. |

---

## 2. 🛠️ 제작법 및 레시피 시나리오 (Recipe Explainer)

특정 아이템의 제작 방법, 재료 종류, 제작에 필요한 장비 및 가공 공정 단계를 알아볼 때 사용합니다.
* **담당 에이전트**: `operator_guide.recipe_explainer`
* **참조 데이터**: `recipes.csv`, `resources.csv`

| 시연 질문 예시 | 테스트 목적 및 기대 결과 |
| :--- | :--- |
| **"기어는 어떻게 만들어?"** | 필요한 입력 자원(`input_resources`)과 필요한 제작 장비, 생산 절차(`production_steps`)를 일목요연하게 정리해 주는지 검증합니다. |
| **"철괴를 만들려면 어떤 장비가 필요해?"** | 철괴의 레시피에 매핑된 필요 장비(`required_equipment`) 정보를 정상적으로 조회하는지 확인합니다. |
| **"제작할 때 자주 발생하는 문제(병목)는 뭐야?"** | `common_bottlenecks`에 기재된 레시피상 병목 요인을 설명하여 공장 최적화 가이드를 제공하는지 검증합니다. |

---

## 3. 🚨 공장 장애 진단 시나리오 (Troubleshooter)

기계 작동 정지, 물류 벨트 정체 등 공장에 문제가 생겼을 때 원인을 파악하고 추천 조치 사항(Action)을 요청할 때 사용합니다.
* **담당 에이전트**: `operator_guide.troubleshooter`
* **참조 데이터**: `troubleshooting_rules.csv`, `action_policy.csv`

| 시연 질문 예시 | 테스트 목적 및 기대 결과 |
| :--- | :--- |
| **"컨베이어 벨트가 멈췄는데 뭘 확인해야 해?"** | `symptom`(증상) 매칭을 통해 점검 순서(`check_order`)와 해결책(`resolution`)을 순서대로 명확히 안내하는지 확인합니다. |
| **"기계가 안 돌아가는데 원인이 뭘까?"** | `possible_causes`(가능한 원인)들을 나열하고 상황에 맞는 점검 지침을 친절하게 제시하는지 검증합니다. |
| **"전력이 부족할 때는 어떻게 해결해야 해?"** | 추천 행동(`action_policy.csv` 연동)에 기반한 구체적인 추천 행동 버튼 정보와 해결 방법이 같이 언급되는지 검증합니다. |

---

## 4. 🧭 튜토리얼 및 진행 방향 시나리오 (Progress / RAG)

"다음에 내가 해야 할 일"이나 튜토리얼 퀘스트 목표를 물어보고 NPC로서 진행을 독려하는 상황을 검증합니다.
* **참조 데이터**: `tutorial.csv`

| 시연 질문 예시 | 테스트 목적 및 기대 결과 |
| :--- | :--- |
| **"지금 다음 목표가 뭐야?"** | 현재 튜토리얼 정보 및 그룹 이름(`group_name`)을 인식하여 다음 단계 지침(`description`)을 대답하는지 확인합니다. |
| **"철광석 채굴기 튜토리얼은 어떻게 깨?"** | `tutorial.csv`에 있는 해당 튜토리얼의 대사 정보와 설명 텍스트를 인용해 클리어 조건을 설명해 주는지 확인합니다. |

---

## 5. 👥 다중 질문 시나리오 (Multi-Question / Decomposer)

플레이어가 한 문장 안에 서로 다른 두 가지 이상의 질문을 병합하여 전달하는 상황을 시연하고 검증합니다.
* **사용 기술**: `QuestionDecomposer`([question_decomposer.py](file:///c:/factory-space/backend/src/agents/operator_guide/question_decomposer.py)), `MultiQuestionRagRetriever`([multi_question_rag_retriever.py](file:///c:/factory-space/backend/src/agents/operator_guide/multi_question_rag_retriever.py))

| 시연 질문 예시 | 테스트 목적 및 기대 결과 |
| :--- | :--- |
| **"분쇄기는 무슨 역할을 하고 기어는 어떻게 만들어?"** | 입력 문장을 "분쇄기가 무슨 역할을 해?"와 "기어는 어떻게 만들어?" 두 개의 개별 서브 질문으로 나누고, 각각 RAG 검색을 실행하여 하나의 대답으로 조화롭게 출력하는지 확인합니다. |
| **"컨베이어 벨트가 멈췄는데 정제소의 전력 소모량은 어떻게 돼?"** | 트러블슈팅과 장비 지식 질문이 섞여도 문맥이 깨지지 않고 각 질문의 정보를 고르게 포함하여 대답하는지 검증합니다. |

---

## 6. 🔌 시연 API 요청/응답 JSON 규격 예시

위의 **다중 질문 시나리오**("분쇄기는 무슨 역할을 하고 기어는 어떻게 만들어?")를 실행할 때, 클라이언트(Unreal Engine 등)와 에이전트 백엔드 파이프라인 사이에 주고받는 실제 JSON 포맷 명세입니다.

### 📥 인풋 JSON (AgentRequestEnvelope)

```json
{
  "type": "agent.request",
  "request_id": "demo-request-uuid-1234",
  "session_id": "player-session-abc",
  "client_id": "unreal-client-1",
  "agent": "operator_guide",
  "payload": {
    "question": "분쇄기는 무슨 역할을 하고 기어는 어떻게 만들어?"
  },
  "context": {
    "current_game_state": {
      "powerStatus": "ON",
      "gridLoad": "75%"
    }
  }
}
```

### 📤 아웃풋 JSON (AgentResponseEnvelope)

```json
{
  "type": "agent.response",
  "request_id": "demo-request-uuid-1234",
  "session_id": "player-session-abc",
  "client_id": "unreal-client-1",
  "agent": "operator_guide",
  "payload": {
    "question": "분쇄기는 무슨 역할을 하고 기어는 어떻게 만들어?",
    "topic": "recipe",
    "final_answer": "분쇄기는 광석 등의 원자재를 1차 가공하여 분말이나 파편으로 파쇄해 주는 기초 설비입니다. 그리고 기어는 제작대 또는 조립기에서 철판 2개를 소모하여 생산할 수 있으며, 선행 레시피로 기초 야금술 연구가 요구됩니다. 공장 내 추가 질문이 있으시면 언제든지 편하게 물어보세요!",
    "metadata": {
      "selectedAgent": "operator_guide",
      "selectedLeafAgent": "operator_guide.recipe_explainer",
      "llm": "used",
      "llmModel": "gemini-1.5-flash",
      "retrieval": {
        "is_multi_question": true,
        "sub_question_count": 2,
        "max_sub_questions": 3,
        "truncated": false,
        "confidence_counts": {
          "high": 2,
          "medium": 0,
          "low": 0
        },
        "sub_questions": [
          {
            "index": 1,
            "question": "분쇄기는 무슨 역할을 해?"
          },
          {
            "index": 2,
            "question": "기어는 어떻게 만들어?"
          }
        ]
      },
      "memory": {
        "used": true,
        "turn_count": 1,
        "max_turns": 5,
        "confirmed_facts": [],
        "summary_version": 1
      }
    }
  },
  "streams": []
}
```

---

## 7. 💡 시연 진행자를 위한 추가 팁 (Demo Tips)

* **질문은 구체적으로 할 것**: 
  "이거 안 돼"와 같은 모호한 문장보다는, **"정제소가 작동하지 않아"**처럼 장비명이나 아이템 이름을 명시적으로 포함하여 질문할 때 매뉴얼 매칭 확률과 답변 퀄리티가 대폭 상승합니다.
* **실시간 게임 상태 결합**: 
  Unreal에서 장비를 선택한 채로 **"이 기계 상태 어때?"**라고 질문하면 실시간 게임 상태(`current_game_state`)의 Scopes(예: powerStatus, inputInventory)를 자동으로 추출하여 맞춤 답변을 생성하는 기능을 시연하기 좋습니다.
* **비상 모드(Fallback) 검증**: 
  만약 시연 환경에서 인터넷 통신이 끊어지거나 LLM API 호출에 장애가 생기더라도, 에이전트가 에러로 멈추지 않고 **정적인 매뉴얼 설명으로 즉시 대체하여 답변(Fallback)**하는 안정성도 함께 체크해 보면 좋습니다.
