# M_Hologram_BuildUp 머티리얼 제작 가이드 (#3 점진 건설)

건물 배치 시 "아래→위 홀로그램 빌드업" 연출용 머티리얼. **에디터에서 수동 제작**(C++로는 그래프 생성 불가).
C++ 측은 `UOJJ_HologramBuildUpComponent`가 프록시 메시에 이 머티리얼의 MID를 입히고 `Progress`/`MinZ`/`MaxZ`를 매 틱 주입한다.

## 룩 (방법 1)
- 경계(`Progress`) **위** = 시안 홀로그램(불투명, 실제 메시 가림)
- 경계 **근처** = 주황(#E8902A) 스캔라인 강발광
- 경계 **아래** = 투명(OpacityMask) → 실제 메시 노출
- 방향: `Progress` 0→1 → 위 덮임 영역이 위로 줄며 **아래부터 실제가 드러남**

## 머티리얼 설정
- **경로**: `/Game/OJJ/Materials/M_Hologram_BuildUp` (제작 후 `BP_BuildController` 인스턴스의 `HologramBuildUpMaterial` 슬롯에 지정)
- **Blend Mode**: `Masked` (Opacity Mask Clip Value 0.5)
- **Shading Model**: `Unlit` 권장(홀로그램 발광 룩) — 또는 Default Lit + Emissive
- **Two Sided**: ✅ (얇은 경계 잘림 방지)

## 파라미터 (C++가 주입하거나 디자이너 튜닝)
| 이름 | 타입 | 주입 | 기본/예 |
|---|---|---|---|
| `Progress` | Scalar | **C++ 매 틱** | 0→1 |
| `MinZ` | Scalar | **C++(배치 시)** | 건물 바운드 하단 월드 Z |
| `MaxZ` | Scalar | **C++(배치 시)** | 건물 바운드 상단 월드 Z |
| `ScanlineWidth` | Scalar | 디자이너 | 0.04 |
| `HologramColor` | Vector | 디자이너 | 시안 (0, 0.8, 1) |
| `ScanlineColor` | Vector | 디자이너 | 주황 #E8902A ≈ (0.91, 0.565, 0.165) |
| `EmissiveBoost` | Scalar | 디자이너 | 8 |

## 노드 구성

### 1) 정규화 높이
```
NormZ = saturate( (WorldPosition.Z - MinZ) / (MaxZ - MinZ) )
```
- `WorldPosition`(Absolute World Position) Z 성분 → (− MinZ) → (÷ (MaxZ − MinZ)) → `Saturate`.

### 2) OpacityMask (경계 위 보임, 아래 투명)
```
Mask = (NormZ > Progress) ? 1 : 0
```
- `If` 노드: A=`NormZ`, B=`Progress`, A>B → 1, A==B → 1, A<B → 0 (또는 `NormZ - Progress` → `Ceil`/`step`).
- 출력 → **Opacity Mask**. (Clip Value 0.5)
- 결과: 경계 위(NormZ>Progress)는 렌더, 아래는 클립 → 실제 메시 비침.

### 3) 색 / 발광 (경계 스캔라인)
```
Dist     = abs(NormZ - Progress)
ScanGlow = saturate( 1 - Dist / ScanlineWidth )      // 경계 근처에서 1
BodyEmis = HologramColor * 약발광(예 1.5)
Emissive = lerp( BodyEmis, ScanlineColor * EmissiveBoost, ScanGlow )
```
- `Emissive` → **Emissive Color**.
- Unlit이면 BaseColor 불필요(Emissive만). Lit이면 `BaseColor = HologramColor`.

### 4) (선택) 디테일
- **가로 주사선 줄무늬**: `frac(WorldPosition.Z * 0.05 - Time * 속도)` → 가는 줄무늬로 Emissive 살짝 변조.
- **Fresnel 림라이트**: `Fresnel` → HologramColor 가장자리 강조(홀로그램 윤곽).
- **플리커**: `Time` 기반 노이즈로 Emissive 미세 흔들기.

### 5) z-fighting (둘 중 하나)
- **머티리얼 WPO(권장)**: `World Position Offset = VertexNormalWS * 2.0`(uu) — 프록시를 실제보다 살짝 부풀려 경계 위가 실제를 확실히 가림. 이 경우 컴포넌트 `ProxyScaleMultiplier = 1.0`으로 둘 것.
- **코드 스케일(폴백)**: WPO 미사용 시 컴포넌트 `ProxyScaleMultiplier = 1.02`(기본)로 프록시를 약간 키워 가림.

## C++ 연동 (이미 구현됨)
- `UOJJ_HologramBuildUpComponent`(`OJJ_HologramBuildUpComponent.h/.cpp`): 프록시 생성 + MID + `Progress`/`MinZ`/`MaxZ` 주입 + Progress=1에서 프록시 제거.
- `AOJJ_BuildController::StartBuildUpEffect`: 신규 배치(PlaceMachine/PlaceFoundationAtCursor) 직후 호출. `BP_BuildController`의 `HologramBuildUpMaterial`/`HologramBuildUpDuration` 슬롯으로 머티리얼·지속 지정.
- 머티리얼 미지정 시 효과 skip(배치 정상) — **에디터에서 본 가이드대로 제작 후 슬롯 지정하면 활성화**.

## 적용 범위 (v1)
- 머신(단일 MeshComponent), 파운데이션(SlabMesh)만. 파운데이션 다리(LegISM)·컨베이어/파이프(스플라인)는 후속.
