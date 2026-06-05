# Manual Q&A CSV 기반 질문/답변 예시

## 목적

이 문서는 Manual Q&A 프로토 단계에서 **5개 CSV 파일만으로 답할 수 있는 질문과 답변 예시**를 정리한다.

프로토 단계에서는 PostgreSQL, pgvector, Embedding, Markdown RAG, LLM Judge를 사용하지 않는다.  
아래 CSV에 들어 있는 데이터만 직접 조회해서 답변한다.

```text
data/game/equipment.csv
data/game/resources.csv
data/game/recipes.csv
data/game/troubleshooting_rules.csv
data/game/action_policy.csv
```

## 답변 가능 범위

현재 CSV 기준으로 답변 가능한 범위는 다음과 같다.

```text
장비 6개
- 채굴기
- 제련기
- 조립기
- 발전기
- 컨베이어
- 저장고

자원 8개
- 철광석
- 철괴
- 구리광석
- 구리괴
- 기어
- 전선
- 전력
- 기본 모터

레시피 5개
- 철괴 제작 공정
- 구리괴 제작 공정
- 기어 제작 공정
- 전선 제작 공정
- 기본 모터 제작 공정

문제 해결 규칙 7개
- 장비가 멈췄을 때
- 전력 부족
- 입력 자원 부족
- 출력 공간 부족
- 컨베이어 막힘
- 의도하지 않은 출력이 나왔을 때
- 생산 병목
```

## 장비 질문

### 제련기는 뭐야?

```text
Q. 제련기는 뭐야?
A. 제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다.
   입력 자원은 철광석, 구리광석이고, 출력 자원은 철괴, 구리괴입니다.
   필요 전력은 10입니다.
```

출처:

```text
equipment.csv -> equipment_smelter
```

추천 행동:

```text
action_explain_equipment_role
```

### 채굴기는 뭐야?

```text
Q. 채굴기는 뭐야?
A. 채굴기는 원석 자원을 채굴하는 기본 생산 장비입니다.
   출력 자원은 철광석, 구리광석입니다.
   필요 전력은 5입니다.
```

출처:

```text
equipment.csv -> equipment_miner
```

### 조립기는 뭐야?

```text
Q. 조립기는 뭐야?
A. 조립기는 가공 자원을 조립해 부품을 제작하는 장비입니다.
   입력 자원은 철괴, 구리괴, 기어, 전선이고,
   출력 자원은 기어, 전선, 기본 모터입니다.
   필요 전력은 15입니다.
```

출처:

```text
equipment.csv -> equipment_assembler
```

### 발전기는 뭐야?

```text
Q. 발전기는 뭐야?
A. 발전기는 공장 설비에 전력을 공급하는 장비입니다.
   입력 자원은 resource_fuel이고, 출력 자원은 전력입니다.
```

출처:

```text
equipment.csv -> equipment_generator
```

### 컨베이어는 뭐야?

```text
Q. 컨베이어는 뭐야?
A. 컨베이어는 자원을 장비 사이로 운반하는 물류 장비입니다.
```

출처:

```text
equipment.csv -> equipment_conveyor
```

### 저장고는 뭐야?

```text
Q. 저장고는 뭐야?
A. 저장고는 자원을 임시 저장하는 장비입니다.
```

출처:

```text
equipment.csv -> equipment_storage
```

## 자원 생산 질문

### 철괴는 어떻게 만들어?

```text
Q. 철괴는 어떻게 만들어?
A. 철괴는 철광석을 제련기에서 제련해서 만듭니다.
   관련 공정은 철괴 제작 공정입니다.
```

출처:

```text
resources.csv -> resource_iron_ingot
recipes.csv -> recipe_iron_ingot
```

추천 행동:

```text
action_explain_resource_production
```

### 구리괴는 어떻게 만들어?

```text
Q. 구리괴는 어떻게 만들어?
A. 구리괴는 구리광석을 제련기에서 제련해서 만듭니다.
   관련 공정은 구리괴 제작 공정입니다.
```

출처:

```text
resources.csv -> resource_copper_ingot
recipes.csv -> recipe_copper_ingot
```

### 기어는 어떻게 만들어?

```text
Q. 기어는 어떻게 만들어?
A. 기어는 철괴를 조립기에서 가공해서 만듭니다.
   기본 모터 제작에 사용됩니다.
```

출처:

```text
resources.csv -> resource_gear
recipes.csv -> recipe_gear
```

### 전선은 어떻게 만들어?

```text
Q. 전선은 어떻게 만들어?
A. 전선은 구리괴를 조립기에서 가공해서 만듭니다.
   기본 모터 제작에 사용됩니다.
```

출처:

```text
resources.csv -> resource_wire
recipes.csv -> recipe_wire
```

### 기본 모터는 어떻게 만들어?

```text
Q. 기본 모터는 어떻게 만들어?
A. 기본 모터는 기어와 전선을 조립기에서 조립해서 만듭니다.
```

출처:

```text
resources.csv -> resource_basic_motor
recipes.csv -> recipe_basic_motor
```

### 전력은 어떻게 만들어?

```text
Q. 전력은 어떻게 만들어?
A. 전력은 발전기에서 공급합니다.
   생산 장비와 물류 장비 가동에 사용됩니다.
```

출처:

```text
resources.csv -> resource_power
equipment.csv -> equipment_generator
```

## 레시피 질문

### 철괴 만들려면 뭐가 필요해?

```text
Q. 철괴 만들려면 뭐가 필요해?
A. 철괴 제작에는 철광석 1개가 필요합니다.
   필요 장비는 제련기이고,
   공정 순서는 철광석 채굴 > 제련기 투입 > 철괴 출력입니다.
```

출처:

```text
recipes.csv -> recipe_iron_ingot
```

추천 행동:

```text
action_explain_recipe_requirements
```

### 구리괴 만들려면 뭐가 필요해?

```text
Q. 구리괴 만들려면 뭐가 필요해?
A. 구리괴 제작에는 구리광석 1개가 필요합니다.
   필요 장비는 제련기이고,
   공정 순서는 구리광석 채굴 > 제련기 투입 > 구리괴 출력입니다.
```

출처:

```text
recipes.csv -> recipe_copper_ingot
```

### 기어 만들려면 뭐가 필요해?

```text
Q. 기어 만들려면 뭐가 필요해?
A. 기어 제작에는 철괴 2개가 필요합니다.
   필요 장비는 조립기이고,
   공정 순서는 철광석 채굴 > 철괴 제련 > 조립기에서 기어 제작입니다.
```

출처:

```text
recipes.csv -> recipe_gear
resources.csv -> resource_iron_ingot
```

### 전선 만들려면 뭐가 필요해?

```text
Q. 전선 만들려면 뭐가 필요해?
A. 전선 제작에는 구리괴 1개가 필요합니다.
   필요 장비는 조립기이고,
   공정 순서는 구리광석 채굴 > 구리괴 제련 > 조립기에서 전선 제작입니다.
```

출처:

```text
recipes.csv -> recipe_wire
resources.csv -> resource_copper_ingot
```

### 기본 모터 만들려면 뭐가 필요해?

```text
Q. 기본 모터 만들려면 뭐가 필요해?
A. 기본 모터 제작에는 기어 1개와 전선 2개가 필요합니다.
   필요 장비는 조립기이고,
   공정 순서는 철괴로 기어 제작 > 구리괴로 전선 제작 > 조립기에서 기본 모터 제작입니다.
```

출처:

```text
recipes.csv -> recipe_basic_motor
resources.csv -> resource_gear
resources.csv -> resource_wire
```

## 문제 해결 질문

### 제련기가 왜 안 돌아가?

```text
Q. 제련기가 왜 안 돌아가?
A. 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인해야 합니다.
```

출처:

```text
troubleshooting_rules.csv -> issue_machine_stopped
equipment.csv -> equipment_smelter
```

추천 행동:

```text
action_check_power
action_check_input_resource
action_check_storage
action_check_conveyor
action_set_recipe
```

### 전력이 부족하면 어떻게 해야 해?

```text
Q. 전력이 부족하면 어떻게 해야 해?
A. 발전기 가동 여부, 연료, 전력 연결 상태를 확인해야 합니다.
```

출처:

```text
troubleshooting_rules.csv -> issue_no_power
```

추천 행동:

```text
action_check_power
action_check_generator
action_check_fuel
```

주의:

```text
현재 action_policy.csv에는 action_check_generator, action_check_fuel이 없다.
프로토에서 실제 추천 행동으로 안정적으로 반환하려면 action_policy.csv에 추가하거나,
troubleshooting_rules.csv의 recommended_action_ids를 현재 action_policy.csv에 존재하는 ID로 맞춰야 한다.
```

### 입력 자원이 부족하면 뭘 확인해야 해?

```text
Q. 입력 자원이 부족하면 뭘 확인해야 해?
A. 입력 자원이 충분한지, 공급 장비와 컨베이어가 정상인지 확인해야 합니다.
```

출처:

```text
troubleshooting_rules.csv -> issue_no_input
```

추천 행동:

```text
action_check_input_resource
action_check_conveyor
action_check_upstream_machine
```

주의:

```text
현재 action_policy.csv에는 action_check_upstream_machine이 없다.
```

### 저장고가 꽉 차면 어떻게 해?

```text
Q. 저장고가 꽉 차면 어떻게 해?
A. 출력 저장 공간을 비우거나 저장고와 컨베이어 연결을 확인해야 합니다.
```

출처:

```text
troubleshooting_rules.csv -> issue_output_full
```

추천 행동:

```text
action_check_storage
action_check_conveyor
```

### 컨베이어가 막혔을 때 뭘 확인해?

```text
Q. 컨베이어가 막혔을 때 뭘 확인해?
A. 컨베이어 방향, 연결 상태, 목적지 저장 공간을 확인해야 합니다.
```

출처:

```text
troubleshooting_rules.csv -> issue_conveyor_blocked
```

추천 행동:

```text
action_check_conveyor
action_check_storage
```

### 생산 병목이 생기면 뭘 확인해야 해?

```text
Q. 생산 병목이 생기면 뭘 확인해야 해?
A. 부족한 자원 라인, 전력 상태, 장비 수, 레시피 설정과 실제 출력물을 확인해야 합니다.
```

출처:

```text
troubleshooting_rules.csv -> issue_production_bottleneck
```

추천 행동:

```text
action_check_input_resource
action_check_power
action_check_machine_count
action_set_recipe
```

주의:

```text
현재 action_policy.csv에는 action_check_machine_count가 없다.
```

## 모르는 질문

### 우주 엘리베이터는 어떻게 업그레이드해?

```text
Q. 우주 엘리베이터는 어떻게 업그레이드해?
A. 현재 매뉴얼 데이터에서 확인할 수 없습니다.
   프로토는 장비, 자원, 레시피, 문제 해결 CSV에 있는 내용만 답변합니다.
```

출처:

```text
없음
```

추천 행동:

```text
action_answer_unknown_without_guessing
```

## 프로토 한계

현재 프로토는 CSV에 있는 정보만 답변한다.

```text
가능:
- CSV에 있는 장비 설명
- CSV에 있는 자원 생산 방법
- CSV에 있는 레시피 요구사항
- CSV에 있는 문제 해결 규칙
- action_policy.csv에 존재하는 추천 행동 반환

불가능:
- CSV에 없는 장비/자원/레시피 답변
- 복잡한 player_state 분석
- Markdown RAG 검색
- LLM 추론
- 실제 Unreal action 실행
```

## 정리

현재 5개 CSV만으로도 Manual Q&A 프로토의 기본 흐름은 검증할 수 있다.

```text
질문
-> 질문 유형 분류
-> CSV 직접 조회
-> 템플릿 답변 생성
-> sources와 recommended_actions metadata 반환
```

다만 문제 해결 CSV의 일부 `recommended_action_ids`는 현재 `action_policy.csv`에 없는 ID를 참조한다.  
프로토 smoke test에서 사용하는 대표 질문은 통과하지만, 더 넓은 문제 해결 질문까지 안정적으로 답하려면 action policy 정합성을 추가로 맞추는 것이 좋다.
