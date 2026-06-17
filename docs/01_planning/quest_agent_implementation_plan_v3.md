# Quest Agent 독립 구현 기획서

## 1. 목적

이 문서는 Factory-Space 프로젝트에서 **퀘스트 에이전트만 독립적으로 구현**하기 위한 기획서다.

기존 전체 구조에서는 다음 흐름을 전제로 했다.

```text
FactorySimulationEngine
→ FactorySnapshot
→ FactoryAnalyzer
→ FactoryInsight
→ QuestComposerAgent
```

하지만 퀘스트 에이전트만 먼저 구현할 경우 `FactorySimulationEngine`, `FactorySnapshot`, `FactoryAnalyzer`, `FactoryInsight`까지 만들면 범위가 커진다.

따라서 이번 구현에서는 `FactorySnapshot / FactoryInsight`를 사용하지 않는다.

대신 서버에 이미 존재하는 퀘스트 상태, 인벤토리, 생산 기록, 해금 정보, 공장 레벨을 모아 `QuestContext`를 만들고, 이 `QuestContext`를 기반으로 퀘스트를 생성한다.

```text
DB / 서버 상태 / 퀘스트 진행도
↓
QuestContextBuilder
↓
QuestContext
↓
QuestComposerAgent
↓
SupportQuestDraft
↓
QuestValidator
↓
QuestInstance
↓
QuestManager
```

---

## 2. 구현 범위

### 2.1 이번에 구현할 것

```text
QuestContextBuilder
QuestComposerAgent
QuestRuleGenerator
QuestValidator
QuestManager
QuestProgressTracker
QuestRewardResolver
LevelProgressionResolver
Quest API
Quest WebSocket Event
```

### 2.2 이번에 구현하지 않을 것

```text
FactorySimulationEngine
FactorySnapshotBuilder
FactoryAnalyzer
FactoryInsightGenerator
PowerAnalyzer
BottleneckAnalyzer
LogisticsAnalyzer
StorageAnalyzer
공장 최적화 Agent
좌표 기반 배치 검증
전체 공장 시뮬레이션
```

---

## 3. 핵심 방향

퀘스트 에이전트는 공장 전체를 직접 분석하지 않는다.

퀘스트 에이전트는 다음 정보만 사용한다.

```text
현재 메인 퀘스트
공장 레벨
현재 보유 아이템
메인 퀘스트 요구 아이템
최근 생산 기록
해금된 기계
해금된 레시피
완료한 퀘스트
진행 중인 퀘스트
최근 실패 / 정체 정보
```

즉, 퀘스트 에이전트의 입력은 `FactorySnapshot`이 아니라 `QuestContext`다.

---

## 4. 전체 구조

```text
Quest System
├─ QuestContextBuilder
├─ QuestComposerAgent
├─ QuestRuleGenerator
├─ QuestValidator
├─ QuestManager
├─ QuestProgressTracker
├─ QuestRewardResolver
└─ QuestEventPublisher
```

### 역할 요약

| 모듈 | 역할 |
|---|---|
| `QuestContextBuilder` | 서버 상태를 모아 QuestContext 생성 |
| `QuestComposerAgent` | QuestContext를 보고 지원 퀘스트 초안 생성 |
| `QuestRuleGenerator` | 룰 기반으로 목표 / 수량 / 타입 결정 |
| `QuestValidator` | 퀘스트가 유효한지 검증 |
| `QuestManager` | 퀘스트 지급 / 상태 변경 / 완료 처리 |
| `QuestProgressTracker` | 서버 이벤트 기반 진행도 갱신 |
| `QuestRewardResolver` | 보상 계산 / 지급 |
| `LevelProgressionResolver` | 메인 퀘스트 완료 시 공장 레벨 증가 처리 |
| `QuestEventPublisher` | WebSocket 이벤트 발행 |

---

## 5. QuestContextBuilder

`QuestContextBuilder`는 퀘스트 에이전트 입력을 만드는 모듈이다.

이 모듈은 공장 전체를 분석하지 않는다.  
이미 서버에 있는 상태 데이터를 읽어서 퀘스트 생성에 필요한 최소 형태로 정리한다.

### 5.1 입력 데이터

```text
Factory
Current Main Quest
Quest Progress
Inventory
Production History
Unlocked Machines
Unlocked Recipes
Completed Quest History
Active Support Quests
Factory Level
```

### 5.2 출력 데이터

```text
QuestContext
```

### 5.3 QuestContext 예시

```json
{
  "factory_id": "factory_001",
  "factory_level": 2,
  "current_main_quest": {
    "quest_id": "main_commtower",
    "title": "통신탑 건설",
    "objectives": [
      {
        "objective_id": "need_iron_ingot",
        "objective_type": "collect_item",
        "item_id": "iron_ingot",
        "required": 20,
        "current": 8
      },
      {
        "objective_id": "need_copper_wire",
        "objective_type": "collect_item",
        "item_id": "copper_wire",
        "required": 10,
        "current": 2
      }
    ]
  },
  "inventory": {
    "iron_ore": 120,
    "iron_ingot": 8,
    "copper_ore": 80,
    "copper_wire": 2
  },
  "recent_production": {
    "iron_ingot": 4,
    "copper_wire": 1
  },
  "unlocked_machines": [
    "Miner_Lv1",
    "Smelter_Lv1",
    "Assembler_Lv1"
  ],
  "unlocked_recipes": [
    "smelt_iron",
    "smelt_copper",
    "make_copper_wire"
  ],
  "active_support_quest_ids": [],
  "completed_quest_ids": [
    "tutorial_001",
    "tutorial_002",
    "main_001"
  ],
  "known_issues": [
    "iron_ingot_shortage",
    "copper_wire_shortage"
  ]
}
```

---

## 6. QuestContext 생성 규칙

`QuestContextBuilder`는 다음 방식으로 `known_issues`를 만든다.

### 6.1 부족 아이템 판단

메인 퀘스트 목표에서 `required > current`이면 부족으로 본다.

```text
required - current = shortage_amount
```

예:

```json
{
  "item_id": "iron_ingot",
  "required": 20,
  "current": 8,
  "shortage_amount": 12
}
```

생성되는 issue:

```text
iron_ingot_shortage
```

### 6.2 생산 가능 여부 판단

부족 아이템을 생산할 수 있는 레시피가 해금되어 있으면 생산 지원 퀘스트 후보가 된다.

```text
iron_ingot 부족
→ smelt_iron 해금됨
→ produce_item 퀘스트 가능
```

### 6.3 생산 불가능 여부 판단

부족 아이템을 생산할 수 있는 레시피가 해금되어 있지 않으면 해금 / 준비 퀘스트 후보가 된다.

```text
copper_wire 부족
→ make_copper_wire 미해금
→ unlock_recipe 또는 prepare_machine 퀘스트 후보
```

### 6.4 이미 진행 중인 지원 퀘스트 중복 방지

같은 아이템 / 같은 목적의 지원 퀘스트가 이미 있으면 새로 만들지 않는다.

```text
active_support_quest 중 iron_ingot 생산 퀘스트 존재
→ iron_ingot 지원 퀘스트 생성 금지
```

---

## 7. QuestComposerAgent

`QuestComposerAgent`는 `QuestContext`를 받아 지원 퀘스트 초안을 만든다.

### 7.1 입력

```python
QuestContext
```

### 7.2 출력

```python
SupportQuestDraft
```

### 7.3 역할

```text
1. 현재 메인 퀘스트 확인
2. 부족한 목표 확인
3. 부족한 아이템 확인
4. 생산 가능한지 확인
5. 해금된 기계 / 레시피 확인
6. 지원 퀘스트 타입 선택
7. 목표 수량 결정
8. 제목 / 설명 생성
9. 보상 후보 생성
10. QuestValidator에 전달
```

---

## 8. QuestComposerAgent는 FactoryInsight를 받지 않는다

기존 구조:

```python
def compose_quest(factory_insight: FactoryInsight) -> SupportQuestDraft:
    ...
```

수정 구조:

```python
def compose_quest(context: QuestContext) -> SupportQuestDraft:
    ...
```

이렇게 해야 퀘스트 에이전트만 독립 구현 가능하다.

---

## 9. 퀘스트 생성 타입

초기 구현에서는 아래 타입만 사용한다.

| quest_type | 설명 |
|---|---|
| `produce_item` | 특정 아이템 생산 |
| `collect_item` | 특정 아이템 보유량 달성 |
| `craft_item` | 특정 제작품 제작 |
| `setup_machine` | 특정 설비 설치 / 등록 |
| `connect_process` | 생산 흐름 연결 |
| `stabilize_goal` | 메인 목표 달성을 위한 묶음형 지원 퀘스트 |

---

## 10. 지원 퀘스트 생성 규칙

### 10.1 produce_item

조건:

```text
메인 퀘스트에 필요한 아이템이 부족함
해당 아이템 생산 레시피가 해금됨
생산 설비가 해금됨
동일 지원 퀘스트가 진행 중이 아님
```

예:

```json
{
  "quest_type": "support",
  "support_type": "produce_item",
  "title": "통신탑 재료 확보",
  "description": "통신탑 건설에 필요한 철 주괴를 추가로 생산하세요.",
  "objectives": [
    {
      "objective_type": "produce_item",
      "item_id": "iron_ingot",
      "target_amount": 12
    }
  ],
  "related_main_quest_id": "main_commtower"
}
```

---

### 10.2 collect_item

조건:

```text
필요 아이템이 부족함
이미 생산된 아이템이 여러 저장소에 흩어져 있음
또는 단순 보유량 달성이 목적임
```

예:

```json
{
  "quest_type": "support",
  "support_type": "collect_item",
  "title": "부품 재고 정리",
  "description": "통신탑 건설에 필요한 부품을 확보하세요.",
  "objectives": [
    {
      "objective_type": "collect_item",
      "item_id": "copper_wire",
      "target_amount": 10
    }
  ],
  "related_main_quest_id": "main_commtower"
}
```

---

### 10.3 setup_machine

조건:

```text
필요 아이템 생산 레시피는 있음
하지만 필요한 기계가 공장에 없음
```

예:

```json
{
  "quest_type": "support",
  "support_type": "setup_machine",
  "title": "제련 라인 준비",
  "description": "철 주괴 생산을 위해 제련기를 준비하세요.",
  "objectives": [
    {
      "objective_type": "register_machine",
      "machine_name": "Smelter_Lv1",
      "target_count": 1
    }
  ],
  "related_main_quest_id": "main_commtower"
}
```

---

### 10.4 connect_process

조건:

```text
필요 기계가 있음
하지만 생산 흐름 연결이 안 되어 있음
```

예:

```json
{
  "quest_type": "support",
  "support_type": "connect_process",
  "title": "철 생산 흐름 연결",
  "description": "채굴기에서 제련기로 철 광석이 전달되도록 생산 흐름을 연결하세요.",
  "objectives": [
    {
      "objective_type": "connect_ports",
      "from_machine_type": "Miner",
      "to_machine_type": "Smelter",
      "item_id": "iron_ore"
    }
  ],
  "related_main_quest_id": "main_commtower"
}
```

---

### 10.5 stabilize_goal

조건:

```text
부족 아이템이 여러 개임
또는 메인 퀘스트 진행을 위한 준비가 여러 단계임
```

예:

```json
{
  "quest_type": "support",
  "support_type": "stabilize_goal",
  "title": "통신탑 재료 생산 라인 안정화",
  "description": "통신탑 건설에 필요한 핵심 재료 생산 흐름을 정리하세요.",
  "objectives": [
    {
      "objective_type": "produce_item",
      "item_id": "iron_ingot",
      "target_amount": 12
    },
    {
      "objective_type": "produce_item",
      "item_id": "copper_wire",
      "target_amount": 8
    }
  ],
  "related_main_quest_id": "main_commtower"
}
```

---

## 11. LLM 사용 범위

초기 구현에서는 LLM을 판단 주체로 쓰지 않는다.

```text
룰 기반
= 퀘스트 타입, 목표, 수량, 보상 결정

LLM
= 제목, 설명, 몰입감 있는 문장 보강
```

### 11.1 LLM에 맡기지 않는 것

```text
퀘스트 완료 조건
보상 수량
해금 조건
아이템 수량
메인 퀘스트 진행 판정
밸런스 수치
```

### 11.2 LLM에 맡길 수 있는 것

```text
퀘스트 제목 후보
퀘스트 설명 문장
NPC 안내 대사
몰입감 있는 목표 설명
```

### 11.3 LLM 출력도 Validator를 통과해야 함

LLM이 만든 문장은 다음 검증을 거친다.

```text
금지 아이템 언급 없음
존재하지 않는 기계 언급 없음
존재하지 않는 레시피 언급 없음
목표 수량 변경 없음
보상 변경 없음
```

---

## 12. QuestValidator

`QuestValidator`는 생성된 퀘스트가 실제 게임 상태에서 유효한지 검증한다.

### 12.1 검증 항목

```text
1. related_main_quest_id가 존재하는가
2. target item_id가 ItemTable에 존재하는가
3. objective_type이 허용된 타입인가
4. target_amount가 0보다 큰가
5. target_amount가 과도하지 않은가
6. 필요한 레시피가 해금되어 있는가
7. 필요한 기계가 해금되어 있는가
8. 동일 목적의 지원 퀘스트가 이미 진행 중인가
9. 보상이 허용 범위 안인가
10. 메인 퀘스트와 직접 관련이 있는가
```

### 12.2 실패 시 처리

```text
검증 실패
→ 퀘스트 생성 취소
→ 실패 사유 로그 저장
→ 필요 시 fallback rule 실행
```

예:

```json
{
  "valid": false,
  "reason": "duplicate_support_quest",
  "message": "동일한 iron_ingot 생산 지원 퀘스트가 이미 진행 중입니다."
}
```

---

## 13. QuestManager

`QuestManager`는 퀘스트 인스턴스를 관리한다.

### 13.1 역할

```text
퀘스트 지급
퀘스트 상태 변경
퀘스트 진행도 저장
퀘스트 완료 처리
보상 수령 처리
다음 메인 퀘스트 활성화
메인 퀘스트 완료 시 공장 레벨 +1 처리
```

### 13.2 Quest 상태

| status | 설명 |
|---|---|
| `locked` | 아직 잠김 |
| `available` | 받을 수 있음 |
| `in_progress` | 진행 중 |
| `completed` | 완료됨 |
| `reward_claimed` | 보상 수령 완료 |
| `failed` | 실패 처리됨 |
| `expired` | 만료됨 |

---

## 14. QuestProgressTracker

`QuestProgressTracker`는 서버 이벤트를 받아 퀘스트 진행도를 갱신한다.

### 14.1 입력 이벤트

```text
item_produced
item_collected
machine_registered
recipe_set
ports_connected
node_connected
power_connected
quest_reward_claimed
```

### 14.2 이벤트 예시

```json
{
  "event_type": "item_produced",
  "factory_id": "factory_001",
  "item_id": "iron_ingot",
  "amount": 1,
  "source_entity_id": "smelter_001"
}
```

### 14.3 진행도 갱신 예시

```text
QuestObjective:
produce iron_ingot 12개

item_produced iron_ingot 1개 발생
→ current_amount +1
```

---

## 15. QuestRewardResolver

보상은 룰 기반으로 결정한다.

### 15.1 보상 타입

| reward_type | 설명 |
|---|---|
| `item` | 아이템 지급 |
| `unlock_recipe` | 레시피 해금 |
| `unlock_machine` | 기계 해금 |
| `factory_level_up` | 공장 레벨 증가 |
| `currency` | 재화 지급 |

### 15.2 지원 퀘스트 보상 원칙

지원 퀘스트는 메인 퀘스트보다 보상이 작아야 한다.

```text
메인 퀘스트
= 공장 성장 / 레벨업 / 주요 해금

지원 퀘스트
= 부족한 자원 보조 / 소량 재화 / 소모품
```

---

## 16. 메인 퀘스트와 레벨 규칙

기존 결정사항을 유지한다.

```text
튜토리얼 완료 = 공장 레벨 Lv.1
메인 퀘스트 하나 완료 = 공장 레벨 +1
서브 / 지원 퀘스트 완료 = 공장 레벨 증가 없음
```

예:

| 진행 상태 | 공장 레벨 |
|---|---:|
| 튜토리얼 진행 중 | 없음 |
| 튜토리얼 완료 | Lv.1 |
| 메인 퀘스트 1개 완료 | Lv.2 |
| 메인 퀘스트 2개 완료 | Lv.3 |

### 16.1 레벨링 구현 기준

레벨링은 `QuestComposerAgent`가 처리하지 않는다.  
`QuestComposerAgent`는 현재 공장 레벨을 `QuestContext`에서 참고만 한다.

공장 레벨 변경은 `QuestManager`와 `LevelProgressionResolver`가 처리한다.

```text
QuestProgressTracker
→ 메인 퀘스트 완료 감지

QuestManager
→ 완료된 퀘스트가 main 타입인지 확인

LevelProgressionResolver
→ main 퀘스트 완료 1회당 factory_level +1 계산

FactoryStateStore
→ factory_level 저장

QuestManager
→ 다음 메인 퀘스트 활성화
```

### 16.2 레벨업 규칙

```text
튜토리얼 전체 완료
= Factory Level Lv.1 부여

main 퀘스트 1개 완료
= Factory Level +1

support / sub 퀘스트 완료
= Factory Level 변화 없음
```

예시:

```text
튜토리얼 완료
→ Lv.1

main_commtower 완료
→ Lv.2

main_signal_booster 완료
→ Lv.3
```

### 16.3 LevelProgressionResolver 역할

```text
1. 완료된 quest_instance 확인
2. quest_type이 main인지 확인
3. 이미 레벨업 처리된 퀘스트인지 확인
4. main 퀘스트 완료라면 factory_level +1
5. 다음 main quest 활성화
6. level_up 이벤트 발행
```

### 16.4 중복 레벨업 방지

같은 메인 퀘스트 완료 이벤트가 여러 번 처리되면 안 된다.

따라서 `quest_instance` 또는 별도 로그에 레벨업 처리 여부를 남긴다.

```json
{
  "quest_instance_id": "qinst_main_commtower_001",
  "quest_type": "main",
  "status": "completed",
  "level_reward_applied": true
}
```

---

## 17. API 엔드포인트

퀘스트 에이전트 구현에 필요한 엔드포인트만 정리한다.

### 17.1 퀘스트 목록 조회

```http
GET /api/factories/{factory_id}/quests
```

### 17.2 현재 메인 퀘스트 조회

```http
GET /api/factories/{factory_id}/quests/current-main
```

### 17.3 지원 퀘스트 생성 요청

개발 / 테스트용으로 사용한다.  
운영에서는 서버 내부 스케줄러나 조건에 따라 호출해도 된다.

```http
POST /api/factories/{factory_id}/quests/compose-support
```

요청:

```json
{
  "reason": "manual_test"
}
```

응답:

```json
{
  "success": true,
  "quest_instance": {
    "quest_instance_id": "qinst_001",
    "quest_type": "support",
    "title": "통신탑 재료 확보",
    "status": "in_progress"
  }
}
```

### 17.4 퀘스트 보상 수령

기존 command 엔드포인트를 사용한다.

```http
POST /api/factories/{factory_id}/commands
```

```json
{
  "action_type": "claim_quest_reward",
  "payload": {
    "quest_instance_id": "qinst_001"
  }
}
```

---

## 18. WebSocket 이벤트

퀘스트 관련 이벤트만 정리한다.

### 18.1 퀘스트 생성

```json
{
  "message_type": "event",
  "event_type": "quest_created",
  "factory_id": "factory_001",
  "payload": {
    "quest_instance_id": "qinst_001",
    "quest_type": "support",
    "title": "통신탑 재료 확보"
  }
}
```

### 18.2 퀘스트 진행도 갱신

```json
{
  "message_type": "event",
  "event_type": "quest_progress_updated",
  "factory_id": "factory_001",
  "payload": {
    "quest_instance_id": "qinst_001",
    "objective_id": "produce_iron_ingot",
    "current": 8,
    "target": 12
  }
}
```

### 18.3 퀘스트 완료

```json
{
  "message_type": "event",
  "event_type": "quest_completed",
  "factory_id": "factory_001",
  "payload": {
    "quest_instance_id": "qinst_001",
    "quest_id": "support_001"
  }
}
```

### 18.4 보상 수령 완료

```json
{
  "message_type": "event",
  "event_type": "quest_reward_claimed",
  "factory_id": "factory_001",
  "payload": {
    "quest_instance_id": "qinst_001",
    "rewards": [
      {
        "reward_type": "item",
        "item_id": "iron_ore",
        "amount": 20
      }
    ]
  }
}
```

---

## 19. DB 저장 구조

### 19.1 quest_master

메인 / 튜토리얼 / 고정 서브 퀘스트 정의 테이블.

| 필드 | 설명 |
|---|---|
| `quest_id` | 퀘스트 마스터 ID |
| `quest_type` | tutorial / main / support |
| `title` | 제목 |
| `description` | 설명 |
| `objective_json` | 목표 정의 |
| `reward_json` | 보상 정의 |
| `unlock_condition_json` | 해금 조건 |

---

### 19.2 quest_instance

유저 공장에 실제 지급된 퀘스트.

| 필드 | 설명 |
|---|---|
| `quest_instance_id` | 인스턴스 ID |
| `factory_id` | 공장 ID |
| `quest_id` | 마스터 ID 또는 generated ID |
| `quest_type` | main / support |
| `related_main_quest_id` | 연결된 메인 퀘스트 |
| `title` | 제목 |
| `description` | 설명 |
| `status` | in_progress / completed 등 |
| `objective_json` | 목표 |
| `reward_json` | 보상 |
| `created_by` | rule / llm / system |
| `created_at` | 생성 시간 |
| `completed_at` | 완료 시간 |
| `level_reward_applied` | 메인 퀘스트 레벨 보상 적용 여부 |

---

### 19.3 quest_progress

퀘스트 목표별 진행도.

| 필드 | 설명 |
|---|---|
| `quest_progress_id` | 진행도 ID |
| `quest_instance_id` | 퀘스트 인스턴스 ID |
| `objective_id` | 목표 ID |
| `objective_type` | produce_item 등 |
| `target_id` | item_id / machine_name 등 |
| `current_amount` | 현재 수량 |
| `target_amount` | 목표 수량 |
| `status` | in_progress / completed |

---

### 19.4 quest_generation_log

퀘스트 생성 기록.

| 필드 | 설명 |
|---|---|
| `log_id` | 로그 ID |
| `factory_id` | 공장 ID |
| `related_main_quest_id` | 관련 메인 퀘스트 |
| `context_json` | QuestContext |
| `draft_json` | 생성 초안 |
| `validation_result_json` | 검증 결과 |
| `created_by` | rule / llm |
| `created_at` | 생성 시간 |

---

## 20. 코드 구조 예시

```text
server/
├─ app/
│  ├─ api/
│  │  ├─ quest_routes.py
│  │  └─ command_routes.py
│  │
│  ├─ domain/
│  │  └─ quest/
│  │     ├─ context_builder.py
│  │     ├─ composer_agent.py
│  │     ├─ rule_generator.py
│  │     ├─ validator.py
│  │     ├─ manager.py
│  │     ├─ progress_tracker.py
│  │     ├─ reward_resolver.py
│  │     ├─ events.py
│  │     └─ models.py
│  │
│  ├─ db/
│  │  └─ quest_repository.py
│  │
│  └─ data/
│     ├─ quest_table_loader.py
│     ├─ item_table_loader.py
│     ├─ machine_table_loader.py
│     └─ recipe_table_loader.py
```

---

## 21. 구현 순서

### 1단계: 데이터 모델

```text
QuestContext
SupportQuestDraft
QuestObjective
QuestReward
QuestInstance
QuestProgress
ValidationResult
```

### 2단계: QuestContextBuilder

```text
현재 메인 퀘스트 조회
인벤토리 조회
목표 부족분 계산
해금 기계 / 레시피 조회
진행 중 지원 퀘스트 조회
QuestContext 생성
```

### 3단계: RuleGenerator

```text
부족 아이템 기반 support quest 생성
생산 가능 여부 판단
목표 수량 결정
지원 퀘스트 타입 결정
```

### 4단계: QuestValidator

```text
아이템 존재 검증
레시피 존재 검증
해금 여부 검증
중복 퀘스트 검증
목표 수량 검증
보상 검증
```

### 5단계: QuestManager

```text
QuestInstance 생성
QuestProgress 생성
상태 변경
완료 처리
보상 처리
```

### 6단계: QuestProgressTracker

```text
item_produced 이벤트 처리
machine_registered 이벤트 처리
ports_connected 이벤트 처리
진행도 갱신
완료 여부 확인
```

### 7단계: WebSocket Event

```text
quest_created
quest_progress_updated
quest_completed
quest_reward_claimed
```

### 8단계: LLM 문장 보강

```text
RuleGenerator 결과를 기반으로 제목 / 설명만 보강
수량 / 보상 / 완료 조건은 변경 금지
```

---

## 22. 최소 구현 버전

가장 먼저 만들 최소 버전은 다음과 같다.

```text
1. 현재 메인 퀘스트의 required_items 확인
2. 현재 inventory와 비교
3. 부족한 item 하나 선택
4. 해당 item 생산 레시피가 해금되어 있으면 produce_item 지원 퀘스트 생성
5. QuestValidator 통과
6. QuestInstance 저장
7. item_produced 이벤트로 진행도 갱신
8. 목표 수량 도달 시 completed 처리
```

최소 버전 예시:

```json
{
  "quest_type": "support",
  "support_type": "produce_item",
  "title": "통신탑 재료 확보",
  "description": "통신탑 건설에 필요한 철 주괴를 추가로 생산하세요.",
  "objectives": [
    {
      "objective_type": "produce_item",
      "item_id": "iron_ingot",
      "target_amount": 12
    }
  ],
  "related_main_quest_id": "main_commtower"
}
```

---

## 23. 최종 결론

```text
FactorySnapshot / FactoryInsight는 전체 공장 분석 시스템 산출물이다.
퀘스트 에이전트만 구현할 때는 사용하지 않는다.

대신 QuestContextBuilder가 서버 상태를 모아 QuestContext를 만든다.
QuestComposerAgent는 QuestContext를 입력으로 받는다.

초기 구현은 Rule 기반으로 한다.
LLM은 제목 / 설명 / 안내 문장 보강에만 사용한다.

QuestValidator가 모든 퀘스트를 검증한다.
QuestManager가 지급 / 진행 / 완료 / 보상을 처리한다.
QuestProgressTracker가 서버 이벤트를 받아 진행도를 갱신한다.

이 구조면 공장 시뮬레이션 전체나 FactoryAnalyzer 없이도
퀘스트 에이전트만 독립적으로 구현할 수 있다.
```
