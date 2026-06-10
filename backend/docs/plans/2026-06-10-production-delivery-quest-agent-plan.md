# 생산/납품 동적 퀘스트 에이전트 계획

## 목표

퀘스트 에이전트가 더 이상 생산/경제/무역 분류를 다루지 않고, 매 요청마다 PostgreSQL의 현재 상황을 읽어 생산 퀘스트와 납품 퀘스트를 합산 5개 생성하도록 바꾼다.

성공 기준은 다음과 같다.

- 응답은 항상 `quests` 배열에 정확히 5개 퀘스트를 담는다.
- 퀘스트 타입은 `production`, `delivery` 두 가지만 사용한다.
- 기존 Unreal 수신 계약인 `id`, `type`, `title`, `description`, `objectives[target_item_id, quantity]`는 유지한다.
- 에이전트는 PostgreSQL 상황을 근거로 이미 충분한 아이템, 부족한 아이템, 최근 완료/생성된 퀘스트를 피해서 후보를 만든다.
- PostgreSQL 조회 실패나 후보 부족 상황에서도 검증 가능한 fallback으로 5개를 반환한다.

## 현재 구조 요약

- `backend/src/agents/quest_generator/service.py`는 서버 내부 예시 퀘스트 풀에서 5개를 뽑는다.
- `quest_generator.production_quest`는 LLM이 예시 ID 5개를 고르면 tool이 실제 `quests` JSON을 만든다.
- `quest_generator.economy_quest`는 별도 leaf agent지만, 현재 프론트가 기대하는 `quests` 배열과 다른 단일 `quest` payload를 반환한다.
- `frontend/Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`는 `quests` 배열을 읽고 창고 수량으로 완료 여부를 계산한다.
- `data/game/resources.csv`, `recipes.csv`, `equipment.csv`에는 생산 후보를 만들 근거 데이터가 이미 있다.

## 핵심 결정

### 퀘스트 타입

- `production`: 목표 아이템을 생산하거나 확보하는 퀘스트.
- `delivery`: 이미 가지고 있거나 곧 만들 수 있는 아이템을 지정 수량 납품하는 퀘스트.

`economy`, `trade`는 제거 대상이다. 과거 타입은 schema와 routing 테스트에서 더 이상 허용하지 않는다.

### 응답 계약

기존 프론트 파싱 비용을 줄이기 위해 두 타입 모두 같은 objective 구조를 쓴다.

```json
{
  "quests": [
    {
      "id": 1,
      "type": "production",
      "title": "철괴 5개 생산",
      "description": "제련기를 사용해 철괴 5개를 생산하세요.",
      "objectives": [
        {
          "target_item_id": "iron_ingot",
          "quantity": 5
        }
      ]
    }
  ]
}
```

납품 퀘스트도 `target_item_id`와 `quantity`를 사용한다. 완료 판정은 1차 구현에서 현재 창고 수량 기반으로 유지하고, 실제 아이템 차감은 별도 UI/완료 액션이 준비될 때 추가한다.

### PostgreSQL 상황 조회

agent가 PostgreSQL session에 직접 접근하지 않고 다음 경계를 둔다.

```text
ProductionDeliveryQuestAgent
  -> QuestAgentService
  -> QuestSituationRepository
  -> PostgreSQL
```

서비스가 필요로 하는 최소 상황은 다음이다.

- 현재 창고 보유량: `item_id`, `quantity`
- 최근 생성/완료된 퀘스트: `quest_type`, `target_item_id`, `created_at`, `completed_at`
- 현재 생산 가능성: 보유 장비 또는 해금된 레시피 목록
- 선택 사항: 공장 설비 상태, 막힌 출력, 전력 부족 같은 병목 정보

프로젝트에 아직 공용 PostgreSQL 스키마가 없다면, 이번 작업에서는 `QuestSituationRepository` 인터페이스와 테스트용 fake repository를 먼저 만들고 실제 PostgreSQL adapter는 같은 인터페이스 뒤에 붙인다. 이때 agent/service 코드는 repository 인터페이스만 바라보게 한다.

## 접근안

### 권장안: 규칙 기반 후보 생성 + LLM 선택/문장화

서비스가 PostgreSQL과 CSV 근거로 생산/납품 후보를 충분히 만든 뒤, LLM leaf agent가 상황에 맞는 5개를 고르고 문구를 다듬는다. tool 호출 결과가 최종 schema를 검증한다.

장점은 5개 개수, 타입 제한, 중복 제거를 코드로 보장할 수 있다는 점이다. LLM은 "왜 지금 이 퀘스트가 좋은지"를 반영하는 역할로 제한된다.

### 대안 1: 전부 규칙 기반 생성

LLM 없이 PostgreSQL 상태만 보고 5개를 만든다. 테스트와 재현성은 가장 좋지만, 퀘스트 문장이 단조롭고 상황 설명이 약해질 수 있다.

### 대안 2: LLM이 PostgreSQL 조회 결과를 보고 직접 5개 생성

구현은 빠르지만 5개 개수, 타입 제한, 없는 아이템 생성, 잘못된 objective shape 같은 실패가 늘어난다. 기존 tool 기반 파이프라인을 쓰는 장점도 줄어든다.

## 생성 규칙

### 생산 후보

생산 후보는 `recipes.csv`와 장비/자원 데이터를 근거로 만든다.

- 플레이어가 만들 수 있는 레시피만 후보로 둔다.
- 이미 창고에 많은 아이템은 우선순위를 낮춘다.
- 다음 레시피의 입력으로 쓰이는 아이템은 우선순위를 높인다.
- 최근 1~2회 생성된 같은 목표 아이템은 제외하거나 낮은 점수를 준다.
- 수량은 레시피 산출량과 현재 보유량을 기준으로 작게 시작한다.

예시 점수 기준:

- 보유량이 0이면 +30
- 다음 레시피 입력 재료면 +20
- 필요한 장비가 현재 사용 가능하면 +15
- 최근 같은 목표 퀘스트가 있으면 -40

### 납품 후보

납품 후보는 창고에 이미 있거나 생산 난도가 낮은 아이템을 중심으로 만든다.

- 현재 보유량이 최소 납품 수량 이상인 아이템을 우선한다.
- 너무 많이 쌓인 아이템은 납품 후보 우선순위를 높인다.
- 생산 체인의 핵심 중간재를 전부 소모시키는 과도한 수량은 피한다.
- 수량은 `max(1, floor(보유량 * 0.3))` 같은 보수적 값으로 시작하고 상한을 둔다.

예시:

- 철광석 30개 보유 -> 철광석 8개 납품
- 철괴 20개 보유 -> 철괴 5개 납품
- 창고에 0개인 아이템 -> 납품 후보 제외

## 5개 조합 규칙

기본 비율은 생산 3개, 납품 2개로 한다.

- 납품 후보가 2개 미만이면 생산 후보로 빈자리를 채운다.
- 생산 후보가 3개 미만이면 납품 후보로 빈자리를 채운다.
- 전체 후보가 5개 미만이면 CSV 기반 기본 생산 후보를 fallback으로 추가한다.
- 같은 `target_item_id`는 한 응답 안에서 한 번만 사용한다.

이 비율은 설정값으로 빼지 않는다. 요청 범위에서는 단순한 상수로 두고, 밸런싱 필요가 생기면 그때 설정화한다.

## 에이전트 구조 변경

### leaf agent

기존 leaf agent 두 개는 통합 leaf agent 하나로 정리한다.

- 신규 허용 leaf: `quest_generator.production_delivery_quest`
- 제거 대상: `quest_generator.production_quest`
- 제거 대상: `quest_generator.economy_quest`

확정 구조는 통합형 `quest_generator.production_delivery_quest` 하나다. 요청의 목표가 "5개 묶음 생성"이므로 생산과 납품을 서로 경쟁 후보로 점수화해야 한다. leaf를 둘로 나누면 최종 5개 조합 비율과 중복 제거를 다시 상위 계층에서 처리해야 한다.

### tool

LLM이 직접 퀘스트 JSON을 완성하지 않고 tool을 호출하게 한다.

```json
{
  "tool_call": {
    "name": "quest_generator.generate_production_delivery_quests",
    "args": {
      "production_count": 3,
      "delivery_count": 2
    }
  }
}
```

tool은 PostgreSQL 상황 조회, 후보 생성, 점수화, 5개 선택, schema 검증을 수행한다. LLM 응답이 실패하면 `QuestAgentService` fallback이 같은 생성기를 사용한다.

## 데이터 모델 초안

서비스 내부에서 사용할 모델은 PostgreSQL 테이블과 직접 묶지 않는다.

```text
QuestSituation
- warehouse_items: list[WarehouseItem]
- recent_quests: list[RecentQuest]
- available_recipe_ids: set[str]
- available_equipment_ids: set[str]

QuestCandidate
- quest_type: "production" | "delivery"
- target_item_id: str
- target_item_name: str
- quantity: int
- score: int
- reason: str
- sources: list[str]
```

최종 응답에는 `sources`와 `reason`을 직접 노출하지 않는다. 디버깅이 필요하면 `payload.metadata.quest_generation` 아래에 넣는다.

## 구현 단계

1. schema 정리
   - `Quest.type`을 `production | delivery`로 제한한다.
   - 기존 `economy`, `trade`, `tutorial`, `exploration` 허용을 제거한다.
   - 테스트에서 잘못된 타입이 거부되는지 확인한다.

2. 아이템 ID 매핑 추가
   - CSV의 `resource_iron_ingot` 같은 id를 프론트 창고가 쓰는 `iron_ingot` 형태로 변환하는 mapper를 추가한다.
   - mapping은 테스트로 고정하고, 기존 프론트 기본 퀘스트의 `iron_ore`, `iron_ingot`, `copper_ore`와 맞춘다.
   - 변환할 수 없는 id는 퀘스트 후보에서 제외한다.

3. 상황 repository 추가
   - `QuestSituationRepository`를 추가한다.
   - 테스트에서는 fake repository로 창고/최근 퀘스트/해금 레시피 상황을 주입한다.
   - 실제 PostgreSQL 연결은 프로젝트의 공용 DB 방식이 확인된 뒤 같은 인터페이스 뒤에 둔다.

4. 후보 생성기 추가
   - `QuestCandidateBuilder`를 추가한다.
   - 생산 후보는 레시피/자원/장비 CSV와 상황 데이터를 함께 사용한다.
   - 납품 후보는 창고 보유량과 최근 퀘스트 기록을 사용한다.

5. 5개 선택 서비스 개편
   - `QuestAgentService.generate_quest_json()`이 후보 생성 결과에서 생산 3개, 납품 2개를 고른다.
   - 후보 부족 시 비율을 유연하게 채우되 전체 5개는 유지한다.
   - 같은 아이템 중복을 막는다.

6. 에이전트/routing 정리
   - `QUEST_SUB_AGENT_IDS`에서 economy leaf를 제거한다.
   - 통합 leaf agent와 tool을 등록한다.
   - 명시적 `sub_agent` 요청도 새 leaf id만 허용한다.

7. 프론트 연동 확인
   - Unreal `QuestManagerSubsystem`이 `delivery` 타입도 기존 objective 구조로 읽는지 확인한다.
   - 납품 완료를 단순 보유량으로 볼지, 완료 시 `TakeItem`으로 차감할지는 별도 결정 항목으로 남긴다.

8. smoke 검증
   - WebSocket 경로에서 quest 요청이 `quests` 5개를 반환하는지 확인한다.
   - 응답 안에 `production`, `delivery` 외 타입이 없는지 확인한다.

## 테스트 계획

- `test_quest_response_rejects_removed_quest_types`
  - `economy`, `trade` 타입은 schema validation에서 실패해야 한다.

- `test_service_generates_five_production_delivery_quests_from_situation`
  - 창고에 철광석/구리광석/철괴가 있는 fake situation에서 총 5개를 생성한다.
  - 타입 집합은 `{"production", "delivery"}`만 포함한다.

- `test_service_prefers_delivery_for_overstocked_items`
  - 특정 아이템 보유량이 많으면 납품 후보에 포함한다.

- `test_service_avoids_recent_duplicate_targets`
  - 최근 퀘스트에 나온 target item은 새 응답에서 제외하거나 후순위가 된다.

- `test_service_fills_with_production_when_delivery_candidates_are_insufficient`
  - 납품 후보가 0~1개여도 총 5개를 반환한다.

- `test_default_agent_router_contains_only_production_delivery_quest_leaf`
  - router 목록에 기존 `production_quest`, `economy_quest` leaf가 없고 통합 leaf만 있다.

- `test_pipeline_routes_production_delivery_quest`
  - LLM routing부터 tool call, final payload까지 `quests` 5개가 유지된다.

## 문서/운영 반영

- `backend/src/AGENT_ROLES.md`에서 quest_generator 하위 leaf 설명을 생산/납품 구조로 갱신한다.
- `backend/src/docs_router.py`의 quest 관련 런타임 설명을 갱신한다.
- 기존 `backend/docs/plans/2026-06-04-quest-rag-plan.md`와 연결되는 후속 계획으로 이 문서를 참조한다.

## 위험과 대응

- PostgreSQL 스키마가 아직 확정되지 않았을 수 있다.
  - 대응: repository 인터페이스와 fake repository를 먼저 두고, 실제 PostgreSQL adapter는 얇게 교체한다.

- 납품 퀘스트 완료 시 아이템 차감 기준이 불명확하다.
  - 대응: 1차 구현은 기존 완료 판정을 유지하고, 차감은 별도 UI/완료 액션 계획으로 분리한다.

- LLM이 5개 제한을 어길 수 있다.
  - 대응: LLM은 tool만 호출하게 하고, 실제 5개 생성은 service가 담당한다.

- `resource_iron_ingot` 같은 CSV id와 프론트의 `iron_ingot` id가 다를 수 있다.
  - 대응: 구현 전 resource id 매핑 테스트를 추가한다. 현재 프론트 기본 퀘스트는 `iron_ore`, `iron_ingot` 형태라, backend가 반환하는 id는 프론트 창고 id와 반드시 맞춰야 한다.

## 완료 기준

- pytest에서 quest service, leaf behavior, agent contract, pipeline edge 테스트가 통과한다.
- smoke script 또는 WebSocket 테스트에서 quest 요청이 `quests` 5개를 반환한다.
- 응답 5개 중 타입은 `production`과 `delivery`만 존재한다.
- 같은 응답 안에서 동일 target item 중복이 없다.
- PostgreSQL 상황 조회 실패 시에도 fallback으로 5개를 반환한다.
- 관련 agent 문서가 생산/납품 구조로 갱신되어 있다.
