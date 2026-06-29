# 로봇 대화 포트레이트 — LDJ 인수인계

**작성:** 2026-06-29 (OJJ) · **대상:** LDJ (대화 벌룬 위젯 담당)

대화 벌룬의 포트레이트 칸에 **idle 도는 로봇**을 실시간으로 띄우는 작업. 캡처/머티리얼/자동 스폰(OJJ 영역)은 **준비 완료**. LDJ는 **위젯에 포트레이트를 표시**하는 부분만 담당하면 된다(텍스처박스 크기/레이아웃은 LDJ 영역).

---

## OJJ가 준비한 것 (건드릴 필요 없음)

| 에셋/클래스 | 경로 | 역할 |
|---|---|---|
| `AOJJ_PortraitCapture` | `Source/Wanted_Factory/.../OJJ_PortraitCapture` | 로봇 메시 + idle 애니를 SceneCapture로 RT에 실시간 캡처 |
| `RT_RobotPortrait` | `/Game/OJJ/Character/Robot/RT_RobotPortrait` | 캡처 출력 RenderTarget (512×512) |
| `M_Portrait_UI` | (포트레이트 UI 머티리얼) | RT를 알파 투명 처리해 UI에 표시 (배경 투명, 로봇 불투명) |
| `UOJJ_PortraitCaptureSubsystem` | (월드 서브시스템) | 화이트리스트 레벨에서 캡처 액터 **자동 스폰** |
| `M_Robot` | `/Game/OJJ/Character/Robot/M_Robot` | 로봇 수동 베이스 머티리얼 (자동 임포트 깨짐 수정 완료) |

---

## LDJ가 할 것 — 위젯에 포트레이트 표시

1. **포트레이트 칸 `Image`의 Brush에 `M_Portrait_UI` 할당**
   - `Brush ▸ Image` = `M_Portrait_UI`
   - **Draw As = `Image`**
   - **Tint = 흰색 `(1, 1, 1, 1)`** — Tint는 곱연산이라 흰색이어야 색/알파가 안 변한다. 회색·반투명 주의.
   - **정사각 1:1 유지** — RT가 512×512 정사각이라 가로세로 비율 다르면 로봇이 찌그러진다.

2. **RT 연결은 신경 쓸 것 없음** — Subsystem이 자동 스폰하며 캡처가 `RT_RobotPortrait`에 상시 갱신된다. `M_Portrait_UI`가 그 RT를 참조하므로 위젯은 머티리얼만 붙이면 끝.

3. **레벨 화이트리스트 확인** — 자동 스폰은 `L_Planet`, `L_GridTest`에서만 동작.
   - 다른 레벨에서도 필요하면: **Project Settings ▸ Game ▸ "OJJ Portrait Capture" ▸ AutoSpawnLevels**에 맵명 추가(코드 수정 불필요).

---

## 동작/주의

- **캡처 워밍업 0.3초**: 레벨 로드 직후 ~0.3초는 캡처가 꺼져 있어(셰이더 첫 컴파일/텍스처 로드 중 깨진 프레임 회피) RT가 비어(투명) 보일 수 있다. 0.3초 뒤 로봇이 나타난다 — 레벨 진입 순간 잠깐이라 대화창엔 영향 거의 없음.
- **상시 캡처**: 대화창이 거의 항상 떠있는 전제로 `bCaptureEveryFrame=true` 상시 캡처. open/close 토글은 두지 않음. 캡처 비용이 문제가 되면 OJJ에 알려주면 간헐 캡처/RT 해상도 축소를 검토한다.
- **단일 인스턴스**: 하나의 RT를 공유하므로 캡처 액터는 레벨당 1개(자동 스폰이 보장). 수동 배치 추가 금지.

---

## 트러블슈팅

| 증상 | 확인 |
|---|---|
| 포트레이트 칸이 빈/투명 | ① 현재 레벨이 화이트리스트(L_Planet/L_GridTest)인가 ② `RT_RobotPortrait` 더블클릭 시 로봇이 보이는가 |
| RT엔 로봇 보이는데 위젯엔 안 뜸 | 위젯 Image Brush에 `M_Portrait_UI` 할당·Draw As=Image·Tint 흰색인지 |
| RT에도 로봇 안 보임 | 캡처/스폰 문제 → OJJ에 전달 (Output Log `[PortraitSubsystem]` / `[OJJ_PortraitCapture]` 경고 확인) |
| 로봇이 찌그러짐 | Image 칸 비율이 1:1인지 |
| 첫 진입 순간만 깨져 보임 | 에디터 첫 PIE의 셰이더 컴파일(패키징은 무관). 워밍업 0.3초로 대부분 회피됨 |
