# Factory-Space 서버 구현 계획서

> 기반 설계: [factory_space_server_simulation_postgresql_jsonb_plan.md](../01_planning/factory_space_server_simulation_postgresql_jsonb_plan.md)
> 본 문서는 설계안을 실제 구현으로 옮기기 위한 단계별 작업 계획이다.
> 검토(2026-06-16)에서 확정한 수정사항(Source of Truth 분리, `owner_id`,
> `version` 낙관적 잠금, connection 정리, 파티셔닝 등)을 각 단계에 반영했다.

---

## 0. 기술 스택 및 원칙

```text
Language   = Python 3.12+
Web        = FastAPI
ORM        = SQLAlchemy 2.0 (async)
DB         = PostgreSQL 16
Migration  = Alembic
ID         = prefix + ULID (애플리케이션 생성)
Test       = pytest + pytest-asyncio
Lint       = ruff (check --fix / format)
```

핵심 불변 원칙:

```text
1. 서버는 좌표를 저장하지 않는다 (논리 그래프만).
2. 모든 영속 ID는 서버가 생성한다.
3. 런타임 값의 SoT는 *_runtime_states 테이블이다.
4. 가변 row 갱신은 version 낙관적 잠금으로 보호한다.
5. 한 공장의 시뮬레이션은 단일 워커가 직렬 처리한다.
6. 모든 외부 입력은 경계에서 검증한다 (소유권 + 마스터 데이터).
```

각 단계는 **테스트 우선(TDD)** 으로 진행하고, 단계 종료 시 `ruff check --fix .`
및 `ruff format .` 을 실행한다.

---

## Phase 1 — 기반 인프라 + DB 스키마

목표: 서버 부팅, DB 연결, 핵심 테이블 마이그레이션까지.

### 1.1 프로젝트 스켈레톤

```text
server/
├─ app/
│  ├─ main.py                # FastAPI 앱 + lifespan
│  ├─ config.py              # env 설정 (Pydantic Settings)
│  ├─ core/
│  │  ├─ ids.py              # prefix + ULID 생성기
│  │  ├─ errors.py           # 도메인 예외 + API 응답 envelope
│  │  └─ time.py             # UTC now 헬퍼
│  ├─ db/
│  │  ├─ session.py          # async engine / session factory
│  │  ├─ base.py             # DeclarativeBase + 공통 mixin
│  │  └─ models/             # 테이블별 ORM 모델 (파일 분리)
│  └─ ...
├─ alembic/
├─ tests/
├─ pyproject.toml
└─ .env.example
```

### 1.2 공통 모델 Mixin

```text
TimestampMixin   = created_at / updated_at (TIMESTAMPTZ)
VersionMixin     = version INTEGER (낙관적 잠금)
```

- `updated_at` 자동 갱신: DB `moddatetime` 트리거를 마이그레이션에서 생성.
- ULID 생성기는 `core/ids.py`에 prefix별 함수로 둔다
  (`new_factory_id()`, `new_entity_id()` ...).

### 1.3 마이그레이션 (Alembic) — 핵심 테이블

```text
factories                  (+ owner_id, version)
factory_nodes              (+ version)
factory_sub_nodes          (+ version)
factory_powers             (+ version)
factory_power_sub_nodes    (+ version)
factory_entities           (정의/설정만, + version)
factory_ports              (+ updated_at)
factory_connections        (+ version, 다형성 from/to)
storage_states             (free = GENERATED, + version)
machine_runtime_states     (status/progress/durability/buffer SoT, + version)
power_runtime_states       (+ version)
production_event_logs      (RANGE 파티셔닝)
quest_instances
quest_objective_progress   (+ UNIQUE(quest_instance_id, objective_id), + version)
factory_snapshots          (+ UNIQUE(factory_id, snapshot_version))
```

추가 작업:
- `moddatetime` 확장 + `updated_at` 테이블 트리거 일괄 생성.
- `production_event_logs` 파티셔닝 함수/초기 파티션 생성.
- 설계 19장 인덱스 + `idx_factories_owner_id` 적용 (GIN은 보류).

### 1.4 검증

```text
[ ] alembic upgrade head 성공
[ ] alembic downgrade -1 성공 (롤백 가능)
[ ] /health 엔드포인트 200
[ ] DB 연결 통합 테스트 통과
```

---

## Phase 2 — Repository 계층

목표: 시뮬레이션/Command가 의존할 데이터 접근 인터페이스 확정.

### 2.1 Repository 패턴

도메인별 Repository를 인터페이스로 정의하고 SQLAlchemy 구현을 둔다.

```text
db/repositories/
├─ factory_repository.py
├─ node_repository.py
├─ entity_repository.py
├─ port_repository.py
├─ connection_repository.py
├─ runtime_repository.py        # machine/power runtime states
├─ storage_repository.py
├─ quest_repository.py
└─ snapshot_repository.py
```

표준 연산: `find_by_id`, `find_by_factory`, `create`, `update`, `delete`.

### 2.2 낙관적 잠금

- `update` 시 `WHERE id = ? AND version = ?` 로 갱신, 영향 row 0이면
  `OptimisticLockError` 발생.
- 호출 측은 재조회 후 재시도 또는 명령 거부.

### 2.3 다형성 connection 정리 헬퍼

```text
delete_connections_referencing(ref_type, ref_id)
= entity / port 삭제 시 from/to로 그 id를 참조하는 connection 일괄 삭제
```

### 2.4 검증

```text
[ ] 각 repository CRUD 단위 테스트
[ ] version 충돌 시 OptimisticLockError 테스트
[ ] connection 정리 헬퍼 테스트 (entity 삭제 → 관련 connection 제거)
[ ] 커버리지 80%+
```

---

## Phase 3 — Command 처리

목표: 클라이언트 명령으로 논리 그래프를 구성/변경.

### 3.1 엔드포인트

```text
POST /api/factories/{factory_id}/commands
```

요청 envelope에 `command_type` + `payload`. 응답은 공통 envelope
(`success`, `data`, `error`).

### 3.2 Command Handler 공통 파이프라인

```text
1. 인증 → 요청 유저 식별
2. 소유권 검증 (factory.owner_id == user_id)
3. payload 스키마 검증 (Pydantic)
4. 마스터 데이터 검증 (machine_name / recipe_id / item_id 존재)
5. 도메인 적용 (version 낙관적 잠금)
6. 부수 정리 (remove 계열 → connection 정리)
7. 결과 반환 + 필요 시 상태 변경 이벤트 enqueue
```

### 3.3 우선 구현 Command

```text
create_node / create_sub_node
register_machine / set_recipe
connect_ports
create_power / create_power_sub_node
register_power_machine / connect_power_to_node
remove_entity / remove_connection
```

- `register_machine`은 MachineTable에서 포트 정의를 읽어 `factory_ports`를
  함께 생성한다.
- `connect_ports`는 flow_type / item_ids 호환성을 검증한다.
- `remove_entity`는 ports cascade + 다형성 connection 정리(2.3) 호출.

### 3.4 마스터 데이터 로더

```text
data/
├─ machine_table_loader.py
├─ recipe_table_loader.py
└─ item_table_loader.py
```

- 파일(JSON/YAML) 기반, 부팅 시 메모리 적재 + 검증.

### 3.5 검증

```text
[ ] 각 Command 단위 테스트 (정상 + 검증 실패 경로)
[ ] 타 유저 공장 명령 거부 (403) 테스트
[ ] 미존재 machine/recipe 거부 (422) 테스트
[ ] remove_entity 후 dangling connection 없음 통합 테스트
[ ] 커버리지 80%+
```

---

## Phase 4 — Simulation Loop

목표: 서버가 생산/전력/물류/저장을 직접 계산.

### 4.1 시뮬레이터 모듈

```text
domain/simulation/
├─ simulation_loop.py      # tick 오케스트레이션 (공장 단위 직렬)
├─ power_simulator.py      # 용량/사용량/margin → power_runtime_states
├─ machine_simulator.py    # progress/buffer/생산완료 → machine_runtime_states
├─ logistics_simulator.py  # connection 기반 아이템 이동
├─ storage_simulator.py    # storage_states 반영
└─ production_delta.py      # tick 결과 집계 모델
```

### 4.2 tick 순서 (설계 20장)

```text
Power → Logistics → Machine → Storage
→ ProductionDelta 생성
→ Inventory 갱신
→ ProductionEventLog 저장
→ (이벤트 enqueue)
```

### 4.3 직렬화/동시성

- 공장 단위 단일 워커. 같은 공장의 Command와 tick은 직렬 처리.
- 죽은 참조(정리 안 된 connection)는 무시하고 정리 대상으로 표시.

### 4.4 차등 주기

```text
1초   = 전력/기계 상태/progress
5초   = inventory 요약 갱신
30초  = FactorySnapshot 생성
60초  = Analyzer / QuestComposer
```

### 4.5 검증

```text
[ ] 시뮬레이터별 결정론적 단위 테스트 (고정 입력 → 기대 delta)
[ ] 전력 부족 시 기계 no_power 전이 테스트
[ ] 입력 부족/출력 막힘 정지 사유 테스트
[ ] 멀티 tick 시나리오 통합 테스트 (생산 누적 검증)
[ ] 커버리지 80%+
```

---

## Phase 5 — WebSocket Push

목표: 시뮬레이션 결과를 실시간 전달.

### 5.1 엔드포인트

```text
WS /ws/factories/{factory_id}
```

- 연결 시 소유권 검증 (factory.owner_id). 미인가 구독 거부.
- 명령은 REST, WS는 push 전용.

### 5.2 이벤트

```text
production_tick_result     (생산 완료 시)
inventory_updated          (5초)
factory_snapshot_updated   (30초)
quest_progress_updated     (진행 변경 시)
```

- ProductionDelta → WS 이벤트 + ProductionEventLog 변환 어댑터.
- 공장별 connection manager로 구독자 fan-out.

### 5.3 검증

```text
[ ] 미인가 유저 WS 연결 거부 테스트
[ ] tick → production_tick_result 수신 통합 테스트
[ ] 페이로드 스키마 계약 테스트
```

---

## Phase 6 — Snapshot / Analyzer / Quest

목표: 분석 기반 퀘스트 생성까지.

### 6.1 Snapshot

- 30초마다 `factory_snapshots`에 단조 증가 version으로 저장
  (`UNIQUE(factory_id, snapshot_version)` 보장).

### 6.2 Analyzer / Quest

```text
FactorySnapshot → FactoryAnalyzer → FactoryInsight
→ QuestComposerAgent → QuestInstance / quest_objective_progress
```

- `quest_objective_progress`는 ProductionDelta로 갱신, UNIQUE로 중복 방지.

### 6.3 검증

```text
[ ] snapshot version 단조 증가 + 중복 거부 테스트
[ ] analyzer 룰 단위 테스트 (issue_tags 도출)
[ ] quest 진행도 갱신 idempotency 테스트
```

---

## 단계 의존 관계

```text
Phase 1 (스키마) ─┬─> Phase 2 (repo) ─> Phase 3 (command)
                  │                          │
                  └──────────────────────────┴─> Phase 4 (simulation)
                                                       │
                                                       ├─> Phase 5 (websocket)
                                                       └─> Phase 6 (snapshot/quest)
```

Phase 1~4는 순차. Phase 5와 6은 Phase 4 이후 병렬 가능.

---

## 공통 완료 기준 (모든 Phase)

```text
[ ] TDD: 테스트 우선 작성 → 구현 → 통과
[ ] 단위 + 통합 테스트, 커버리지 80%+
[ ] ruff check --fix . / ruff format . 통과
[ ] 소유권/마스터 데이터 경계 검증 포함
[ ] 코드 리뷰 후 docs/04_reviews/ 에 리뷰 문서 작성
```

---

## 리스크 및 대응

| 리스크 | 영향 | 대응 |
|--------|------|------|
| 다형성 connection dangling | 시뮬레이션 죽은 참조 | remove 시 명시적 정리 + tick 방어 |
| Command·tick 동시 쓰기 충돌 | 상태 불일치 | 공장 단위 직렬화 + version 잠금 |
| production_event_logs 폭증 | 저장소/성능 | 파티셔닝 + 보존 정책 |
| 단일 서버 부하 한계 | tick 지연 | Simulation/Agent Worker 분리(후속) |
| 마스터 데이터 오타 | 잘못된 그래프 | Command 단계 존재 검증 + 거부 |
