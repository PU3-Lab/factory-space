# Factory-Space 서버 시뮬레이션 구조 기획서
## PostgreSQL + JSONB 기반 설계안

---

## 1. 전체 방향

Factory-Space의 공장 구조는 **클라이언트 좌표 중심**이 아니라 **서버 논리 그래프 중심**으로 관리한다.

클라이언트는 공장 상태를 계산하지 않는다.  
클라이언트는 설치, 연결, 설정, 제거 같은 명령만 서버에 보낸다.

서버는 공장 논리 구조를 저장하고, 생산 / 전력 / 물류 / 저장 시뮬레이션을 직접 수행한다.

```text
Client
= 설치 / 연결 / 설정 / 제거 명령 전송
= 서버가 내려준 상태를 화면에 표시
= 좌표 / 회전 / 메시 배치 / 컨베이어 시각 경로 관리

Server
= ID 생성
= 명령 검증
= 공장 논리 그래프 저장
= 생산 / 전력 / 물류 / 저장 시뮬레이션
= 생산 tick 결과 생성
= FactorySnapshot 생성
= FactoryAnalyzer 실행
= QuestComposerAgent 실행
= 퀘스트 진행도 판정
```

핵심 원칙은 다음과 같다.

```text
서버는 좌표를 저장하지 않는다.
서버는 Node / SubNode / Entity / Port / Connection 중심의 논리 그래프만 관리한다.
서버가 모든 영속 ID를 생성한다.
클라이언트는 새 ID를 만들지 않는다.
클라이언트는 공장 상태를 계산하지 않는다.
DB는 PostgreSQL + JSONB 기반으로 설계한다.
핵심 그래프 구조는 테이블로 저장한다.
자주 바뀌는 상태 / 요약 / 설정 / 스냅샷 / 로그성 데이터는 JSONB로 저장한다.
```

---

## 2. DB 설계 방향

Factory-Space는 공장 구조, 기계 속성, 생산 규칙, 퀘스트 조건, 분석 결과가 계속 바뀔 가능성이 높다.

따라서 모든 필드를 정규화해서 테이블 컬럼으로 분리하면 초기 개발 속도가 느려진다.

반대로 전체 공장 상태를 하나의 JSONB로 저장하면 시뮬레이션에서 특정 Entity, Port, Connection을 찾기 어렵다.

그래서 DB는 **하이브리드 구조**로 간다.

```text
핵심 식별 구조
= 테이블 컬럼

변경 가능성이 높은 상세 데이터
= JSONB
```

---

## 3. 테이블로 분리해야 하는 데이터

아래 데이터는 시뮬레이션이 자주 조회하므로 테이블로 분리한다.

```text
factories
factory_nodes
factory_sub_nodes
factory_powers
factory_power_sub_nodes
factory_entities
factory_ports
factory_connections
```

이 데이터들은 서버 논리 그래프의 뼈대다.

서버는 매 tick마다 다음 정보를 빠르게 찾아야 한다.

```text
이 공장의 Entity 목록
이 Node에 속한 Entity 목록
이 Entity의 Port 목록
이 Port와 연결된 Connection
이 Power가 공급하는 Node 목록
특정 Node의 생산 / 저장 / 물류 상태
```

따라서 이 구조를 통짜 JSONB에 넣지 않는다.

---

## 4. JSONB로 저장하는 데이터

아래 데이터는 JSONB로 저장한다.

```text
summary_json
runtime_json
config_json
items_json
snapshot_json
reward_json
objective_json
analysis_json
produced_items_json
consumed_items_json
```

JSONB로 저장하는 이유는 다음과 같다.

```text
속성 변경이 잦다.
아이템 종류가 계속 늘어날 수 있다.
퀘스트 조건이 다양해진다.
Analyzer 결과 구조가 계속 바뀔 수 있다.
Snapshot은 조회보다 저장 / 전달 목적이 강하다.
초기 구현 속도가 빠르다.
```

---

## 5. 저장하지 않는 데이터

서버 DB에는 좌표를 저장하지 않는다.

```text
position
rotation
world_x
world_y
world_z
mesh transform
conveyor path coordinates
```

이 데이터는 클라이언트 LayoutData에서 관리한다.

```json
{
  "layout_id": "layout_factory_001",
  "entity_layouts": [
    {
      "entity_id": "ent_001",
      "position": { "x": 20, "y": 8, "z": 0 },
      "rotation": 90
    }
  ],
  "transport_layouts": [
    {
      "connection_id": "conn_001",
      "path": [
        { "x": 8, "y": 8, "z": 0 },
        { "x": 9, "y": 8, "z": 0 }
      ]
    }
  ]
}
```

LayoutData는 서버 분석용이 아니다.  
클라이언트 렌더링과 월드 복원용이다.

---

## 6. 전체 DB 구조

```text
Factory 구조
├─ factories
├─ factory_nodes
├─ factory_sub_nodes
├─ factory_powers
└─ factory_power_sub_nodes

Graph 구조
├─ factory_entities
├─ factory_ports
└─ factory_connections

Runtime / Simulation 구조
├─ machine_runtime_states
├─ power_runtime_states
└─ production_event_logs
   (connection 런타임은 factory_connections.runtime_json에 inline 저장)

Inventory / Snapshot 구조
├─ storage_states
├─ factory_inventory_summaries
└─ factory_snapshots

Quest 구조
├─ quest_instances
└─ quest_objective_progress
```

---

## 7. PostgreSQL 기본 설정 및 공통 규칙

ID는 서버가 생성한다.

권장 ID 형식은 prefix + ULID다.

```text
factory_01J...
node_01J...
sub_01J...
power_01J...
psub_01J...
ent_01J...
port_01J...
conn_01J...
qinst_01J...
```

ULID는 애플리케이션 레벨에서 생성하는 것을 기본으로 한다.  
DB에서는 ID를 자동 생성하지 않고, 서버 Command Handler가 생성한다.  
따라서 ID 자동 생성을 위한 별도 확장(pgcrypto 등)은 필요하지 않다.

### 7.1 Source of Truth 원칙 (중요)

데이터 중복 저장을 피하기 위해 테이블 역할을 명확히 나눈다.

```text
정의 / 설정 (변하지 않거나 드물게 변함)
= base 테이블 (factory_entities, factory_powers, factory_connections ...)
= 식별자, 타입, 이름, current_recipe_id, config_json

고빈도 가변 런타임 (매 tick 갱신)
= *_runtime_states 테이블 (machine_runtime_states, power_runtime_states ...)
= status, progress, buffer, capacity/usage, margin ...
```

같은 값을 base 테이블과 runtime 테이블에 동시에 들고 있지 않는다.  
런타임 값의 Source of Truth는 항상 `*_runtime_states` 테이블이다.

단, connection 런타임은 갱신 빈도와 데이터량이 낮아 별도 테이블을 만들지 않고
`factory_connections.runtime_json`에 inline으로 둔다.

### 7.2 타임스탬프 규칙

모든 시간 컬럼은 `TIMESTAMPTZ`(UTC)로 저장한다.  
시뮬레이션 tick, 스냅샷, 이벤트 로그가 모두 UTC 기준이기 때문이다.

`updated_at`은 INSERT 시점에만 채워지므로 UPDATE 시 자동 갱신되도록
`moddatetime` 트리거 또는 애플리케이션 레벨에서 갱신을 보장한다.

```sql
CREATE EXTENSION IF NOT EXISTS moddatetime;

-- 예: factory_entities
CREATE TRIGGER trg_factory_entities_updated_at
BEFORE UPDATE ON factory_entities
FOR EACH ROW EXECUTE FUNCTION moddatetime(updated_at);
```

`updated_at`을 가진 모든 테이블에 동일한 트리거를 건다.

### 7.3 동시성 규칙

Command Handler(설정 변경)와 Simulation Loop(tick 갱신)가 같은 엔티티를 동시에
쓸 수 있으므로 충돌을 방지해야 한다.

```text
원칙 1
= 한 공장의 시뮬레이션은 단일 워커가 직렬로 처리한다 (공장 단위 직렬화)

원칙 2
= 가변 테이블에는 version 컬럼을 두고 낙관적 잠금(optimistic lock)을 적용한다
```

가변 상태 테이블(`*_runtime_states`, `factory_entities`, `factory_connections` 등)에는
`version INTEGER NOT NULL DEFAULT 0` 컬럼을 두고, UPDATE 시 version을 비교/증가시킨다.

### 7.4 마스터 데이터 검증

`machine_name`, `current_recipe_id`, `item_ids` 등은 DB FK가 아니라
파일 기반 마스터(MachineTable / RecipeTable / ItemTable)로 관리한다.

Command Handler는 명령을 적용하기 전에 마스터 데이터에 존재하는 값인지 검증하고,
존재하지 않으면 명령을 거부한다. (검증 실패 시 4xx 응답)

---

## 8. Factory 테이블

### 8.1 factories

Factory는 공장의 루트다.

```sql
CREATE TABLE factories (
    factory_id TEXT PRIMARY KEY,
    owner_id TEXT NOT NULL,

    factory_name TEXT NOT NULL,
    factory_level INTEGER NOT NULL DEFAULT 1,
    current_main_quest_id TEXT NULL,

    config_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    summary_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`owner_id`는 공장 소유 유저를 가리킨다.  
Command Handler는 명령을 적용하기 전에 요청 유저가 해당 공장의 `owner_id`와
일치하는지 검증한다. WebSocket 구독도 같은 기준으로 권한을 확인한다.

> 유저/계정 테이블(`users` 등)은 별도 인증 도메인에서 관리하며 본 문서 범위 밖이다.
> `owner_id`는 그 유저 식별자를 참조한다.

`config_json` 예시:

```json
{
  "simulation_enabled": true,
  "tick_rate": 1,
  "snapshot_interval_sec": 30
}
```

`summary_json` 예시:

```json
{
  "node_count": 5,
  "entity_count": 24,
  "status": "running",
  "issue_tags": []
}
```

---

## 9. Node 테이블

### 9.1 factory_nodes

FactoryNode는 생산, 저장, 물류, 채취, 허브, 방어 같은 일반 공장 노드다.

```sql
CREATE TABLE factory_nodes (
    node_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    node_type TEXT NOT NULL,
    node_name TEXT NOT NULL,

    summary_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    runtime_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`node_type` 후보:

```text
production
resource
storage
logistics
hub
defense
```

`summary_json` 예시:

```json
{
  "main_outputs": ["iron_ingot"],
  "required_power": 45,
  "status": "running",
  "issue_tags": []
}
```

`runtime_json` 예시:

```json
{
  "production_per_min": {
    "iron_ingot": 4
  },
  "input_shortage": [],
  "output_blocked": false
}
```

---

### 9.2 factory_sub_nodes

FactorySubNode는 FactoryNode 내부의 작업 단위다.

```sql
CREATE TABLE factory_sub_nodes (
    sub_node_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,
    node_id TEXT NOT NULL REFERENCES factory_nodes(node_id) ON DELETE CASCADE,

    sub_node_type TEXT NOT NULL,
    sub_node_name TEXT NOT NULL,

    summary_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    runtime_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`sub_node_type` 예시:

```text
mining
smelting
crafting
storage
input
output
distribution
```

`summary_json` 예시:

```json
{
  "main_recipe_ids": ["smelt_iron"],
  "main_outputs": ["iron_ingot"],
  "status": "running",
  "issue_tags": []
}
```

---

## 10. Power 테이블

### 10.1 factory_powers

FactoryPower는 전력 네트워크 단위다.  
FactoryNode의 하위가 아니라 Factory 아래에서 Node와 동급이다.

```sql
CREATE TABLE factory_powers (
    power_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    power_type TEXT NOT NULL,
    power_name TEXT NOT NULL,

    summary_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`power_type` 예시:

```text
main_power_grid
remote_power_grid
backup_power_grid
```

`factory_powers`는 전력 네트워크의 정의만 가진다.  
total_capacity, total_usage, margin, status 같은 계산 결과(런타임)는
`power_runtime_states`에 저장한다 (7.1 Source of Truth 원칙 참조).

---

### 10.2 factory_power_sub_nodes

PowerSubNode는 FactoryPower 내부 구성 요소다.

```sql
CREATE TABLE factory_power_sub_nodes (
    power_sub_node_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,
    power_id TEXT NOT NULL REFERENCES factory_powers(power_id) ON DELETE CASCADE,

    sub_node_type TEXT NOT NULL,
    sub_node_name TEXT NOT NULL,

    summary_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    runtime_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`sub_node_type` 예시:

```text
generator
transmission
battery
backup
```

---

## 11. Entity 테이블

### 11.1 factory_entities

FactoryEntity는 유저가 실제 설치한 설비 인스턴스다.

```text
Smelter_Lv1
= MachineTable에 있는 설비 정의

ent_001
= 유저가 실제 설치한 설비 인스턴스
```

```sql
CREATE TABLE factory_entities (
    entity_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    node_id TEXT NULL REFERENCES factory_nodes(node_id) ON DELETE CASCADE,
    sub_node_id TEXT NULL REFERENCES factory_sub_nodes(sub_node_id) ON DELETE CASCADE,

    power_id TEXT NULL REFERENCES factory_powers(power_id) ON DELETE CASCADE,
    power_sub_node_id TEXT NULL REFERENCES factory_power_sub_nodes(power_sub_node_id) ON DELETE CASCADE,

    machine_name TEXT NOT NULL,
    current_recipe_id TEXT NULL,

    config_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),

    CHECK (
        (
            node_id IS NOT NULL
            AND sub_node_id IS NOT NULL
            AND power_id IS NULL
            AND power_sub_node_id IS NULL
        )
        OR
        (
            node_id IS NULL
            AND sub_node_id IS NULL
            AND power_id IS NOT NULL
            AND power_sub_node_id IS NOT NULL
        )
    )
);
```

검증 규칙:

```text
일반 기계면:
node_id, sub_node_id 필수
power_id, power_sub_node_id NULL

전력 기계면:
power_id, power_sub_node_id 필수
node_id, sub_node_id NULL
```

`factory_entities`는 설비의 정의/설정만 가진다.  
status, durability, progress, input/output buffer 같은 고빈도 런타임 값은
`machine_runtime_states`에 저장한다 (7.1 Source of Truth 원칙 참조).

---

## 12. Port 테이블

### 12.1 factory_ports

포트는 서버 논리 연결 그래프를 만들기 위해 필요하다.  
포트도 좌표를 가지지 않는다.

```sql
CREATE TABLE factory_ports (
    port_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,
    entity_id TEXT NOT NULL REFERENCES factory_entities(entity_id) ON DELETE CASCADE,

    port_type TEXT NOT NULL,
    port_index INTEGER NOT NULL,

    flow_type TEXT NOT NULL,
    item_ids_json JSONB NOT NULL DEFAULT '[]'::jsonb,

    config_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),

    UNIQUE (entity_id, port_type, port_index)
);
```

`port_type`:

```text
input
output
```

`flow_type`:

```text
solid
liquid
gas
power
```

입력 포트 예시:

```json
{
  "port_type": "input",
  "flow_type": "solid",
  "item_ids": ["iron_ore"]
}
```

출력 포트 예시:

```json
{
  "port_type": "output",
  "flow_type": "solid",
  "item_ids": ["iron_ingot"]
}
```

---

## 13. Connection 테이블

### 13.1 factory_connections

FactoryConnection은 Node, Power, Entity, Port 사이의 논리 연결을 관리한다.

좌표나 실제 경로는 저장하지 않는다.

```sql
CREATE TABLE factory_connections (
    connection_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    connection_type TEXT NOT NULL,

    from_type TEXT NOT NULL,
    from_id TEXT NOT NULL,

    to_type TEXT NOT NULL,
    to_id TEXT NOT NULL,

    item_ids_json JSONB NOT NULL DEFAULT '[]'::jsonb,

    transport_type TEXT NULL,
    transport_tier INTEGER NULL,

    status TEXT NOT NULL DEFAULT 'active',

    config_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    runtime_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

> **참조 무결성 주의**: `from_id` / `to_id`는 다형성(node/port/entity 등) 참조라
> FK를 걸 수 없다. 따라서 Entity나 Port가 `ON DELETE CASCADE`로 삭제돼도
> 그것을 가리키던 connection은 자동으로 정리되지 않는다.
> `remove_entity` / `remove_connection` Command 처리에서 관련 connection을
> 명시적으로 삭제해야 한다 (24.2 참조). 시뮬레이션 루프는 죽은 참조를 만나면
> 해당 connection을 무시하고 정리 대상으로 표시한다.

`connection_type`:

```text
item_flow
power_supply
signal
```

`from_type`, `to_type`:

```text
node
sub_node
power
power_sub_node
entity
port
```

역할 기준:

```text
Port → Port
= 실제 아이템 이동 계산용

Power → Node
= 전력 공급 계산용

Node → Node
= 요약 / 분석 / UI 구조용
```

`runtime_json` 예시:

```json
{
  "throughput_per_min": 60,
  "current_flow_per_min": 42,
  "status": "active",
  "issue_tags": []
}
```

---

## 14. Storage / Inventory 테이블

### 14.1 storage_states

StorageState는 storage entity 단위로 가진다.

```sql
CREATE TABLE storage_states (
    storage_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,
    storage_entity_id TEXT NOT NULL REFERENCES factory_entities(entity_id) ON DELETE CASCADE,

    items_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    capacity INTEGER NOT NULL DEFAULT 0,
    used INTEGER NOT NULL DEFAULT 0,
    free INTEGER GENERATED ALWAYS AS (capacity - used) STORED,

    version INTEGER NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),

    UNIQUE (storage_entity_id)
);
```

`items_json` 예시:

```json
{
  "iron_ore": 120,
  "iron_ingot": 8,
  "copper_ore": 35
}
```

Node / Factory 단위 재고는 aggregation으로 계산한다.

```text
StorageState
= storage entity 기준

NodeStorageSummary
= Node 내부 storage aggregation

FactoryInventorySummary
= Factory 전체 storage aggregation
```

---

### 14.2 factory_inventory_summaries

Factory 전체 재고 요약이다.

```sql
CREATE TABLE factory_inventory_summaries (
    factory_id TEXT PRIMARY KEY REFERENCES factories(factory_id) ON DELETE CASCADE,

    items_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    shortage_items_json JSONB NOT NULL DEFAULT '[]'::jsonb,

    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

이 테이블은 매 tick마다 쓰지 않는다.

추천 갱신 주기:

```text
5초
= inventory_updated push

30초
= snapshot 생성
```

---

## 15. Runtime State 테이블

### 15.1 machine_runtime_states

기계 진행도, 상태, 정지 이유를 저장한다.

```sql
CREATE TABLE machine_runtime_states (
    entity_id TEXT PRIMARY KEY REFERENCES factory_entities(entity_id) ON DELETE CASCADE,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    status TEXT NOT NULL DEFAULT 'idle',
    progress REAL NOT NULL DEFAULT 0.0,
    progress_time REAL NOT NULL DEFAULT 0.0,
    durability INTEGER NOT NULL DEFAULT 1000,

    stop_reason TEXT NULL,

    runtime_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    last_tick BIGINT NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`status`:

```text
idle
running
stopped
blocked
waiting_input
waiting_output
no_power
broken
```

`runtime_json` 예시:

```json
{
  "input_buffer": {
    "iron_ore": 2
  },
  "output_buffer": {},
  "last_recipe_completed_at": "2026-06-16T10:20:00Z"
}
```

---

### 15.2 power_runtime_states

전력 계산 결과를 저장한다.

```sql
CREATE TABLE power_runtime_states (
    power_id TEXT PRIMARY KEY REFERENCES factory_powers(power_id) ON DELETE CASCADE,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    total_capacity REAL NOT NULL DEFAULT 0,
    total_usage REAL NOT NULL DEFAULT 0,
    margin REAL NOT NULL DEFAULT 0,
    margin_rate REAL NOT NULL DEFAULT 0,

    status TEXT NOT NULL DEFAULT 'idle',
    issue_tags_json JSONB NOT NULL DEFAULT '[]'::jsonb,

    runtime_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

`status`:

```text
stable
warning
danger
blackout
```

---

## 16. Production Event 테이블

### 16.1 production_event_logs

생산 완료 로그를 저장한다.

```sql
CREATE TABLE production_event_logs (
    production_event_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    tick BIGINT NOT NULL,

    entity_id TEXT NOT NULL REFERENCES factory_entities(entity_id) ON DELETE CASCADE,
    node_id TEXT NULL,
    sub_node_id TEXT NULL,

    recipe_id TEXT NOT NULL,

    consumed_items_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    produced_items_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

예시:

```json
{
  "consumed_items_json": {
    "iron_ore": 2
  },
  "produced_items_json": {
    "iron_ingot": 1
  }
}
```

이 로그는 다음에 사용한다.

```text
생산 이력 확인
퀘스트 진행도 갱신
생산률 계산
FactoryAnalyzer 분석
디버깅
```

> **무한 증가 주의**: 1초 tick에서 생산 완료마다 row가 쌓이므로 공장·시간당
> 적재량이 매우 크다. 다음 전략을 적용한다.
>
> - `created_at`(또는 `tick`) 기준 시간 파티셔닝(`PARTITION BY RANGE`)
> - 일정 기간(예: 7~30일) 경과 row는 집계 후 아카이브하거나 삭제
> - 생산률·퀘스트 갱신은 가능하면 이 로그 대신 ProductionDelta 스트림에서
>   직접 처리하여 로그 조회 부하를 낮춘다

---

## 17. FactorySnapshot 테이블

### 17.1 factory_snapshots

FactorySnapshot은 JSONB로 통째로 저장한다.

```sql
CREATE TABLE factory_snapshots (
    snapshot_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    snapshot_version BIGINT NOT NULL,

    snapshot_json JSONB NOT NULL,

    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),

    UNIQUE (factory_id, snapshot_version)
);
```

`snapshot_json` 예시:

```json
{
  "factory_id": "factory_001",
  "factory_level": 3,
  "current_main_quest_id": "main_commtower",
  "node_summaries": [
    {
      "node_id": "node_iron_001",
      "node_type": "production",
      "status": "unstable",
      "main_outputs": ["iron_ingot"],
      "production_per_min": {
        "iron_ingot": 4
      },
      "issue_tags": [
        "low_output",
        "power_instability"
      ]
    }
  ],
  "power_summaries": [
    {
      "power_id": "power_main_001",
      "status": "danger",
      "total_capacity": 180,
      "total_usage": 172,
      "margin_rate": 0.04,
      "issue_tags": [
        "power_margin_low"
      ]
    }
  ],
  "storage_summary": {
    "items": {
      "iron_ore": 120,
      "iron_ingot": 8
    },
    "shortage_items": [
      "iron_ingot"
    ]
  }
}
```

Snapshot은 클라이언트 표시, 분석, 퀘스트 생성의 기준 데이터다.

---

## 18. Quest 테이블

### 18.1 quest_instances

유저에게 발급된 퀘스트 인스턴스를 저장한다.

```sql
CREATE TABLE quest_instances (
    quest_instance_id TEXT PRIMARY KEY,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    quest_id TEXT NOT NULL,
    quest_type TEXT NOT NULL,

    title TEXT NOT NULL,
    description TEXT NOT NULL,

    status TEXT NOT NULL DEFAULT 'active',

    objective_json JSONB NOT NULL DEFAULT '[]'::jsonb,
    reward_json JSONB NOT NULL DEFAULT '{}'::jsonb,
    meta_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    created_by TEXT NOT NULL DEFAULT 'system',

    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    completed_at TIMESTAMPTZ NULL,
    claimed_at TIMESTAMPTZ NULL
);
```

`quest_type`:

```text
main
support
daily
event
```

`created_by`:

```text
system
quest_composer_agent
```

`objective_json` 예시:

```json
[
  {
    "objective_id": "produce_iron_ingot",
    "objective_type": "produce_item",
    "target_item_id": "iron_ingot",
    "target_value": 20
  },
  {
    "objective_id": "stabilize_power",
    "objective_type": "stabilize_power",
    "target_margin_rate": 0.15
  }
]
```

---

### 18.2 quest_objective_progress

목표별 진행도는 별도 테이블로 분리한다.

```sql
CREATE TABLE quest_objective_progress (
    objective_progress_id TEXT PRIMARY KEY,
    quest_instance_id TEXT NOT NULL REFERENCES quest_instances(quest_instance_id) ON DELETE CASCADE,
    factory_id TEXT NOT NULL REFERENCES factories(factory_id) ON DELETE CASCADE,

    objective_id TEXT NOT NULL,
    objective_type TEXT NOT NULL,

    target_item_id TEXT NULL,
    current_value REAL NOT NULL DEFAULT 0,
    target_value REAL NOT NULL,

    status TEXT NOT NULL DEFAULT 'active',

    progress_json JSONB NOT NULL DEFAULT '{}'::jsonb,

    version INTEGER NOT NULL DEFAULT 0,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),

    UNIQUE (quest_instance_id, objective_id)
);
```

진행도는 자주 갱신되므로 `objective_json` 안에 묶지 않고 별도 테이블로 둔다.  
`(quest_instance_id, objective_id)` UNIQUE로 목표당 진행도 row 중복을 방지한다.

---

## 19. 인덱스 설계

자주 조회하는 기준에 인덱스를 둔다.

```sql
CREATE INDEX idx_factories_owner_id
ON factories(owner_id);

CREATE INDEX idx_factory_nodes_factory_id
ON factory_nodes(factory_id);

CREATE INDEX idx_factory_sub_nodes_node_id
ON factory_sub_nodes(node_id);

CREATE INDEX idx_factory_entities_factory_id
ON factory_entities(factory_id);

CREATE INDEX idx_factory_entities_node_id
ON factory_entities(node_id);

CREATE INDEX idx_factory_entities_sub_node_id
ON factory_entities(sub_node_id);

CREATE INDEX idx_factory_ports_entity_id
ON factory_ports(entity_id);

CREATE INDEX idx_factory_connections_factory_id
ON factory_connections(factory_id);

CREATE INDEX idx_factory_connections_from
ON factory_connections(from_type, from_id);

CREATE INDEX idx_factory_connections_to
ON factory_connections(to_type, to_id);

CREATE INDEX idx_production_event_logs_factory_tick
ON production_event_logs(factory_id, tick);

CREATE INDEX idx_factory_snapshots_factory_version
ON factory_snapshots(factory_id, snapshot_version DESC);

CREATE INDEX idx_quest_instances_factory_status
ON quest_instances(factory_id, status);
```

JSONB 검색이 필요한 필드에는 GIN 인덱스를 추가한다.

```sql
CREATE INDEX idx_storage_states_items_json
ON storage_states USING GIN (items_json);

CREATE INDEX idx_factory_snapshots_snapshot_json
ON factory_snapshots USING GIN (snapshot_json);

CREATE INDEX idx_quest_instances_objective_json
ON quest_instances USING GIN (objective_json);
```

단, 초기에는 GIN 인덱스를 과하게 만들지 않는다.  
실제 조회 패턴이 생긴 뒤 추가한다.

---

## 20. 서버 시뮬레이션 루프

서버는 일정 주기로 공장 상태를 계산한다.

```text
Simulation Tick
↓
Power Simulation
↓
Logistics Simulation
↓
Machine Simulation
↓
Storage Simulation
↓
ProductionDelta 생성
↓
Inventory 갱신
↓
ProductionEventLog 저장
↓
Quest Progress 갱신
↓
FactorySnapshot 생성
↓
WebSocket Push
```

추천 주기:

```text
1초
= 전력 / 기계 상태 / 기계 progress

생산 완료 시점
= production_tick_result push

5초
= 생산 / 저장 / 물류 요약
= inventory_updated push

30초
= FactorySnapshot 생성
= factory_snapshot_updated push

60초
= FactoryAnalyzer 실행
= QuestComposerAgent 실행
```

---

## 21. ProductionDelta

생산 tick 결과는 ProductionDelta로 정리한다.

ProductionDelta는 서버 내부 이벤트이며, WebSocket 이벤트와 ProductionEventLog로 변환된다.

```json
{
  "tick": 128,
  "factory_id": "factory_001",
  "entity_id": "ent_smelter_001",
  "node_id": "node_iron_001",
  "sub_node_id": "sub_smelting_001",
  "recipe_id": "smelt_iron",
  "consumed_items": {
    "iron_ore": 2
  },
  "produced_items": {
    "iron_ingot": 1
  },
  "inventory_delta": {
    "iron_ore": -2,
    "iron_ingot": 1
  }
}
```

ProductionDelta 사용처:

```text
inventory 갱신
production_tick_result WebSocket push
production_event_logs 저장
quest_objective_progress 갱신
FactorySnapshot 생산률 계산
FactoryAnalyzer 분석 데이터
```

---

## 22. WebSocket 이벤트

WebSocket은 공장당 하나로 시작한다.

```text
WS /ws/factories/{factory_id}
```

명령은 REST API로 처리하고, WebSocket은 서버 상태 push에 집중한다.

```text
REST API
= 생성 / 연결 / 설정 / 제거 명령

WebSocket
= 시뮬레이션 결과 / 상태 변경 / 생산 결과 / 퀘스트 진행 push
```

---

### 22.1 production_tick_result

서버는 시뮬레이션 tick에서 생산 완료를 계산하고, 입력 소비 / 출력 생성 / 저장소 반영 후 클라이언트에 생산 결과를 push한다.

```json
{
  "message_type": "event",
  "event_type": "production_tick_result",
  "factory_id": "factory_001",
  "payload": {
    "tick": 128,
    "produced_items": [
      {
        "entity_id": "ent_smelter_001",
        "node_id": "node_iron_001",
        "sub_node_id": "sub_smelting_001",
        "item_id": "iron_ingot",
        "amount": 1
      }
    ],
    "consumed_items": [
      {
        "entity_id": "ent_smelter_001",
        "item_id": "iron_ore",
        "amount": 2
      }
    ],
    "inventory_delta": {
      "iron_ore": -2,
      "iron_ingot": 1
    },
    "inventory_current": {
      "iron_ore": 118,
      "iron_ingot": 9
    }
  }
}
```

클라이언트는 이 값을 다시 계산하지 않는다.  
클라이언트는 이 이벤트를 받아 UI, 이펙트, 생산 로그만 갱신한다.

---

### 22.2 inventory_updated

```json
{
  "message_type": "event",
  "event_type": "inventory_updated",
  "factory_id": "factory_001",
  "payload": {
    "items": {
      "iron_ore": 118,
      "iron_ingot": 9
    }
  }
}
```

---

### 22.3 factory_snapshot_updated

```json
{
  "message_type": "event",
  "event_type": "factory_snapshot_updated",
  "factory_id": "factory_001",
  "payload": {
    "snapshot_version": 42
  }
}
```

---

### 22.4 quest_progress_updated

```json
{
  "message_type": "event",
  "event_type": "quest_progress_updated",
  "factory_id": "factory_001",
  "payload": {
    "quest_instance_id": "qinst_001",
    "objective_id": "produce_iron_ingot",
    "current": 8,
    "target": 20
  }
}
```

---

## 23. FastAPI 프로젝트 구조

```text
server/
├─ app/
│  ├─ main.py
│  ├─ api/
│  │  ├─ factory_routes.py
│  │  ├─ command_routes.py
│  │  ├─ quest_routes.py
│  │  └─ websocket_routes.py
│  │
│  ├─ domain/
│  │  ├─ factory/
│  │  │  ├─ models.py
│  │  │  ├─ commands.py
│  │  │  ├─ state_store.py
│  │  │  └─ graph.py
│  │  │
│  │  ├─ simulation/
│  │  │  ├─ simulation_loop.py
│  │  │  ├─ power_simulator.py
│  │  │  ├─ machine_simulator.py
│  │  │  ├─ logistics_simulator.py
│  │  │  ├─ storage_simulator.py
│  │  │  └─ production_delta.py
│  │  │
│  │  ├─ analyzer/
│  │  │  ├─ factory_analyzer.py
│  │  │  └─ insight_models.py
│  │  │
│  │  └─ quest/
│  │     ├─ quest_composer.py
│  │     ├─ quest_manager.py
│  │     └─ quest_models.py
│  │
│  ├─ db/
│  │  ├─ session.py
│  │  ├─ models.py
│  │  ├─ repositories.py
│  │  └─ migrations/
│  │
│  └─ data/
│     ├─ machine_table_loader.py
│     └─ recipe_table_loader.py
```

---

## 24. 구현 순서

### 24.1 1단계 — PostgreSQL + SQLAlchemy 모델

먼저 핵심 테이블을 만든다.

```text
factories
factory_nodes
factory_sub_nodes
factory_powers
factory_power_sub_nodes
factory_entities
factory_ports
factory_connections
storage_states
machine_runtime_states
production_event_logs
quest_instances
quest_objective_progress
```

---

### 24.2 2단계 — Command 처리

클라이언트 명령을 하나의 엔드포인트에서 처리한다.

```text
POST /api/factories/{factory_id}/commands
```

우선 구현할 Command:

```text
create_node
create_sub_node
register_machine
set_recipe
connect_ports
create_power
create_power_sub_node
register_power_machine
connect_power_to_node
remove_entity
remove_connection
```

모든 Command Handler의 공통 책임:

```text
1. 요청 유저가 factory.owner_id와 일치하는지 권한 검증
2. machine_name / recipe_id / item_id를 마스터 데이터로 존재 검증
3. 가변 row 갱신 시 version 비교(낙관적 잠금)
4. remove_entity / remove_connection 시 다형성 참조 connection 명시적 정리
   (factory_connections의 from_id/to_id는 FK가 아니므로 CASCADE되지 않음)
```

---

### 24.3 3단계 — Simulation Loop

초기 시뮬레이션 루프를 만든다.

```text
Power Simulation
Machine Simulation
Storage Simulation
Logistics Simulation
ProductionDelta 생성
Inventory 갱신
ProductionEventLog 저장
WebSocket push
```

---

### 24.4 4단계 — Snapshot / Analyzer / Quest

서버 상태가 안정적으로 계산된 후 Snapshot과 Agent를 붙인다.

```text
FactorySnapshot
→ FactoryAnalyzer
→ FactoryInsight
→ QuestComposerAgent
→ QuestInstance
```

---

## 25. 최종 결론

Factory-Space 서버는 클라이언트 배치 좌표가 아니라 서버 논리 그래프를 기준으로 공장을 관리한다.

DB는 PostgreSQL + JSONB 기반으로 설계한다.

전체 공장 상태를 하나의 JSONB로 저장하지 않는다.  
시뮬레이션에 필요한 핵심 그래프 구조는 테이블로 분리한다.

```text
테이블로 분리
= Factory / Node / SubNode / Power / Entity / Port / Connection

JSONB로 저장
= summary / runtime / config / inventory items / snapshot / quest objective / reward / analysis result
```

서버는 좌표를 저장하지 않는다.  
좌표와 실제 배치는 클라이언트 LayoutData로 분리한다.

클라이언트는 새 ID를 만들지 않는다.  
클라이언트는 명령 JSON만 보낸다.

서버는 모든 영속 ID를 생성한다.  
서버는 MachineTable로 명령을 검증한다.  
서버는 공장 논리 그래프를 저장한다.

서버가 생산 / 전력 / 물류 / 저장을 시뮬레이션한다.

시뮬레이션 tick에서 생산 완료를 계산하고, 입력 소비 / 출력 생성 / 저장소 반영 후 `production_tick_result` 이벤트로 클라이언트에 알려준다.

클라이언트는 생산 계산을 하지 않는다.  
클라이언트는 서버가 push한 결과를 받아 화면만 갱신한다.

서버는 FactorySnapshot을 만든다.  
FactoryAnalyzer는 서버 Snapshot을 기준으로 문제를 분석한다.  
QuestComposerAgent는 분석된 문제를 현재 메인 퀘스트 목적에 맞는 지원 퀘스트로 묶는다.

초기에는 FastAPI 한 서버에서 API, WebSocket, Simulation Loop를 같이 돌린다.  
나중에 부하가 커지면 Simulation Worker와 Agent Worker를 분리한다.
