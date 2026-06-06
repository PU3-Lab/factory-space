# Manual Q&A CSV 기반 질문지

이 질문지는 `data/game`의 Manual Q&A CSV를 기준으로 만들었다.

사용 목적:
- GPT-5.4 Nano 시연 질문
- 로컬 `gemma4:e2b` 응답 비교
- `operator_guide` 라우팅 및 leaf agent 분기 확인
- CSV 근거 기반 답변 여부 확인

## 1. 장비 질문

예상 leaf agent: `operator_guide.machine_help`

| 번호 | 질문 | CSV 근거 |
| --- | --- | --- |
| 1 | 채굴기는 무슨 역할을 해? | `equipment.csv -> equipment_miner` |
| 2 | 제련기는 뭐 하는 장비야? | `equipment.csv -> equipment_smelter` |
| 3 | 조립기는 언제 써? | `equipment.csv -> equipment_assembler` |
| 4 | 발전기는 왜 필요해? | `equipment.csv -> equipment_generator` |
| 5 | 컨베이어는 어떤 장비야? | `equipment.csv -> equipment_conveyor` |
| 6 | 저장고는 언제 연결해야 해? | `equipment.csv -> equipment_storage` |

## 2. 자원 질문

예상 leaf agent: `operator_guide.machine_help` 또는 `operator_guide.recipe_explainer`

| 번호 | 질문 | CSV 근거 |
| --- | --- | --- |
| 1 | 철광석은 어떻게 얻어? | `resources.csv -> resource_iron_ore` |
| 2 | 철괴는 어디에 써? | `resources.csv -> resource_iron_ingot` |
| 3 | 구리광석은 어떻게 채굴해? | `resources.csv -> resource_copper_ore` |
| 4 | 구리괴는 어떤 제작에 필요해? | `resources.csv -> resource_copper_ingot` |
| 5 | 기어는 뭘 만들 때 필요해? | `resources.csv -> resource_gear` |
| 6 | 전선은 어디에 사용돼? | `resources.csv -> resource_wire` |
| 7 | 전력은 어디서 공급돼? | `resources.csv -> resource_power` |
| 8 | 기본 모터는 어떤 자원으로 만들어? | `resources.csv -> resource_basic_motor` |

## 3. 레시피 질문

예상 leaf agent: `operator_guide.recipe_explainer`

| 번호 | 질문 | CSV 근거 |
| --- | --- | --- |
| 1 | 철괴를 만들려면 뭐가 필요해? | `recipes.csv -> recipe_iron_ingot` |
| 2 | 구리괴 제작 순서를 알려줘. | `recipes.csv -> recipe_copper_ingot` |
| 3 | 기어 만들려면 어떤 재료와 장비가 필요해? | `recipes.csv -> recipe_gear` |
| 4 | 전선은 어떻게 만들어? | `recipes.csv -> recipe_wire` |
| 5 | 기본 모터를 만들려면 어떤 순서로 생산해야 해? | `recipes.csv -> recipe_basic_motor` |

## 4. 문제 해결 질문

예상 leaf agent: `operator_guide.troubleshooter`

| 번호 | 질문 | CSV 근거 |
| --- | --- | --- |
| 1 | 장비가 멈췄는데 뭘 확인해야 해? | `troubleshooting_rules.csv -> issue_machine_stopped` |
| 2 | 제련기가 왜 안 돌아가? | `troubleshooting_rules.csv -> issue_machine_stopped`, `equipment.csv -> equipment_smelter` |
| 3 | 전력 경고가 뜨면 뭘 봐야 해? | `troubleshooting_rules.csv -> issue_no_power` |
| 4 | 장비 입력 슬롯이 비어 있으면 어떻게 해? | `troubleshooting_rules.csv -> issue_no_input` |
| 5 | 출력 슬롯이 꽉 차서 생산이 멈췄어. | `troubleshooting_rules.csv -> issue_output_full` |
| 6 | 컨베이어에서 자원이 안 움직여. | `troubleshooting_rules.csv -> issue_conveyor_blocked` |
| 7 | 예상한 결과물이랑 다른 자원이 나왔어. | `troubleshooting_rules.csv -> issue_unintended_output` |
| 8 | 기어가 계속 부족한데 어디를 봐야 해? | `troubleshooting_rules.csv -> issue_production_bottleneck` |

## 5. 발표 시연 추천 질문

시연에서는 아래 순서로 질문하면 구조 설명이 쉽다.

### 질문 1: 장비 설명

```text
제련기는 뭐 하는 장비야?
```

확인 포인트:
- `operator_guide.machine_help`로 분기되는지 확인
- CSV의 제련기 역할, 입력 자원, 출력 자원을 참고하는지 확인

### 질문 2: 레시피 설명

```text
기어 만들려면 어떤 재료와 장비가 필요해?
```

확인 포인트:
- `operator_guide.recipe_explainer`로 분기되는지 확인
- 철괴 2개, 조립기, 기어 제작 공정을 설명하는지 확인

### 질문 3: 문제 해결

```text
컨베이어가 멈췄는데 뭘 확인해야 해?
```

확인 포인트:
- `operator_guide.troubleshooter`로 분기되는지 확인
- 전력, 입력, 출력 저장 공간, 컨베이어, 레시피 순서로 확인을 안내하는지 확인
- 응답 metadata의 `llmModel`이 `gpt-5.4-nano`인지 확인

### 질문 4: 생산 병목

```text
기본 모터가 계속 부족한데 어디를 확인해야 해?
```

확인 포인트:
- 기어와 전선 생산 흐름을 함께 언급하는지 확인
- 입력 자원, 전력, 장비 수, 레시피 설정을 확인하라고 안내하는지 확인

### 질문 5: 모르는 질문

```text
우주 엘리베이터는 어떻게 업그레이드해?
```

확인 포인트:
- CSV 근거에 없는 내용을 지어내지 않는지 확인
- 현재 매뉴얼 근거만으로는 충분하지 않다고 안내하는지 확인

## 6. 실제 요청 JSON 템플릿

```json
{
  "type": "agent.request",
  "request_id": "manual-qa-demo-001",
  "session_id": "demo-session",
  "client_id": "unreal-client",
  "payload": {
    "question": "컨베이어가 멈췄는데 뭘 확인해야 해?"
  },
  "context": {
    "language": "ko",
    "mode": "prototype_demo"
  }
}
```

## 7. 기대 응답 확인 포인트

응답에서 아래 metadata를 확인한다.

```json
{
  "llmProvider": "openai",
  "llmModel": "gpt-5.4-nano",
  "selectedAgent": "operator_guide",
  "selectedLeafAgent": "operator_guide.troubleshooter"
}
```

Unreal UI에는 아래 값만 표시하면 된다.

```text
payload.final_answer
```

