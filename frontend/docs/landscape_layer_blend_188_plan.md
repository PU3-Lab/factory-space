# #188 랜드스케이프 다중 표면 — Snow + Ground Layer Blend 설계 가이드

> **방향(2026-06-15 수정안)**: 이슈 원안의 *Sand+Rock 신규 임포트*에서 변경 →
> **기존 머티리얼 2종(M_Snow, M_Ground)을 레이어화**. 텍스처 임포트 불필요, 작업 축소.
>
> - 레이어 1 = **Snow** (`Snow015_2K-JPG_*`, 현재 베이스 `MI_Snow`)
> - 레이어 2 = **Ground** (`Ground037_2K-JPG_*`, 이미 있는 `MI_Ground`)
> - **경사 자동 마스크 + 수동 페인트** 둘 다로 섞기
>
> ⚠️ **역할 분담 (AGENTS.md 준수)**: 머티리얼 그래프 편집·LayerInfo 생성·페인팅은 **에디터 손작업(사용자)**.
> 이 문서(설계도·수식·단계 가이드)가 Claude 몫. 임시 Unreal Python으로 에셋을 우회 생성하지 않음.

---

## 0. 현재 상태 (확인됨)

| 항목 | 상태 |
|---|---|
| Landscape 액터 | ✅ `L_Planet:PersistentLevel.Landscape_1` (실제 Landscape, Paint 가능) |
| 현재 지형 머티리얼 | 단일 표면 (LandscapeLayerBlend 노드 0건) — `MI_Snow` 추정 |
| LayerInfo 에셋 | ❌ 0개 → **신규 생성 필수** |
| Snow 텍스처 | `/Game/OJJ/Textures/Snow015_2K-JPG_` Color·NormalDX·Roughness·AmbientOcclusion |
| Ground 텍스처 | `/Game/OJJ/Textures/Ground037_2K-JPG_` Color·NormalDX·Roughness·AmbientOcclusion |
| 레이어별 TileScale 파라미터 | ❌ 현재 없음 (고정 타일링) → 설계에서 추가 |

> 참고: 이슈 원문의 "Ground049B"는 리포 실제 세트와 불일치. 실제는 **Ground037** 사용.

---

## 1. 머티리얼 그래프 설계도 — `M_Landscape_PlanetBlend` (신규 마스터)

**왜 신규 마스터?** `M_Snow`/`M_Ground`를 직접 수정하지 않고 새 블렌드 마스터를 만들면,
두 원본 머티리얼은 다른 메시에서 계속 재사용 가능하고 롤백도 쉽다. 텍스처 에셋은 그대로 공유.

### 1-1. 핵심 아이디어 — "단일 가중치 `w` 로 통합"

경사 자동과 수동 페인트를 **하나의 블렌드 가중치 `w`** 로 합친다. `w=0`이면 Snow, `w=1`이면 Ground.

```
w = saturate( SlopeAuto + GroundPaint − SnowPaint )

  · SlopeAuto  : 경사도 자동 마스크 (평지 0 → 경사 1)        [WorldNormal.Z 기반]
  · GroundPaint: "Ground" 레이어 수동 페인트 가중치 (0~1)     [LandscapeLayerSample]
  · SnowPaint  : "Snow"  레이어 수동 페인트 가중치 (0~1)      [LandscapeLayerSample]
```

이 한 줄이 4가지 케이스를 전부 만족한다:

| 상황 | SlopeAuto | GroundPaint | SnowPaint | w | 결과 |
|---|---|---|---|---|---|
| 평지, 페인트 없음 | 0 | 0 | 0 | 0 | **Snow** (눈) |
| 경사, 페인트 없음 | 1 | 0 | 0 | 1 | **Ground** (땅) — 자동 |
| 평지에 Ground 칠함 | 0 | 1 | 0 | 1 | **Ground** — 수동 추가 |
| 경사에 Snow 칠함 | 1 | 0 | 1 | 0 | **Snow** — 자동 무시, 수동 강제 |

→ **눈은 평지, 땅(바위)은 경사**가 기본. 페인트로 양방향 덮어쓰기 가능.

### 1-2. 노드 그래프 (ASCII)

```
[VertexNormalWS] ──► (Mask B / Z) ──► Z
                                       │
   Param: Slope_Threshold (0.85) ──►(−)│  SlopeAuto = saturate( (Threshold − Z) / Falloff )
   Param: Slope_Falloff   (0.10) ──►(÷)┘            평지 Z≈1 → 0,  경사 Z↓ → 1
                                       │
                                       ▼
[LandscapeLayerSample "Ground"] ─►(+)  │
[LandscapeLayerSample "Snow"  ] ─►(−)──┴─► saturate ─► w ───────────────┐
                                                                         │
  ┌─ SNOW 샘플러 그룹 ─────────────────────┐                            │
  │ [TexCoord]×[Param Snow_TileScale] ─► UV │                           │
  │   Snow015_Color     ─► SnowBaseColor    │                          (lerp A)
  │   Snow015_NormalDX  ─► SnowNormal        │                           │
  │   Snow015_Roughness ─► SnowRough         │                          │
  └─────────────────────────────────────────┘                          │
  ┌─ GROUND 샘플러 그룹 ───────────────────┐                           (lerp B)
  │ [TexCoord]×[Param Ground_TileScale]─►UV │                           │
  │   Ground037_Color    ─► GroundBaseColor │                           │
  │   Ground037_NormalDX ─► GroundNormal     │                          │
  │   Ground037_Roughness─► GroundRough      │                          │
  └─────────────────────────────────────────┘                          │
                                                                         ▼
   BaseColor = lerp(SnowBaseColor, GroundBaseColor, w) ──────────► [BaseColor]
   Normal    = lerp(SnowNormal,    GroundNormal,    w) ──────────► [Normal]
   Roughness = lerp(SnowRough,     GroundRough,     w) ──────────► [Roughness]
```

> **Lerp 3개**(BaseColor·Normal·Roughness)는 **같은 `w`** 를 공유한다. `w` 는 한 번만 계산해서 분기.

### 1-3. 노드 선택 주의점

- **VertexNormalWS 사용** (PixelNormalWS 아님). 정점 노멀은 노멀맵 디테일에 영향받지 않아 경사 마스크가 매끈하다. PixelNormalWS를 쓰면 노멀맵 요철 때문에 경계가 지글거린다.
- `LandscapeLayerSample` 노드(= MaterialExpressionLandscapeLayerSample)가 **페인트 레이어를 등록**한다. 이 노드에 적은 레이어 이름("Snow"/"Ground")이 곧 Paint 탭의 타겟 레이어 이름이 되며, **LayerInfo 이름과 정확히 일치**해야 한다(대소문자 포함).
- AO(AmbientOcclusion)는 선택: BaseColor에 곱하거나 무시. 1차에선 생략 권장(단순화).

---

## 2. 경사 자동 마스크 수식

```
Z = VertexNormalWS 의 Z 성분            // 평지 → Z ≈ 1.0,  수직 절벽 → Z ≈ 0.0
SlopeAuto = saturate( (Slope_Threshold − Z) / Slope_Falloff )
```

| 파라미터 | 권장 기본값 | 의미 |
|---|---|---|
| `Slope_Threshold` | **0.85** | 이 값보다 Z가 낮아지면(=더 가팔라지면) Ground 시작. 0.85 ≈ 약 31° 경사부터 |
| `Slope_Falloff` | **0.10** | 전환 부드러움. 작을수록 Snow→Ground 경계가 또렷, 클수록 그라데이션 |

- 임계각 ↔ Z 환산: `Z = cos(경사각)`. 예) 30°→0.866, 45°→0.707, 20°→0.940.
- **둘 다 ScalarParameter** 로 만들어 MI에서 실시간 튜닝. (땅을 더 일찍 드러내려면 Threshold↑)
- 방향 결정: **Ground = 경사, Snow = 평지** (사용자 확정. 눈은 가파른 면에 안 쌓임 → 자연스러움).

---

## 3. LayerInfo 2개 생성 가이드 (Weight-Blended)

> LayerInfo가 없으면 **Paint가 칠해지지 않는다**(이슈 ⚠️ 핵심).

1. 먼저 §5로 새 마스터/인스턴스를 **Landscape에 할당**해야 Paint 탭에 레이어가 뜬다.
2. 상단 모드 드롭다운 → **Landscape** → **Paint** 탭.
3. **Target Layers** 목록에 `Snow`, `Ground` 두 레이어가 보임(머티리얼의 LandscapeLayerSample 이름에서 자동 인식).
4. 각 레이어 우측 **+ 아이콘** 클릭 → **Weight-Blended Layer (normal)** 선택.
   - ❗ "Non Weight-Blended"(알파 블렌드) 아님 — 반드시 **Weight-Blended**.
5. 저장 위치/이름: `Content/OJJ/Landscape/` 폴더 새로 만들어 `LI_Snow`, `LI_Ground` 로 저장.
6. 두 레이어 모두 LayerInfo가 채워졌는지 확인(아이콘이 회색→정상).

---

## 4. 레이어별 TileScale 파라미터

현재 M_Snow/M_Ground는 고정 타일링이라 **레이어별 스케일 분리 불가** → 신규 마스터에서 파라미터화.

```
각 레이어 UV = TextureCoordinate[0]  ×  ScalarParameter
                                         ├─ Snow_TileScale   (기본 1.0)
                                         └─ Ground_TileScale (기본 1.0)
```

- 노드: `TextureCoordinate` → `Multiply`(B핀에 ScalarParameter) → 해당 레이어 모든 텍스처 샘플의 UV로.
- 값 ↑ → 타일 촘촘(텍스처 작아짐), 값 ↓ → 크게. 눈/땅 디테일 밀도를 따로 맞출 수 있음.
- 파라미터 그룹을 `00_Tiling` 등으로 묶어 MI에서 정리.

**최종 노출 파라미터(MI에서 조절)**

| 파라미터 | 타입 | 기본 | 그룹 |
|---|---|---|---|
| `Slope_Threshold` | Scalar | 0.85 | 10_Slope |
| `Slope_Falloff` | Scalar | 0.10 | 10_Slope |
| `Snow_TileScale` | Scalar | 1.0 | 00_Tiling |
| `Ground_TileScale` | Scalar | 1.0 | 00_Tiling |

---

## 5. 단계별 에디터 셋업

1. **마스터 생성**: `Content/OJJ/Materials` 에 Material 신규 → `M_Landscape_PlanetBlend`.
   - Material 디테일에서 사용 도메인 Surface 유지(기본).
2. **그래프 구성**: §1-2 대로 노드 배치. Snow/Ground 텍스처는 콘텐츠 브라우저에서 드래그.
   - 텍스처 샘플의 Sampler Type: Color=Color, NormalDX=Normal, Roughness=Linear Grayscale 자동.
3. **컴파일/저장**: Apply → Save. 에러 없는지 확인(LandscapeLayerSample 있으면 "landscape" 사용 플래그 자동).
4. **MI 생성**: `M_Landscape_PlanetBlend` 우클릭 → Create Material Instance → `MI_Landscape_PlanetBlend`.
5. **Landscape에 할당**: 레벨에서 `Landscape_1` 선택 → 디테일 패널 **Landscape Material** 슬롯에 `MI_Landscape_PlanetBlend` 지정.
   - (현재 슬롯이 `MI_Snow`인지 여기서 확인 가능. 교체하면 됨.)
6. **LayerInfo 생성**: §3 진행 (Snow/Ground, Weight-Blended).
7. **컴파일 대기**: 셰이더 컴파일 완료까지 지형이 검게 보일 수 있음 — 정상.

---

## 6. 페인팅 가이드

1. **Landscape → Paint** 탭, Target Layer 선택(Snow 또는 Ground).
2. 브러시 Size / Falloff / Strength 조절 후 좌클릭 드래그로 칠하기.
3. **기본 동작 확인**: 아무것도 안 칠해도 **경사면은 자동으로 Ground**, 평지는 Snow로 보여야 한다(§1-1).
   - 안 그러면 → 마스터에 SlopeAuto 연결 누락 또는 VertexNormalWS Z 마스크 확인.
4. **수동 보정**:
   - 평지 일부를 흙길로 → `Ground` 선택해 칠함.
   - 가파른 면에 눈을 강제로 → `Snow` 선택해 칠함(자동 Ground를 덮음).
5. **주의(이슈 ⚠️)**: 페인트 가중치는 `.umap`에 저장되어 **용량 증가**. 넓게 칠할수록 커밋 용량↑.
   - 자동 경사 마스크를 최대한 활용하고 **수동 페인트는 보정용으로 최소화** 권장.
   - **Git LFS 도입 논의 임박**(팀 메모) — `.umap`/텍스처가 LFS 대상이 되면 워크플로 변경 예정.

---

## 7. 검증 체크리스트

- [ ] 마스터 컴파일 에러 0, MI 생성됨
- [ ] Landscape Material 슬롯 = `MI_Landscape_PlanetBlend`
- [ ] Paint 탭에 `Snow`/`Ground` 레이어 표시 + 둘 다 LayerInfo(Weight-Blended) 채워짐
- [ ] 페인트 0 상태에서 경사=Ground / 평지=Snow 자동 전환 육안 확인
- [ ] `Ground` 페인트로 평지에 땅 칠해짐 / `Snow` 페인트로 경사에 눈 칠해짐
- [ ] `Slope_Threshold`·`TileScale` MI에서 실시간 반영
- [ ] PIE 진입 후에도 정상(셰이더 영구 컴파일)

---

## 8. #184 충돌 회피

이 작업은 **랜드스케이프 머티리얼/에셋 영역 한정**. `OJJ_Player`/`OJJ_Ladder`/`OJJ_RampFoundation`
미접촉 → 데스크탑 로컬 #184와 파일 충돌 없음. (push 권한 미복구 상태 → 로컬 커밋만 누적)

신규/수정 예상 에셋:
- `Content/OJJ/Materials/M_Landscape_PlanetBlend.uasset` (신규)
- `Content/OJJ/Materials/MI_Landscape_PlanetBlend.uasset` (신규)
- `Content/OJJ/Landscape/LI_Snow.uasset`, `LI_Ground.uasset` (신규)
- `Content/OJJ/Levels/L_Planet.umap` (Landscape Material 슬롯 변경: **MI_Snow → MI_Landscape_PlanetBlend** 확정 + 페인트 가중치)

---

## 9. 핀 단위 연결표 (M_Landscape_PlanetBlend)

> **현재 Landscape Material 슬롯 = `MI_Snow` 확정** → §5-5에서 이걸 `MI_Landscape_PlanetBlend`로 교체.
> 아래 노드 ID(N1~N26)는 임의 식별용. 에디터에서 우클릭 검색으로 노드 추가 후 표대로 연결.

### 9-1. 노드 목록

| ID | 노드 타입 | 핵심 설정 |
|---|---|---|
| **N1** | TextureCoordinate | CoordinateIndex = 0 |
| **N2** | ScalarParameter | 이름 `Snow_TileScale`, 기본 **1.0**, Group `00_Tiling` |
| **N3** | Multiply | A=N1, B=N2 → Snow UV |
| **N4** | ScalarParameter | 이름 `Ground_TileScale`, 기본 **1.0**, Group `00_Tiling` |
| **N5** | Multiply | A=N1, B=N4 → Ground UV |
| **N6** | TextureSample | `Snow015_2K-JPG_Color`, SamplerType **Color**, UVs=N3 |
| **N7** | TextureSample | `Snow015_2K-JPG_NormalDX`, SamplerType **Normal**, UVs=N3 |
| **N8** | TextureSample | `Snow015_2K-JPG_Roughness`, SamplerType **Linear Grayscale**, UVs=N3 |
| **N9** | TextureSample | `Ground037_2K-JPG_Color`, SamplerType **Color**, UVs=N5 |
| **N10** | TextureSample | `Ground037_2K-JPG_NormalDX`, SamplerType **Normal**, UVs=N5 |
| **N11** | TextureSample | `Ground037_2K-JPG_Roughness`, SamplerType **Linear Grayscale**, UVs=N5 |
| **N12** | VertexNormalWS | (입력 없음) |
| **N13** | ComponentMask | **B 만 체크** (R/G/A 해제) → Z 성분 |
| **N14** | ScalarParameter | 이름 `Slope_Threshold`, 기본 **0.85**, Group `10_Slope` |
| **N15** | Subtract | A=N14, B=N13 → (Threshold − Z) |
| **N16** | ScalarParameter | 이름 `Slope_Falloff`, 기본 **0.10**, Group `10_Slope` |
| **N17** | Divide | A=N15, B=N16 |
| **N18** | Saturate | 입력=N17 → **SlopeAuto** (Saturate 없으면 Clamp Min0/Max1) |
| **N19** | LandscapeLayerSample | ParameterName **`Ground`**, PreviewWeight 0 → GroundPaint |
| **N20** | LandscapeLayerSample | ParameterName **`Snow`**, PreviewWeight 0 → SnowPaint |
| **N21** | Add | A=N18, B=N19 → SlopeAuto+GroundPaint |
| **N22** | Subtract | A=N21, B=N20 → −SnowPaint |
| **N23** | Saturate | 입력=N22 → **w** (블렌드 가중치) |
| **N24** | LinearInterpolate (Lerp) | A=N6, B=N9, Alpha=N23 → BaseColor |
| **N25** | LinearInterpolate (Lerp) | A=N7, B=N10, Alpha=N23 → Normal |
| **N26** | LinearInterpolate (Lerp) | A=N8, B=N11, Alpha=N23 → Roughness |

### 9-2. 연결표 (출력 핀 → 입력 핀)

| # | From (노드.출력핀) | → | To (노드.입력핀) |
|---|---|---|---|
| 1 | N1 TextureCoordinate.출력 | → | N3 Multiply.**A** |
| 2 | N2 Snow_TileScale.출력 | → | N3 Multiply.**B** |
| 3 | N1 TextureCoordinate.출력 | → | N5 Multiply.**A** |
| 4 | N4 Ground_TileScale.출력 | → | N5 Multiply.**B** |
| 5 | N3 Multiply.출력 | → | N6.**UVs**, N7.**UVs**, N8.**UVs** (Snow 3개) |
| 6 | N5 Multiply.출력 | → | N9.**UVs**, N10.**UVs**, N11.**UVs** (Ground 3개) |
| 7 | N12 VertexNormalWS.출력 | → | N13 ComponentMask.입력 |
| 8 | N13 ComponentMask(B).출력 | → | N15 Subtract.**B** |
| 9 | N14 Slope_Threshold.출력 | → | N15 Subtract.**A** |
| 10 | N15 Subtract.출력 | → | N17 Divide.**A** |
| 11 | N16 Slope_Falloff.출력 | → | N17 Divide.**B** |
| 12 | N17 Divide.출력 | → | N18 Saturate.입력 |
| 13 | N18 Saturate.출력 (SlopeAuto) | → | N21 Add.**A** |
| 14 | N19 LayerSample"Ground".출력 | → | N21 Add.**B** |
| 15 | N21 Add.출력 | → | N22 Subtract.**A** |
| 16 | N20 LayerSample"Snow".출력 | → | N22 Subtract.**B** |
| 17 | N22 Subtract.출력 | → | N23 Saturate.입력 |
| 18 | N23 Saturate.출력 (**w**) | → | N24.**Alpha**, N25.**Alpha**, N26.**Alpha** (Lerp 3개 공유) |
| 19 | N6 SnowColor.**RGB** | → | N24 Lerp.**A** |
| 20 | N9 GroundColor.**RGB** | → | N24 Lerp.**B** |
| 21 | N7 SnowNormal.**RGB** | → | N25 Lerp.**A** |
| 22 | N10 GroundNormal.**RGB** | → | N25 Lerp.**B** |
| 23 | N8 SnowRough.**R** | → | N26 Lerp.**A** |
| 24 | N11 GroundRough.**R** | → | N26 Lerp.**B** |
| 25 | N24 Lerp.출력 | → | **Material.Base Color** |
| 26 | N25 Lerp.출력 | → | **Material.Normal** |
| 27 | N26 Lerp.출력 | → | **Material.Roughness** |

### 9-3. 노출 파라미터 (MI에서 조절)

| 파라미터 | 타입 | 기본 | Group | 비고 |
|---|---|---|---|---|
| `Snow_TileScale` | Scalar | 1.0 | 00_Tiling | ↑ 촘촘 / ↓ 크게 |
| `Ground_TileScale` | Scalar | 1.0 | 00_Tiling | 〃 |
| `Slope_Threshold` | Scalar | 0.85 | 10_Slope | ≈31°부터 Ground. ↑ 땅 더 일찍 |
| `Slope_Falloff` | Scalar | 0.10 | 10_Slope | ↑ 경계 부드럽게 |

### 9-4. 연결 팁 / 흔한 실수

- **N13 ComponentMask는 B(=Z)만** 체크. R/G/A 켜져 있으면 벡터가 나와 Subtract에서 에러.
- N8/N11 Roughness는 **R 핀**을 Lerp에 (RGB 통째 X). Sampler Type을 Linear Grayscale로 두면 R에 값.
- N6~N11 **UVs 핀을 빠뜨리면** 기본 UV(타일링 1)로 나옴 → TileScale 무효. 5·6번 연결 꼭 확인.
- `LandscapeLayerSample`의 ParameterName 철자(`Ground`/`Snow`)가 §3 LayerInfo 이름과 **정확히 일치**해야 Paint 인식.
- Saturate 노드가 안 보이면: 우클릭 검색 "Saturate" 없을 시 **Clamp**(Min Default 0, Max Default 1)로 대체.
- 컴파일 후 한 번 **Apply→Save**, 셰이더 컴파일 끝까지 대기.

---

## 10. Falloff 튜닝 가이드 (#188 후속) — 완만 지형 대응 + 흑백 검증

> 배경: 지형이 완만해 경사 자동 마스크만으론 Ground가 잘 안 드러남. `Slope_Threshold`/`Slope_Falloff`를
> 올려/조정해 더 완만한 면도 Ground로 전환. **MI_Land 파라미터라 라이브 조정(컴파일 불필요).**
> ⚠️ 페인트는 Git LFS 도입 후로 보류(.umap 용량) — 이번엔 **파라미터 튜닝만**(MI_Land 작은 파일).

### 10-1. 수식 복습 (§2)
```
SlopeAuto = saturate( (Slope_Threshold − VertexNormalWS.Z) / Slope_Falloff )
   Z = VertexNormalWS.Z :  평지 Z≈1.0  →  수직 Z≈0.0      (Z = cos(경사각))
```
- **Slope_Threshold** = 이 Z값보다 낮아지면(=더 가팔라지면) Ground 시작. 값 ↑ → 더 완만한 면도 Ground.
- **Slope_Falloff** = 전환 밴드 폭. 값 ↓ 또렷(칼선) / 값 ↑ 부드러운 그라데이션.

### 10-2. Slope_Threshold 값별 효과 (= 임계 경사각)
| Threshold | ≈ 경사각(acos) | 효과 |
|---|---|---|
| 0.85 (현재) | ~31.8° | 32°보다 가파른 면만 Ground (완만 지형엔 거의 안 나옴) |
| 0.90 | ~25.8° | 26°부터 |
| **0.92** | ~23° | 23°부터 — 완만한 둔덕도 땅 (권장 시작점) |
| **0.95** | ~18° | 18°부터 — 꽤 완만해도 땅 |
| 0.97 | ~14° | 거의 평지 빼고 전부 땅 |
> 완만 지형이면 **0.92~0.95**에서 시작해 Ground 면적 보며 조절.

### 10-3. Slope_Falloff 값별 효과 (전환 폭)
| Falloff | 효과 |
|---|---|
| 0.03 | 칼 같은 경계(Snow↔Ground 딱 끊김) |
| 0.05 | 또렷 |
| 0.10 (현재) | 중간 |
| 0.20 | 부드러운 그라데이션 |
| 0.30+ | 매우 넓은 혼합(뿌옇게 섞임) |
> 자연스러운 전환은 0.05~0.12. Threshold를 먼저 맞추고 Falloff로 경계 느낌만 다듬기.

### 10-4. ⭐ 흑백 검증 절차 (어제 N18 방식)
경사 분포를 흑백으로 눈으로 보며 값 확정. **마스크 출력을 BaseColor에 임시 직결.**

1. `M_Landscape_PlanetBlend`(마스터) 열기.
2. **N18(Saturate = SlopeAuto) 출력 → Material 의 Base Color** 에 임시 연결.
   - (페인트 0 상태면 최종 가중치 `w`(N23)도 SlopeAuto와 사실상 동일 — N18로 충분)
3. **Apply → Save**, 셰이더 컴파일 대기. 지형이 흑백으로:
   - **흰색 = Ground(경사로 판정)** / **검정 = Snow(평지)** / 회색 = 전환 밴드
4. **MI_Land 디테일 패널**에서 `Slope_Threshold`/`Slope_Falloff` 슬라이더 라이브 조정 → 흰 면적이 실시간 변함.
   - 원하는 Ground 분포(능선·비탈이 적당히 흰색) 나올 때까지 Threshold↑/Falloff 조정.
   - 회색 밴드 폭이 곧 전환 부드러움 = Falloff 체감.
5. 값 확정(예: Threshold 0.93, Falloff 0.08) → **기록**해 둠.

### 10-5. 원복 절차 (검증 후 필수)
1. 마스터에서 **N18→BaseColor 임시 연결 제거**.
2. **N24(Lerp, BaseColor용) 출력 → Material Base Color** 재연결(§1-2 원래 상태).
3. Normal(N25)·Roughness(N26) Lerp 연결은 안 건드렸으면 그대로.
4. **Apply → Save**, 컴파일 대기 → 컬러 블렌드 정상 복귀.
> ⚠️ **흑백 디버그 와이어 상태로 커밋 금지** — 반드시 N24 원복 후 저장.

### 10-6. 확정값 커밋
- MI_Land 오버라이드로 둘지(인스턴스값) / 마스터 ScalarParameter 기본값(N14·N16)에 박을지 선택.
  - **마스터 기본값에 박기**(권장): 새 인스턴스에도 적용. M_Landscape_PlanetBlend의 `Slope_Threshold`/`Slope_Falloff` Default Value 수정 → Save.
- 변경 파일: `M_Landscape_PlanetBlend.uasset`(+ MI_Land.uasset 오버라이드 시) — **작은 파일, 용량 영향 미미**.
- 페인트 가중치(.umap)는 **이번에 안 건드림** → LFS 보류 무영향.

---

## 11. 눈/땅 반전 — 평지=Ground, 경사=Snow (산봉우리 눈)

> 목표: 현재 *평지=Snow / 경사=Ground* → **평지=Ground / 경사·봉우리=Snow** 로 반전.

### 11-A. ⭐ 권장 — Subtract 노드 한 곳만 스왑 (1 edit)
경사 마스크의 부호만 뒤집으면 끝. **Lerp·페인트 항·LayerSample 이름 전부 그대로** → 의미 안 꼬임.

- **N15 Subtract 입력 A↔B 스왑**: `A=N14(Threshold), B=N13(Z)` → **`A=N13(Z), B=N14(Threshold)`**
  - 기존: `(Threshold − Z)` = 경사에서 높음 → 경사=Ground
  - 변경: `(Z − Threshold)` = **평지에서 높음** → 평지=Ground(Lerp B), 경사=Snow(Lerp A)
- 나머지 **무수정**: Lerp(A=Snow/B=Ground) 그대로, `w = saturate(mask + GroundPaint − SnowPaint)` 그대로.
- **페인트 의미 자동 정합**: GroundPaint(+)는 여전히 Ground 추가(경사에 땅 강제), SnowPaint(−)는 Snow 추가(평지에 눈 강제). 직관 유지.

> 결과적으로 N18 마스크는 이제 "평지 Ground 가중치"(평지 흰색). 노드 1개만 만지므로 실수 위험 최저.

### 11-B. 대안 — Lerp 스왑 + 페인트 항 스왑 (5 edits, 동등하지만 번거로움)
- N24/N25/N26 **Lerp 3개 A↔B 스왑**: `A=Ground, B=Snow`
- **w 페인트 항 스왑**: `w = saturate(SlopeAuto + SnowPaint − GroundPaint)` (N21/N22의 GroundPaint/SnowPaint 입력 교체)
- 결과 동일하나 5곳을 일관되게 맞춰야 함(하나라도 누락 시 색/페인트 꼬임). **11-A 권장.**

### 11-C. 흑백 검증 해석 (반전 후)
- 11-A 적용 시 N18→BaseColor 직결하면: **흰 = Ground(평지) / 검정 = Snow(경사·봉우리)**.
- (11-B 적용 시엔 반대: 흰=Snow(경사)/검정=Ground(평지) — w를 보면 Snow쪽.)

### 11-D. ⚠️ Falloff 튜닝 방향 반전 (§10 보정)
11-A에서 마스크 = `saturate((Z − Threshold)/Falloff)` 라 **Threshold 의미가 뒤집힘**:
| Slope_Threshold | 효과 (11-A 기준) |
|---|---|
| ↑ (예 0.95, cos18°) | 18°보다 가파르면 Snow → **눈 많음**(평지 가까운 면만 땅) |
| ↓ (예 0.82, cos35°) | 35°보다 가파라야 Snow → **눈 적음**(가파른 봉우리만 눈) |
> "봉우리에만 눈"은 **낮은 Threshold(0.80~0.85)**, "눈 많은 설원"은 높은 Threshold. Falloff는 §10 동일(전환 폭).

### 11-E. 원복·커밋
- 검증 후 N18→BaseColor 임시 와이어 제거 → N24(Lerp)→BaseColor 재연결(§10-5).
- 11-A는 N15 스왑이 **영구 변경**(원복 대상 아님) — 검증 와이어만 원복.
- 커밋: `M_Landscape_PlanetBlend.uasset`만(작은 파일). 페인트 .umap 무관 → LFS 보류 무영향.
