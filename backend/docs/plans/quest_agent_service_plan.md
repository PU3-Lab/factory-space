# 퀘스트 에이전트 서비스 개편 계획

## 상태

기존 퀘스트 에이전트는 서버 내부 예제 퀘스트 풀에서 생산 퀘스트 5개를 고르는 구조였다. 이후 방향은 PostgreSQL에 저장된 현재 게임 상황을 읽고, 생산 퀘스트와 납품 퀘스트를 합산 5개 생성하는 구조로 개편한다.

## 목표

Unreal에서 `agent: "quest_generator"` 요청을 보내면 backend가 PostgreSQL의 현재 상황을 조회해 `production`, `delivery` 두 타입만 포함한 퀘스트 5개를 반환한다.

성공 기준:

- 응답은 항상 `payload.quests` 배열에 5개 퀘스트를 담는다.
- 퀘스트 타입은 `production`, `delivery`만 허용한다.
- `economy`, `trade`, `tutorial`, `exploration` 타입은 퀘스트 에이전트 응답 계약에서 제거한다.
- 같은 응답 안에서 동일 `target_item_id`를 중복으로 내보내지 않는다.
- PostgreSQL 조회 실패나 후보 부족 상황에서도 fallback으로 5개를 반환한다.
- Unreal 수신 계약인 `id`, `type`, `title`, `description`, `objectives[target_item_id, quantity]`는 유지한다.

## 현재 유지할 응답 계약

```json
{
  "type": "agent.response",
  "request_id": "request-quest",
  "session_id": "session-1",
  "client_id": "unreal-client",
  "agent": "quest_generator",
  "payload": {
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
      },
      {
        "id": 2,
        "type": "delivery",
        "title": "철광석 8개 납품",
        "description": "창고에 보관 중인 철광석 8개를 납품하세요.",
        "objectives": [
          {
            "target_item_id": "iron_ore",
            "quantity": 8
          }
        ]
      }
    ],
    "metadata": {
      "selectedAgent": "quest_generator",
      "selectedLeafAgent": "quest_generator.production_delivery_quest"
    }
  },
  "streams": []
}
```

## leaf Agent 범위

퀘스트 생성 도메인은 통합 leaf Agent 하나를 사용한다.

- `quest_generator.production_delivery_quest`: PostgreSQL 상황과 CSV 기준 데이터를 함께 사용해 생산/납품 후보를 만들고, 총 5개의 퀘스트를 생성한다.

제거 대상:

- `quest_generator.production_quest`: 단독 생산 leaf로는 유지하지 않는다.
- `quest_generator.economy_quest`: 경제 퀘스트 타입을 제거하므로 유지하지 않는다.
- `quest_generator.trade_quest`: 무역 퀘스트는 이번 범위가 아니다.
- `quest_generator.tutorial_quest`, `quest_generator.exploration_quest`: 현재 퀘스트 에이전트 범위가 아니다.

명시된 `sub_agent`가 제거된 leaf id를 가리키면 `INVALID_SUB_AGENT`로 처리한다.

## PostgreSQL 상황 조회

Agent가 직접 DB session을 다루지 않는다. 접근 경계는 다음처럼 둔다.

```text
ProductionDeliveryQuestAgent
  -> QuestAgentService
  -> QuestSituationRepository
  -> PostgreSQL
```

`QuestSituationRepository`가 제공할 최소 데이터:

- 창고 보유량: `item_id`, `quantity`
- 최근 생성/완료된 퀘스트: `quest_type`, `target_item_id`, `created_at`, `completed_at`
- 해금된 레시피: `recipe_id`
- 사용 가능한 장비: `equipment_id`

1차 구현에서 PostgreSQL 테이블명이 아직 확정되지 않았다면 repository interface와 fake repository 테스트를 먼저 만든다. 실제 PostgreSQL adapter는 같은 interface 뒤에 붙인다.

## CSV 기준 데이터와 item id

생산 후보는 `data/game/recipes.csv`, `resources.csv`, `equipment.csv`를 기준 데이터로 사용한다. 단, CSV의 `resource_iron_ingot` 같은 id와 Unreal 창고의 `iron_ingot` id가 다를 수 있으므로 mapper를 둔다.

규칙:

- backend 응답의 `target_item_id`는 Unreal 창고 id와 맞춘다.
- `resource_` prefix 제거만으로 매핑 가능한 항목은 mapper에서 명시적으로 검증한다.
- 매핑할 수 없는 CSV 자원은 퀘스트 후보에서 제외한다.

## 생성 규칙

기본 조합은 생산 3개, 납품 2개다.

생산 후보:

- 해금된 레시피와 사용 가능한 장비로 만들 수 있는 아이템만 후보로 둔다.
- 보유량이 부족하거나 다음 생산 체인에 필요한 아이템의 점수를 높인다.
- 최근 생성/완료된 같은 target item은 제외하거나 점수를 낮춘다.

납품 후보:

- PostgreSQL 창고 보유량이 충분한 아이템만 후보로 둔다.
- 과잉 재고는 납품 후보 점수를 높인다.
- 생산 체인의 핵심 중간재를 모두 소모하는 수량은 만들지 않는다.

후보 부족 처리:

- 납품 후보가 2개 미만이면 생산 후보로 채운다.
- 생산 후보가 3개 미만이면 납품 후보로 채운다.
- 전체 후보가 5개 미만이면 CSV 기반 deterministic fallback 후보로 채운다.

## 구현 계획

### Task 1. schema 정리

대상 파일:

- `backend/src/agents/quest_generator/schemas.py`
- `backend/tests/test_quest_agent_service.py`

작업:

- [ ] `Quest.type`을 `Literal["production", "delivery"]`로 제한한다.
- [ ] `economy`, `trade`, `tutorial`, `exploration` 타입 거부 테스트를 추가한다.
- [ ] 기존 `quests` 배열 응답 계약은 유지한다.

검증:

- [ ] `test_quest_response_rejects_removed_quest_types`
- [ ] `test_quest_response_accepts_production_and_delivery`

### Task 2. PostgreSQL 상황 repository 경계 추가

대상 파일:

- `backend/src/agents/quest_generator/repository.py`
- `backend/src/agents/quest_generator/models.py`
- `backend/tests/test_quest_agent_repository.py`

작업:

- [ ] `QuestSituationRepository` interface를 추가한다.
- [ ] repository가 PostgreSQL에서 창고, 최근 퀘스트, 해금 레시피, 사용 가능 장비를 읽도록 설계한다.
- [ ] 테스트에서는 fake repository로 상황을 주입한다.

검증:

- [ ] fake repository로 `QuestSituation`을 구성하는 단위 테스트를 통과시킨다.
- [ ] PostgreSQL adapter는 DB 연결 설정이 준비된 뒤 integration test로 분리한다.

### Task 3. item id mapper 추가

대상 파일:

- `backend/src/agents/quest_generator/item_mapping.py`
- `backend/tests/test_quest_agent_item_mapping.py`

작업:

- [ ] CSV resource id를 Unreal 창고 item id로 변환한다.
- [ ] `resource_iron_ore -> iron_ore`, `resource_iron_ingot -> iron_ingot`, `resource_copper_ore -> copper_ore` 매핑을 테스트로 고정한다.
- [ ] 매핑 실패 항목은 후보 생성에서 제외할 수 있게 한다.

검증:

- [ ] `test_maps_csv_resource_ids_to_unreal_item_ids`
- [ ] `test_unknown_resource_id_is_not_mapped`

### Task 4. 후보 생성기 추가

대상 파일:

- `backend/src/agents/quest_generator/candidates.py`
- `backend/tests/test_quest_agent_candidates.py`

작업:

- [ ] 생산 후보를 레시피/장비/창고 상황으로 만든다.
- [ ] 납품 후보를 창고 보유량으로 만든다.
- [ ] 최근 퀘스트 중복 target item을 점수에서 불리하게 처리한다.

검증:

- [ ] `test_builds_production_candidates_from_unlocked_recipes`
- [ ] `test_builds_delivery_candidates_from_warehouse_stock`
- [ ] `test_recent_duplicate_targets_are_deprioritized`

### Task 5. 서비스 생성 로직 개편

대상 파일:

- `backend/src/agents/quest_generator/service.py`
- `backend/tests/test_quest_agent_service.py`

작업:

- [ ] `QuestAgentService.generate_quest_json()`이 PostgreSQL 상황 기반 후보에서 총 5개를 고른다.
- [ ] 기본 비율은 생산 3개, 납품 2개로 둔다.
- [ ] 후보 부족 시에도 총 5개를 유지한다.
- [ ] 중복 `target_item_id`를 제거한다.

검증:

- [ ] `test_service_generates_five_production_delivery_quests_from_situation`
- [ ] `test_service_prefers_delivery_for_overstocked_items`
- [ ] `test_service_fills_with_production_when_delivery_candidates_are_insufficient`
- [ ] `test_service_falls_back_when_postgresql_situation_is_unavailable`

### Task 6. agent/routing 정리

대상 파일:

- `backend/src/agents/quest_generator/agent.py`
- `backend/src/agents/quest_generator/production_delivery_quest.py`
- `backend/src/agents/quest_generator/tools.py`
- `backend/src/agents/router.py`
- `backend/tests/test_agent_contracts.py`
- `backend/tests/test_agent_leaf_behaviors.py`
- `backend/tests/test_pipeline_edges.py`

작업:

- [ ] `QUEST_SUB_AGENT_IDS`를 `quest_generator.production_delivery_quest` 하나로 정리한다.
- [ ] economy leaf 등록을 제거한다.
- [ ] 통합 leaf agent가 `quest_generator.generate_production_delivery_quests` tool을 노출한다.
- [ ] LLM 응답이 tool을 호출하지 않으면 오류 또는 deterministic fallback으로 처리한다.

검증:

- [ ] `test_default_agent_router_contains_production_delivery_quest_agent`
- [ ] `test_removed_quest_sub_agents_are_rejected`
- [ ] `test_pipeline_routes_production_delivery_quest`

### Task 7. smoke 검증

대상 파일:

- `backend/scripts/smoke_agent_pipeline.py`
- `backend/tests/test_smoke_agent_pipeline_script.py`

작업:

- [ ] quest smoke가 `quests` 5개를 검증한다.
- [ ] 응답 타입이 `production`, `delivery` 외 값을 포함하지 않는지 검증한다.

검증:

- [ ] `python -m pytest tests/test_smoke_agent_pipeline_script.py -v`
- [ ] PostgreSQL이 연결된 개발 환경에서 WebSocket quest 요청 smoke를 수행한다.

## 최종 검증 명령

```bash
cd backend
python -m pytest tests/test_quest_agent_service.py -v
python -m pytest tests/test_agent_contracts.py tests/test_agent_leaf_behaviors.py tests/test_pipeline_edges.py -v
python -m pytest tests/test_smoke_agent_pipeline_script.py -v
python -m ruff check src tests scripts
```

PostgreSQL integration test는 DB 연결 환경 변수가 준비된 환경에서 별도 실행한다.
