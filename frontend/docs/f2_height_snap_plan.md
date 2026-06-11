# F2 계획 — 지형 높이 통합: 직배치 Z(최고점) + Foundation 높이 스냅(§5-4)

작성일: 2026-06-11 · 브랜치 OJJ · 선행: F1-b'(e80248e) Codex 사후 리뷰 완료(3 PASS / 1 BUG — §0-a로 편입)

> 참조: F2 백로그 원문은 `OJJ_Grid.cpp:1306-1314` 주석(OJJ_GetUniformSurfaceZ). 결정 (a) "셀 대표높이
> 최악점→최고점" 채택, 직배치는 §5-2대로 F3까지 유지.

---

## 0. 선행 정리 (작고 독립 — 첫 커밋)

### 0-a. [BUG·Codex 사후 리뷰] 철거 호버 게이트가 클릭 거부 조건과 불일치
- 증상: 빈 Foundation 셀 호버 → Foundation 전체 빨강(철거 가능 표시) → 클릭하면 다른 셀 위
  머신/컨베이어 때문에 `RemoveFoundation` 거부 (OJJ_Grid.cpp:1245-1249). 호버≠클릭 판정.
- 수정: 그리드에 read-only 헬퍼 `OJJ_CanRemoveFoundation(AActor*) const` 신설(위 건물 점유 검사 =
  RemoveFoundation:1245 루프와 동일 식) → `RemoveFoundation`과 `UpdateDemolishHover`(BuildController:556-579)가
  공유. 단일 진실원 계약(호버 색 = 클릭 판정)을 철거 모드에도 적용.
- 결정 ⓐ ✅(i) 하이라이트 생략 확정. 보강: **"철거 거부 사유의 화면 표시(현재 로그뿐)"는 UI 백로그로
  태그** — §9.5 UI/UX 계열, 토스트/커서 툴팁 등 표현 수단은 UI 패스에서 결정.

### 0-b. [동봉] 호버 MID 가짜 diff 차단 — RF_Transient
- `OJJ_EnsureTileMIDs`의 EnsureMID 람다(OJJ_Grid.cpp:2550) Create 직후 `MID->SetFlags(RF_Transient);` 1줄
  → 호버/오버레이 MID 5종이 레벨 dirty(가짜 diff)를 만들지 않음.
- 동일 패턴 권고(범위 결정점 ⓑ): `InputArrowMID/OutputArrowMID`(OJJ_Grid.cpp:522-523)도 같은 벡터 —
  같이 처리 추천(+2줄). WaterArea의 WaterMID는 타 소유 영역이라 태그만.

---

## 1. 지형 직배치 Z — 셀 대표높이 최악점→최고점 (결정 (a) 반영)

### 베이크 변경 (OJJ_Grid.cpp 베이크 루프 723-790)
- 5점 샘플에서 **최고점 부호 델타**(`MaxSignedDelta = max(Hit.Z − PlaneZ)`) 추적 추가,
  `GroundZTmp[Idx]`에 기존 `WorstSignedDelta`(:775) 대신 최고점 저장.
- **결정점 ①: blocked 분류 기준은 최악점(|델타| 최대) 유지 (추천)**
  - 유지 시: blocked 집합 불변(회귀 0), GroundZ 대표값만 바뀜. 절벽/구덩이 셀은 계속 차단.
  - 최고점으로 바꾸면: 아래로 꺼진 셀(구덩이·절벽 끝)이 buildable로 풀려 액터가 허공에 걸침 — 비추천.
- **결정점 ②: 캐시 강제 무효화** — GroundZ 의미가 바뀌어도 기존 시그니처(크기/톨러런스/원점)는 동일해
  자동 무효화가 안 됨. 시그니처에 베이크 버전 int 1개 추가(예: `CacheBakeVersion`) **추천** — 옛 캐시를
  들고 있는 맵이 자동 재베이크 유도. (수동 재베이크 약속만으로 가면 stale 의미 혼용 위험.)
- `DumpGroundZReport` 라벨 "최악점"→"최고점", 헤더 주석(OJJ_Grid.h:107-108, 1358) 갱신.

### 소비처 변경
- **`OJJ_GetUniformSurfaceZ` 지형 경로**(:1315): `OutZ = 평면` → `평면 + max(GroundZ over Cells)`
  (GroundZ 유효 시 — `OJJ_HasValidGroundZData`; 무효 시 평면 폴백 = 회귀 0).
  - 균일 취급은 유지(지형 셀끼리 GroundZ 달라도 거부하지 않음) — §5-2 직배치 비파괴 원칙.
  - **결정점 ③: 풋프린트 대표 = max (추천)** — 묻힘 0(결정 (a)의 목적), 뜸은 ≤ tol+셀간차로 유계.
    평균은 묻힘이 되살아나 기각.
- 머신 `GetMachinePlacementLocation`(:633)·컨베이어 `PathSurfaceZ`(:2182)는 자동 수혜(코드 변경 0).
  - 긴 컨베이어는 경로 최고 셀 기준으로 전체가 떠오름(벨트 평탄 불변) — F2 수용, 세그먼트 분할은 F3.
- 비주얼 `OJJ_GetCellVisualBaseZInternal`(:1372): 타일이 셀 최고점에 위치 → 경사면 교차(줄무늬) 구조 해소.
  **VisualZLift 0 재검증**(귀퉁이 ±0.4셀 밖 미샘플 잔존 교차 가능 — 실측 후 결정).

### 검증
재베이크 → F1-c에서 묻힘 확인된 경사 셀에 머신/컨베이어 직배치 → 묻힘 0 / 뜸 ≤ tol 실측.

---

## 2. [본론] Foundation 높이 스냅 (§5-4) — 1m 단위 N단

### 단 격자 정의 — 결정점 ④ (핵심)
- **추천: 상면 Z = 평면 + Thickness + N×100** (N ≥ 0 정수, 100uu = 1m)
  - `N = clamp(ceil((풋프린트 GroundZ 최고점 − Thickness) / 100), 0, ∞)` → 상면이 항상 지형 최고점
    이상(묻힘 0)이고 초과 < 100. 평탄 지대 N=0 = **현행 F1 동작 그대로(회귀 0)**.
  - 대안(상면 = 평면 + N×100 순수 격자): 평탄 지대 상면이 +50→+100으로 변해 기존 배치·걷기 전부
    흔들림 — 기각.
- GroundZ 무효(미베이크) 맵: N=0 폴백 — 회귀 0.
- 결정 ⑤ ✅ 상한 없음 확정. 보강: **배치 성공 로그에 N값 포함**(BuildController 배치 성공 로그에
  `N=%d` 추가) — 분포 실측 데이터로 상한 재검토 근거 축적.

### 산출 책임 — 데이터/좌표=그리드, 액터 이동=컨트롤러 (F1-b 결정점 ② 계약 유지)
- 그리드 헬퍼 신설: `float OJJ_ComputeFoundationSnapLift(FIntPoint Origin, FIntPoint Size, float Thickness) const`
  → N×100 반환(무효 시 0). 풋프린트 GroundZ 순회는 그리드 내부(`GetCellGroundZ` 첫 실소비).
- `PlaceFoundationAtCursor`(BuildController:957-970):
  `PlaceLocation.Z += SnapLift;` → `TryPlaceFoundation(…, PlaceLocation.Z + Thickness, …)` —
  **TryPlaceFoundation 시그니처·셀별 SurfaceZ 저장 구조 변경 0** (이미 임의 SurfaceZ 수용).
- Foundation 액터는 통째로 +N×100 이동 — `UpdateSlabVisual` 불변(상면 = 액터Z + Thickness 유지).
- 결정 ⑥ ✅(i) 프로토 수용 확정. 보강: **"슬래브 하부 갭으로 캐릭터 진입 가능 여부"를 §2 검증의
  PIE 관찰 항목에 추가** — 진입 가능하면 갇힘/시야 문제 여부 기록 후 (ii) 스커트 재검토 트리거.

### SurfaceZ 규칙/컨베이어와의 연동 — 변경 0으로 성립 (F1-c 단일원 통일의 보상)
- `OJJ_GetUniformSurfaceZ`는 셀에 저장된 SurfaceZ만 비교(:1336-1340) →
  - 같은 단 Foundation 위(같은 Foundation이든 같은 N의 인접 Foundation이든): 균일 통과 — 머신/컨베이어
    배치 그대로 동작.
  - **다른 단 걸침: SurfaceZ 불일치로 자동 거부** — 이미 구현된 "이높이 Foundation 거부" 규칙이 N단을
    자연 처리. 단차 연결(경사 컨베이어/계단)은 F3.
  - 지형↔Foundation 혼합 걸침: 거부(현행 유지).
- 머신 Z(SurfaceZ+AABB 보정)·컨베이어 들어올림(:2183)·호버/비주얼 타일(`GetFoundationSurfaceZ`)·
  철거(위 건물 게이트): 전부 저장 SurfaceZ 소비라 자동 추종 — **그리드측 코드 변경 0**.
- `CanPlaceFoundation`(:1118): blocked(높이초과) 셀은 원래 게이트에 없음(void/water/overlap/occupied만)
  → 경사지 개척(blocked 지형 위 Foundation) 시나리오가 게이트 변경 없이 성립. F1의 "blocked 위 슬래브
  묻힘" 잠재 문제도 스냅이 해소.
- **결정점 ⑦: 호버 프리뷰** — F2 최소 = 현행 셀 타일(높이 미표현, 클릭 후 실물 확인) **추천**.
  고스트 슬래브 메시는 F2 범위 밖.

### 검증
경사 지대 배치 → 상면 = 평면+Thickness+N×100 ∧ ≥ 지형 최고점 / 같은 단 위 머신·컨베이어 배치 /
단차 걸침 거부 / 철거 회귀(0-a 포함).

---

## 3. Thickness 50 vs 45 — ✅ 결정 완료 (2026-06-11): 45 확정

- 사실관계: 캐릭터 `MaxStepHeight` UE 기본 45 → Thickness 50은 평지→Foundation 도보 진입 불가, 45는 가능.
- **결정 ⑧ 확정: Thickness 45** — 플레이어 진입 성립, 값 1곳(OJJ_Foundation.h:49).
  - NavMesh `AgentMaxStepHeight=35`(DefaultEngine.ini:19)는 **AI/NPC 미사용 확인(팀 협의 불필요)으로
    동기화 불필요** — 변경 없음.
  - PIE 걷기 검증은 F2-3 커밋 시 확인 절차로만 수행(결정 변수 아님).
- 스냅 산식(§2)의 BaseLift = 45로 확정 입력. N단(100) 차이는 도보 불가 — 계단/램프 F3.

---

## 4. 톨러런스 50→100 재검토 — (a) 적용 후 실측과 묶음

- `BuildableHeightTolerance`(OJJ_Grid.h:168) 50→100: blocked 완화 → buildable 비율 증가.
  `CacheHeightTolerance`가 시그니처라 변경 시 캐시 자동 무효 — **항목 1 재베이크와 같은 사이클로 묶음**.
- 부작용: 직배치 뜸 상한 ~50→~100(최고점 기준이라 묻힘은 계속 0), VisualZLift 재튜닝 폭 증가. water 판정 무관.
- 절차: ① 항목 1 적용 베이크(tol 50)에서 비율 실측(베이크 로그+GroundZReport) → ② tol 100 재베이크
  실측 → 두 수치를 근거로 채택. 에디터 베이크 2회라 변인 분리 비용 낮음.

---

## 실행 순서 / 커밋 단위 / 수정 범위

| 순서 | 커밋 | 내용 | 파일 |
|---|---|---|---|
| 1 | F2-0 | 0-a 호버 게이트 BUG + 0-b RF_Transient | OJJ_Grid.h/.cpp, OJJ_BuildController.cpp |
| 2 | F2-1 | 베이크 최고점(+버전 시그니처) + GetUniformSurfaceZ 지형 GroundZ 소비 + 재베이크·실측 | OJJ_Grid.h/.cpp, 레벨 재베이크 |
| 3 | F2-2 | tol 100 채택 여부(실측 근거) | OJJ_Grid.h(기본값), 재베이크 |
| 4 | F2-3 | Thickness 45 적용(✅ 결정 완료 — PIE 걷기는 확인 절차만) | OJJ_Foundation.h |
| 5 | F2-4 | 높이 스냅 — SnapLift 헬퍼 + 컨트롤러 적용 | OJJ_Grid.h/.cpp, OJJ_BuildController.cpp |

결정점 현황 (2026-06-11 전부 확정 — 추천안 일괄 승인):
ⓐ 철거불가 호버 생략(+거부 사유 화면 표시는 UI 백로그) · ⓑ ArrowMID 동봉 · ① 분류 최악점 유지 ·
② 캐시 버전 필드 추가 · ③ 풋프린트 대표 max · ④ 단 격자 = 평면+Thickness+N×100 ·
⑤ N 상한 없음(배치 로그에 N 기록) · ⑥ 슬래브 갭 수용(+PIE 캐릭터 진입 관찰) · ⑦ 호버 타일 유지 ·
⑧ Thickness 45(AI/NPC 미사용, Nav 동기화 불필요)

UI 백로그(F2 범위 밖 태그): 철거/배치 거부 사유의 화면 표시 — 현재는 로그(OutReason)뿐. 토스트·커서
툴팁 등 표현 수단은 UI/UX 패스(§9.5)에서 결정.
