# Dummy → OJJ 그리드 컨베이어 통합 계획서

> 목표: 공장의 머신 + 컨베이어를 **공간(그리드)**과 **전체 상태 집계(매니저)**로 책임 분리한다.
> - `AOJJ_Grid` = **공간/배치의 단일 소스** (셀 점유·머신 위치·컨베이어 경로).
> - `AFactoryManager`(`UGameInstanceSubsystem`) = **전체 상태 집계** (연결 그래프·스냅샷, 향후 전력·생산).
> 컨베이어 클래스는 팀 합의로 `ADummyConveyor` → **`AConveyor`로 rename 완료**(커밋 `1b22341`, `Conveyor.h/.cpp`). 그 외 Dummy 파일은 불가침.
> 그리드가 *기존* `AConveyor`를 **인지·등록**하고, 매니저가 그리드를 조회해 **연결을 집계**하는 것이 핵심.

## 진행 현황 (2026-06-02)
| Step | 상태 | 비고 |
|---|---|---|
| Step 1 — 저장 타입 일반화 | ✅ 완료·커밋 | `4e369bd` |
| Step 2 — 입력 포트 API | ✅ 완료·커밋 | `08701fd` |
| Step 3-a — 컨베이어 셀 등록/조회 | ✅ 완료·커밋 | `549e771` |
| Step 3-b — 경로/포트 인지 이식 | ⏳ 대기 | **I/O 이식 PR 후** |
| Step 3-c — 엔드포인트 연결 | ⏳ 블록 | **담당자 `AMachineBase` I/O 이식 PR 머지+풀 선행 필요** |
| Step 4·5 (매니저), 6 (입력) | ⏳ 대기 | 그리드 Step 3 완료 후 |

## 0-Z. 범위 원칙 (2026-06-02 재확정)
- **Dummy_* 파일 = 불가침(읽기 전용).** 통합은 **`OJJ_` 영역에만** 신설/작업한다. Dummy를 수정·폐기하는 단계는 **없다**(참고·이식 소스로만 읽음).
- **컨베이어는 예외 — 이미 처리됨.** 팀 합의로 `ADummyConveyor` → `AConveyor` rename 완료(`Conveyor.h/.cpp`, 커밋 `1b22341`, BP 보호 CoreRedirect 포함). 이후 단계는 `AConveyor`를 **그대로 사용**(추가 개명·폐기 없음).
- **`ADummyMachineBase` = 범위 제외(읽기만).** 머신베이스 담당자 영역 — 본 계획서는 참조만 하고 수정하지 않는다. 담당자가 아이템 I/O(4종)를 **`AMachineBase`로 이식하는 별도 PR**을 진행(3-c 선행). 이는 I/O를 베이스로 **올리는 것**이지 **Dummy 폐기가 아님** — `ADummyMachineBase`/`ADummyGrid` 등 Dummy는 **유지**된다.
- **폐기(deprecation) 단계 없음.** `ADummyGrid` 등 프로토타입 제거는 본 통합 범위 밖.

## 0-A. 아키텍처 — 책임 분리 (2026-06-02 회의 반영)
**집계 책임을 `AOJJ_Grid`가 아닌 별도 `AFactoryManager`로 분리한다.**

| 컴포넌트 | 형태 | 책임 |
|---|---|---|
| `AOJJ_Grid` | `AActor` (레벨 배치) | **공간/배치** — 셀 점유(`OccupiedCells`), 머신 위치/footprint(`OJJ_ActorToCells`/`OJJ_ActorToOrigin`), 컨베이어 경로 등록, 포트(입출력) 판정 |
| `AFactoryManager` | **`UGameInstanceSubsystem`** ✅ 확정 | **전체 상태 집계** — 그리드를 조회(derive-on-query)해 연결 그래프 구성, 스냅샷 API 제공, 향후 전력·생산 등 확장 |

- **매니저 형태 = `UGameInstanceSubsystem` 확정.** 웹소켓 클라(`OJJ_FactoryWSClient`)와 **동일 패턴**(레벨 독립, GameInstance 수명). 차후 WS 송신 주체와 자연 연결.
- **선후관계:** **그리드 작업(Step 1~3)을 모두 완료한 뒤** 매니저 단계(Step 4~5)로 진입. 매니저는 완성된 그리드 조회 API 위에 얹힌다.
- Step 1~3은 **소속/내용 변경 없음**(그대로 `AOJJ_Grid`). Step 4~5만 소유자가 `AFactoryManager`로 이동.

## 0. 확정 결정사항 (사용자)
- **머신 식별 = 좌표(`FIntPoint` Origin).** 안정적 id 없음, 포인터 직렬화 불가 → 모든 조회/연결은 좌표 키.
- **컨베이어 클래스는 rename(완료) 외 손대지 않는다.** `AConveyor`의 `SetPath` / `ConfigureTransport` / `OccupiedGridCells` 등 기존 인터페이스를 그대로 호출만 한다. (단 Step 3에서 `ConfigureTransport` 인자 1줄 완화 = c-1.)
- **직렬화(JSON)·웹소켓 전송은 범위 밖** — 다음 단계. 단, 스냅샷 USTRUCT는 *좌표 기반*이라 다음 단계에서 그대로 직렬화 가능하도록 설계.
- **향후 정보(전력·생산량 등)는 구조만 확장 가능하게, 지금은 미구현(YAGNI).**
- **이번 통합 범위 = Step 1~6.** (Step 6 = OJJ_Player·BuildController 컨베이어 입력. 폐기 단계 없음.)
- **제약:** `main` 미수정. 신규 심볼은 `OJJ_` prefix. Step마다 **빌드 통과 + `/codex:adversarial-review`** 통과 후 다음 Step.

## 1. 현황 요약 (조사 결과)
- `AOJJ_Grid` 저장: `TMap<FIntPoint, TWeakObjectPtr<AMachineBase>> OccupiedCells`, `TMap<TWeakObjectPtr<AMachineBase>, TArray<FIntPoint>> MachineToCells` — **머신만** 인지.
- 출력 포트 API 존재: `GetMachineOutputDir/Cells/Targets`, `CardinalFromVector`. **입력 포트 API 없음.**
- 컨베이어 인지 로직은 **`ADummyGrid`(프로토타입)에만** 완성: `TryPlaceConveyor`, `BuildConveyorPlacementPath`, `CanPlaceConveyorPath` + 익명 네임스페이스 헬퍼(`IsMachineBackOutputPair`, `IsMachineFrontInputPair`, `FindInputMachineAtPathEnd`, `CollectConveyorReservedCells`). 저장 타입이 `TWeakObjectPtr<AActor>`라 컨베이어를 셀에 담음.
- `AConveyor`(AActor 직속)는 **그리드를 역참조하지 않음** — 그리드가 일방적으로 구동.
- 아이템 전송은 **`ADummyMachineBase` 엔드포인트 필수**(아이템 I/O API가 거기 있음). `AMachineBase`만으로는 전송 불가 ← **놓치기 쉬운 의존성**.

## 1-A. 선결 확인 결과 — 등록 머신 타입 (읽기 전용, 코드 증거)
**결론: 현재 `AOJJ_Grid`에는 순수 `AMachineBase`가 등록된다 (ADummyMachineBase 아님).**

| 항목 | 코드 증거 | 결론 |
|---|---|---|
| 등록 주체 | `OJJ_BuildController` (production) | 이게 `AOJJ_Grid`에 머신 등록 |
| MachineClass 타입 | `OJJ_BuildController.h:55` `TSubclassOf<AMachineBase> MachineClass` | **`AMachineBase`로만 제약** |
| 실제 spawn/등록 | `OJJ_BuildController.cpp:282` `SpawnActor<AMachineBase>(MachineClass…)` → `:295` `TryPlaceMachine(NewMachine…)` | **순수 `AMachineBase`** 등록 |
| 실존 머신 | `AMinerMachine : public AMachineBase`, `AGrinder : public AMachineBase` | 둘 다 순수 AMachineBase |
| 아이템 I/O API | `PeekFirstOutputItem`/`TryTakeFirstOutputItem`/`CanReceiveConveyorItem`/`ReceiveConveyorItem` → `ADummyMachineBase.h:35-45`에만. `MachineBase.h`엔 없음(`AMachineBase : public AActor`) | I/O는 **Dummy 전용** |

**→ Step 3 직격 의존성:** 이식할 컨베이어 로직(`CollectConveyorReservedCells`)은 source/target이 `ADummyMachineBase`여야 통과한다. 현재 프로덕션 머신(Miner/Grinder)은 순수 `AMachineBase`라 **그대로는 컨베이어 연결이 무조건 실패**한다.

**Step 3 선결 결정 — 🔄 방향 변경(회의): 아이템 I/O를 `AMachineBase`로 이식 → 인터페이스 불필요(c-1 철회 검토):**
- **신 방침 (채택 후보):** 담당자가 `ADummyMachineBase`의 아이템 I/O(4종)를 **`AMachineBase`로 이식**하면, 모든 프로덕션 머신이 엔드포인트가 되어 **컨베이어 엔드포인트 = `AMachineBase*` 직접 사용**. → `IOJJ_ConveyorEndpoint` 인터페이스 **불필요**, **(c-1) 철회 검토**(`ConfigureTransport` 인자도 `AMachineBase*`로 단순화 가능).
  - 이는 이전에 기각했던 (a)에 가까우나, **담당자 영역의 I/O 이식 PR로 진행**되는 점이 다름(본 계획서가 머신베이스를 직접 수정하지 않음). Dummy는 **삭제 아님** — `ADummyMachineBase`의 I/O를 `AMachineBase`로 **올리는(이식)** 것이며 Dummy 클래스는 유지.
- ~~(c) UInterface `IOJJ_ConveyorEndpoint` / (c-1) 시그니처 완화~~ — I/O가 `AMachineBase`로 올라오면 경계용 인터페이스의 존재 이유가 사라짐. **보류/철회 검토**. (I/O 이식이 무산되면 (c)로 복귀.)
- ~~(b) reparent~~ — 기각 유지.

**⛓️ Step 3-c 선행 의존성 (외부 PR):** 컨베이어 엔드포인트 연결(Step 3-c)은 **담당자의 `AMachineBase` 아이템 I/O 이식 PR이 머지 + 풀(pull)된 뒤** 착수한다. 그 전까지 3-c는 블록.
> Step 1·2·3-a는 이 결정과 독립(이미 완료). Step 3-b/3-c는 I/O 이식 PR 선행.

---

## Step 1 — 저장 타입 일반화 (`AMachineBase*` → `AActor*`), 동작 무변경
**왜:** 컨베이어(`AConveyor : AActor`, 머신 아님)를 셀에 등록하려면 컨테이너가 `AActor`를 담아야 함. `ADummyGrid`가 이미 이 형태(`OccupiedCells: TWeakObjectPtr<AActor>`, `ActorToCells`).

**변경:**
- `OccupiedCells` → `TMap<FIntPoint, TWeakObjectPtr<AActor>>`
- `MachineToCells` → `OJJ_ActorToCells : TMap<TWeakObjectPtr<AActor>, TArray<FIntPoint>>`
- **신규 `OJJ_ActorToOrigin : TMap<TWeakObjectPtr<AActor>, FIntPoint>`** — origin을 `min(X),min(Y)` 재계산하지 않고 **등록 시점에 명시 저장**(Codex 지적: 비직사각형/이동·회전 후 min-recompute가 깨질 위험 제거). `GetMachineOrigin`은 이 맵을 조회, 미등록 시 `(INT_MIN,INT_MIN)` 센티넬 유지.
- 머신 전용 함수는 **시그니처 유지**하고 내부에서 `Cast<AMachineBase>`로 좁힌다:
  - `CalculateFootprint(AMachineBase*, …)` — 그대로(머신 footprint 전용). 컨베이어는 footprint가 아니라 PathCells 기반이라 **이 경로를 타지 않음**(Step 3에서 별도 등록 경로).
  - `GetMachineAtCell` — `Cast<AMachineBase>` 유지 → 컨베이어 셀은 `nullptr` 반환(의도된 동작).
  - `IsCellOccupied` — **현재 `GetMachineAtCell`에 위임(`OJJ_Grid.cpp:173`)하던 것을 끊고**, `OccupiedCells`의 `AActor` weak ptr 유효성으로 직접 판정 → 컨베이어 셀도 `true`.
  - `CanPlaceMachine`(`OJJ_Grid.cpp:327`) — 점유 검사를 머신타입이 아닌 **`AActor` 점유** 기준으로 변경 → 머신이 컨베이어 위에 겹치지 못하게.
  - `GetMachineOutputDir/Cells/Targets`, `GetMachineCells` — 입력은 여전히 `AMachineBase*`. 내부 맵 조회만 `OJJ_ActorToCells`로 교체.

**⚠️ 깨질 위험 (검토 포인트 직접 대응):**
1. **`IsCellOccupied` vs `GetMachineAtCell` 의미 분리.** 일반화 후 `IsCellOccupied`는 *컨베이어 셀도 true*여야 하고, `GetMachineAtCell`은 *컨베이어 셀은 nullptr*. 현재 둘이 같은 weak-machine-ptr로 판정 → **분기 명시 필요**. (회귀 1순위 지점)
2. **`GetMachineOutputTargets`의 self/dup 제거 + `Cast<AMachineBase>`** — 출력 타깃 셀에 컨베이어가 있으면 머신 캐스트 실패로 자동 제외됨(의도). "출력이 컨베이어에 연결"은 Step 4에서 별도 처리, 여기선 머신만 반환 유지.
3. **`SweepStaleEntries` / `RegisterMachineInternal`** 양방향 맵 정리가 `AActor` 키로 동작하는지(컨베이어 stale 포함) 회귀 확인.
4. **`RemoveMachine(AMachineBase*)` / `RemoveMachineAt`** — 머신만 제거. 컨베이어 제거는 Step 3에서 `OJJ_RemoveActorAt` 신설.

**검증:** 기존 머신 배치/제거/호버/출력타깃 동작 무변경 회귀(`OJJ_GridHoverSmokeTest` 활용). **빌드 + adversarial-review.**

**🚦 Step 2 진입 전 필수 게이트 — 컨베이어 셀 회귀 (Codex 보강):**
가짜/테스트 컨베이어(또는 AActor)를 한 셀에 등록한 뒤 **반드시** 아래 3개를 확인하고, 하나라도 실패하면 Step 2 진입 금지:
1. `IsCellOccupied(conveyorCell) == true`
2. `GetMachineAtCell(conveyorCell) == nullptr`
3. `CanPlaceMachine(머신, conveyorCell …) == false` (머신이 컨베이어 위 겹침 차단)
추가로 머신 셀 회귀: `IsCellOccupied=true / GetMachineAtCell=머신 / CanPlaceMachine=false` 무변경 확인.

**롤백:** 타입만 되돌리면 복구 가능(맵 키 타입 변경 + `OJJ_ActorToOrigin` 추가가 침습 범위).

---

## Step 2 — 입력 포트 API 신설 (출력 포트의 대칭)
**왜:** 컨베이어 끝단이 "머신 입력 포트"에 닿는지 판정하려면 입력 방향/셀이 필요. 현재 출력만 존재.

**추가 (OJJ_ prefix, 기존 컨벤션 = 입력은 머신 앞면 +Front):**
- `OJJ_GetMachineInputDir(AMachineBase*) : FIntPoint` (= `+Front` 카디널, `GetMachineOutputDir`의 부호 반전 재사용)
- `OJJ_GetMachineInputCells(AMachineBase*) : TArray<FIntPoint>` (footprint의 Front쪽 모서리 +InputDir 이웃)
- 기존 `GetMachineOutputCells` 구현을 방향 인자화하여 **출력/입력 공유**(중복 로직 방지).

**좌표 식별 검증 포인트:** 멀티셀·회전 머신에서 입력/출력 셀 산출이 `EffectiveSize`(회전 step swap)와 일관되는지 — 출력 로직이 이미 footprint 모서리 기반이라 회전 무관. 입력도 동일 규칙 재사용으로 보장.

**검증:** 1×1, 2×1, 2×2, 회전 0/1/2/3 케이스 입력셀 산출 단위 확인. **빌드 + adversarial-review.**

---

## Step 3 — 그리드가 기존 `AConveyor`를 인지·등록 (클래스 무변경)
**왜:** 컨베이어 경로 유효성 판정 + 셀 점유 등록을 `AOJJ_Grid`로 가져온다. **`AConveyor`는 그대로**, 그리드가 그 actor에 대해 `SetPath`/`ConfigureTransport`만 호출.

**하위 분할:** **3-a ✅(완료·커밋)** 그리드 셀 등록/조회(`OJJ_GetConveyorAtCell`/`OJJ_RegisterActorCells`/`OJJ_RemoveActorAt`) · **3-b** 경로/포트 인지 로직 이식 · **3-c** 엔드포인트 연결(아이템 transport).

**선결 (3-b/3-c):** 🔄 1-A 방향 변경 — 아이템 I/O가 `AMachineBase`로 이식되면 **엔드포인트 = `AMachineBase*` 직접 사용**, `IOJJ_ConveyorEndpoint`/(c-1) **불필요(철회 검토)**.
- `AConveyor::ConfigureTransport`의 source/target 인자는 `AMachineBase*`로 단순화 가능(I/O가 베이스에 있으므로).
- **⛓️ 3-c는 담당자의 `AMachineBase` 아이템 I/O 이식 PR 머지+풀 선행 필요**(그 전까지 블록).

**추가 (Dummy_GridConveyor.cpp 로직을 OJJ_ 메서드로 가져옴, 컨베이어/머신 클래스 미변경):**
- 익명 네임스페이스 헬퍼 이식: `OJJ_*` — `GetMachineBackStep/FrontStep`, `IsMachineBackOutputPair`, `IsMachineFrontInputPair`, `FindInputMachineAtPathEnd`, `CollectConveyorReservedCells`.
- 그리드 메서드: `OJJ_BuildConveyorPlacementPath`, `OJJ_CanPlaceConveyorPath`, `OJJ_TryPlaceConveyor(AConveyor*, PathCells, OutReason)`, `OJJ_RemoveActorAt(FIntPoint)`.
- `OJJ_TryPlaceConveyor` 내부: `OJJ_RegisterActorCells(Conveyor, ReservedCells)` → `Conveyor->SetActorLocation(...)` → `Conveyor->SetPath(...)` → `Conveyor->ConfigureTransport(cells, source, target)`. **(전부 기존 AConveyor 공개 API)**

**⚠️ 빠뜨리기 쉬운 의존성 (검토 포인트 직접 대응):**
1. **엔드포인트 타입(1-A).** `CollectConveyorReservedCells`는 source/target이 아이템 I/O 가능 타입이어야 통과. 현재 프로덕션 머신은 순수 `AMachineBase` → 1-A 결정 미반영 시 **항상 실패**.
2. **`Grid->GridToWorld` / `IsValidGridCell` 의존** — 헬퍼가 그리드 좌표 변환을 호출. `AOJJ_Grid`의 동명 함수로 바인딩되는지(시그니처 동일) 확인.
3. **포트 컨벤션 일치** — Dummy는 "출력=뒤(-Front), 입력=앞(+Front)". `AOJJ_Grid::GetMachineOutputDir`도 `-Front`. Step 2 입력 API와 **부호 컨벤션 충돌 없는지** 교차 확인.
4. **충돌/연속성 검증** — `ManhattanDistance==1` 연속성, 점유 충돌, "입력 포트 직전 셀은 비어야 함" 규칙 누락 금지.
5. **컨베이어 제거 시 양방향 맵 정리** — `OJJ_RemoveActorAt`가 `OccupiedCells` + `OJJ_ActorToCells` 동시 정리.

**검증 — ADummyGrid parity 8케이스 (Codex 보강, line-by-line 동작 일치 확인):**
이식한 `AOJJ_Grid` 컨베이어 로직이 기존 `ADummyGrid`와 동일 결과를 내는지 8케이스 대조:
1. **start-on** — 머신 출력 셀에서 시작(정상)
2. **start-adjacent** — 머신 인접 셀에서 시작(정상)
3. **end-on** — 머신 입력 셀에서 종료(정상)
4. **end-adjacent** — 머신 입력 인접에서 종료(정상)
5. **blocked** — 경로가 점유 셀로 차단(실패)
6. **non-contiguous** — `ManhattanDistance≠1` 비연속(실패)
7. **self-target** — source==target 동일 머신(실패)
8. **wrong-side** — 머신의 입력/출력이 아닌 면에 접함(실패)
+ "입력 포트 직전 셀은 비어야 함", reserved 셀이 머신 시작/끝 셀 제외·경로 셀 포함 규칙 일치 확인. **빌드 + adversarial-review.**

---

## Step 4 — 연결 그래프를 `AFactoryManager`가 집계 (소속 이동: 그리드 → 매니저)
**왜:** "A(출력) →[컨베이어]→ B(입력)" 토폴로지를 **매니저가** 권위 있게 집계한다. 그리드는 공간 정보만 제공하고, 연결 그래프 구성은 매니저 책임(0-A 분리).

> **소속 변경 (회의 반영):** 연결 그래프는 ~~`AOJJ_Grid`~~ → **`AFactoryManager`(`UGameInstanceSubsystem`)**가 보유. 매니저가 그리드를 조회(derive-on-query)해 집계.

**선결:** Step 1~3(그리드) 완료 후 진입. 매니저는 완성된 그리드 조회 API에 의존.

**통지 방식 — ✅ 2번(이벤트에 데이터 실어 증분 갱신)으로 팀 합의.** 상세 이벤트 명세(종류/페이로드/식별키/재동기화 안전장치)는 Step 4에서 설계. Step 1~3(그리드 공간 작업)과는 독립.

**설계 결정 — 파생 vs 저장:** 기본은 **파생(derive-on-query)**. 연결은 컨베이어의 `OccupiedGridCells` 양끝 + source/target에서 계산 가능 → 매니저가 별도 상태 중복 저장 안 함(불변식 깨질 여지 최소화, YAGNI). 성능 이슈 시에만 매니저 측 캐시.
- 그리드 측 조회 헬퍼: `OJJ_GetConveyorAtCell(FIntPoint) : AConveyor*` (공간 정보 — 그리드 소속).
- 매니저가 그리드(들)를 순회하여 연결쌍(SourceOrigin→TargetOrigin) 집계 → Step 5 스냅샷으로 노출.

**검증:** 다중 컨베이어/분기 시 매니저 집계 연결쌍 정확성. **빌드 + adversarial-review.**

---

## Step 5 — 확장형 USTRUCT 스냅샷 조회 API (좌표 기반, `AFactoryManager` 소속)
**왜:** 전체 상태를 외부(향후 AI/WS)가 읽을 표준 read-only 표면. **좌표 식별**이라 다음 단계 직렬화에 그대로 사용.

> **소속 변경 (회의 반영):** 스냅샷 API는 ~~`AOJJ_Grid::OJJ_GetGridSnapshot`~~ → **`AFactoryManager::GetFactorySnapshot`**로 이동. USTRUCT 설계(`FOJJ_GridSnapshot` 등)는 **그대로 유지**, 소유자만 매니저. 매니저가 그리드를 조회해 스냅샷을 구성.

**USTRUCT (BlueprintType, OJJ_ prefix):**
```cpp
UENUM() enum class EOJJOccupantType : uint8 { None, Machine, Conveyor };

USTRUCT() struct FOJJ_MachineSnapshot {
    FIntPoint Origin;               // 식별 = 좌표
    TArray<FIntPoint> Cells;        // footprint
    FIntPoint OutputDir; TArray<FIntPoint> OutputCells;
    FIntPoint InputDir;  TArray<FIntPoint> InputCells;
    // 향후: Power/Production 등 — 지금 미구현(YAGNI), 필드만 추후 추가
};
USTRUCT() struct FOJJ_ConveyorSnapshot {
    TArray<FIntPoint> PathCells;
    TArray<FIntPoint> OccupiedCells;
    FIntPoint SourceMachineOrigin;  // 좌표 참조(포인터 아님)
    FIntPoint TargetMachineOrigin;
    bool bHasValidSource = false;   // (Codex 보강) stale/미등록 시 false
    bool bHasValidTarget = false;
};
USTRUCT() struct FOJJ_GridSnapshot {
    TArray<FOJJ_MachineSnapshot> Machines;
    TArray<FOJJ_ConveyorSnapshot> Conveyors;
};
```
- `UFUNCTION(BlueprintCallable) FOJJ_GridSnapshot AFactoryManager::GetFactorySnapshot() const;` — 매니저가 그리드(들)를 순회하여 머신/컨베이어 각각 채움. 컨베이어의 source/target은 **Origin 좌표로 환산**(포인터 비직렬화 회피).
- **🚫 sentinel 계약 (Codex 보강):** `AConveyor`는 source/target을 **weak ptr**로 보관(`Conveyor.h`)이라 stale 가능. 스냅샷 생성 시 endpoint가 유효·등록됨이면 `bHasValidSource/Target=true` + 실제 Origin, **무효/미등록이면 `false`로 두고 `INT_MIN` origin을 무음으로 방출하지 않는다**(소비자가 `bHasValid*`로 분기). 직렬화는 범위 밖이지만 **이 in-memory sentinel 계약은 지금(마지막 범위 Step) 확정**한다.
- **확장 규칙:** 향후 정보는 USTRUCT에 필드 추가만으로 확장(구조 안정). **지금은 추가 안 함.**
- **범위 밖 명시:** JSON 직렬화·WS 송신은 다음 단계. 스냅샷은 순수 데이터 반환까지.

**검증:** 스냅샷이 그리드 실제 상태와 일치(머신 N개·컨베이어 M개·연결쌍). **빌드 + adversarial-review.**

---

## Step 6 — `OJJ_Player` · `OJJ_BuildController` 컨베이어 입력 (OJJ_ 영역 신설)
**왜:** 플레이어가 빌드모드에서 **컨베이어를 드래그로 배치**할 수 있게 한다. 현재 `OJJ_BuildController`는 머신 단일 모드, `OJJ_Player`는 컨베이어 입력이 없다. Dummy(`ADummyBuildController`/`ADummyPlayer`)는 **참고 소스로만 읽고**, 구현은 `OJJ_` 쪽에 신설.

**선결:** Step 3(그리드 컨베이어 인지·`OJJ_TryPlaceConveyor`) 완료 — 입력이 호출할 배치 API가 있어야 함.

**추가 (`OJJ_` 영역만 수정, Dummy 불가침):**
- `AOJJ_BuildController`: 배치 모드 enum(`EOJJ_BuildPlacementMode { Machine, Conveyor }`) + 컨베이어 드래그 상태(`ConveyorDragCells`) + 드래그 시작/갱신/커밋(`OJJ_TryPlaceConveyor` 호출) + 경로 호버 프리뷰.
- `AOJJ_Player`: 입력 액션 신설 `IA_SetMachineMode`/`IA_SetConveyorMode`(모드 전환), 좌클릭 드래그 릴리즈/취소 핸들러(`BuildPlaceReleased`/`BuildPlaceCanceled`). `IMC_Build`에 매핑.
- 머신 배치 경로는 **무변경 회귀**(모드=Machine이 기존 동작).

**검증:** 컨베이어 모드 진입 → 드래그 → 배치 happy/실패 경로, 머신 모드 무변경 회귀. **빌드 + adversarial-review.**

> 참고: Dummy의 입력/드래그 흐름(`UpdateConveyorDrag`/`CommitConveyorDrag` 등)은 **읽기 전용 참고**. `OJJ_`로 옮겨 구현하며 Dummy 파일은 수정하지 않는다.

---

## 단계 순서 근거
**[그리드] 1→2→3 · [매니저] 4→5 · [입력] 6** (총 Step 1~6, 폐기 단계 없음):
컨테이너 일반화(1) 없이는 컨베이어 등록 불가 → 입력 API(2) 없이는 경로 끝단 판정 불가 → 인지/등록(3) 없이는 연결 없음 → **(여기까지 `AOJJ_Grid`)** → 완성된 그리드 위에 `AFactoryManager`가 연결 집계(4) → 그 위에 스냅샷(5) **(여기부터 `AFactoryManager`)** → Step 3의 배치 API 위에 플레이어 컨베이어 입력(6) **(`OJJ_Player`/`OJJ_BuildController`)**. 각 Step은 **이전 Step의 회귀가 통과해야** 진행. (Step 1·2는 1-A 결정과 독립; Step 3 진입 전 1-A 확정 필요. Step 4·Step 6 진입 전 그리드 Step 1~3 완료 필요. Step 6은 4·5와 독립이라 순서 무관.)

## 빠진 선결조건 체크리스트 (Codex 검토용)
- [ ] 머신 등록 경로가 컨베이어 엔드포인트 가능 타입을 보존/제공하는가(1-A 결정)?
- [ ] `IsCellOccupied`/`GetMachineAtCell` 의미 분리 회귀 없는가?
- [ ] 입력/출력 포트 부호 컨벤션(±Front) 일관한가?
- [ ] 멀티셀·회전에서 입력셀 산출이 `EffectiveSize`와 일관한가?
- [ ] 컨베이어 제거 시 양방향 맵 누수 없는가?
- [ ] 스냅샷 source/target Origin 환산이 미등록/stale 머신에서 센티넬 처리되는가?

## 비범위 (이번 통합 아님)
- **Dummy_* 파일 수정/폐기** — 불가침(읽기 전용). `ADummyGrid` 등 프로토타입 제거 단계 없음.
- **`ADummyMachineBase` 수정** — 머신베이스 담당자 영역, 읽기만.
- 컨베이어 추가 개명/이식 — rename은 이미 완료(`AConveyor`, 커밋 `1b22341`). 추가 작업 없음.
- JSON 직렬화·웹소켓 송수신.
- 전력·생산량 등 향후 정보 구현(구조만 확장 가능).
- `main` 브랜치 수정.

> ※ 이전 "비범위"였던 **플레이어 컨베이어 입력**은 이번에 **Step 6으로 범위 편입**됨.
