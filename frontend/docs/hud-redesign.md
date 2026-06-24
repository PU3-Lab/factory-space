# HUD 재제작 설계 + 계약 체크리스트

> **방식: WBP_MainHUD 인플레이스 레이아웃 재제작 / C++ `UI_MainHUD` 무변경 / 스탯 범위 제외**
> LDJ(이동진) 합의 완료. 작업 = UMG 에디터(`WBP_MainHUD.uasset`). 커밋: "LDJ 합의" 명시 + 리뷰어 `L-DongJin`. ⚠️ git 전 에디터 닫기.

---

## ⚠️⚠️ 절대 유지 (어기면 깨짐) ⚠️⚠️

이름·타입 그대로 유지하면 C++(`UI_MainHUD`)·OJJ_Player 연결이 자동 보존된다. **이름만 지키면 레이아웃/스타일/위치는 전부 자유.**

| 위젯 이름 (변경 금지) | 타입 | 용도 | 깨지는 이유 |
|---|---|---|---|
| **`WBP_QuestWindow`** | `UUI_QuestWindow` | 미션 패널 | BindWidget + 빌드모드 `GetWidgetFromName("WBP_QuestWindow")` |
| **`TXT_InGameTime`** | `TextBlock` | 시간 (실시간) | Tick 폴링 |
| **`TXT_DisasterDay`** | `TextBlock` | SOL/DAY (실시간) | Tick 폴링 |
| **`TXT_WindSpeed`** | `TextBlock` | 날씨 풍속 | 날씨 델리게이트 |
| **`TXT_Rainfall`** | `TextBlock` | 날씨 강수 | 날씨 델리게이트 |
| **`B_PlanetEvent`** | `Border` | 행성이벤트 배경 | 이벤트 델리게이트 |
| **`TXT_PlanetEvent`** | `TextBlock` | 행성이벤트 텍스트 | 이벤트 델리게이트 |
| **`WBP_DialogueBalloon`** (임베드 자식) | `UI_DialogueBalloon` | Sky 호출 응답/AI 대사창 | **삭제 금지** — `GetAllWidgetsOfClass`로 찾음. 삭제 시 Sky AI 입력/응답/튜토리얼 깨짐. **위치 이동만 가능.** 내부 **`ET_OperatorInput` 이름 유지** |

## ✅ 삭제 가능
- **스탯 O2/HP/SUIT/TEMP** — `UI_MainHUD` BindWidget에 없음 + C++ 백엔드 전무 → **순수 BP**. WBP에서 자유 삭제·재배치, C++ 안 깨짐. (실데이터 연동은 백로그.)

## 🆕 신규
- **`NS_SkyAvatar`** — **NamedSlot**, 좌하단. Sky **Live2D 자리**(플레이스홀더). 현재 C++ 계약 없음(자유). 추후 Live2D 콘텐츠 주입 슬롯.

---

## 📐 배치 (레이아웃 맵)

```
┌──────────────────────────────────────────────────────────┐
│ [상단좌] SOL/시간 캡슐          [상단우] 날씨바             │
│  TXT_DisasterDay / TXT_InGameTime   TXT_WindSpeed/Rainfall │
│                                     B_PlanetEvent/TXT_      │
│                                                            │
│                                  [우상] 미션               │
│                                   WBP_QuestWindow          │
│                                                            │
│                                                            │
│ [좌하] Sky + 대사                  [중하] 자동저장   [우하] │
│  NS_SkyAvatar(Live2D)               (자동저장 표시)  (비움) │
│  + WBP_DialogueBalloon                                     │
└──────────────────────────────────────────────────────────┘
```

| 영역 | 내용 | 구성 위젯 |
|---|---|---|
| 상단좌 | SOL/시간 캡슐 | `TXT_DisasterDay`, `TXT_InGameTime` |
| 상단우 | 날씨바 | `TXT_WindSpeed`, `TXT_Rainfall`, `B_PlanetEvent`, `TXT_PlanetEvent` |
| 우상 | 미션 | `WBP_QuestWindow` |
| 좌하 | Sky + 대사 | `NS_SkyAvatar`(신규) + `WBP_DialogueBalloon`(이동) |
| 중하 | 자동저장 | (자동저장 표시) |
| 우하 | 비움 | — |

> ⚠️ 미션(우상)과 Sky/대사(좌하)를 분리 배치 → 기존 "우상단 미션 vs 우측 파란 패널" 자리 겹침 해소.
> `WBP_DialogueBalloon`은 삭제 없이 **좌하로 이동만**.

---

## 데이터 바인딩 소스 (재제작 후 자동 재연결 — 이름 유지 시)

| 요소 | 소스 시스템 | 방식 |
|---|---|---|
| 시간 / SOL | `PlanetEventManagerSubsystem` | NativeTick 폴링 |
| 날씨 / 행성이벤트 | `PlanetEventManagerSubsystem` | 델리게이트 (`OnWeatherChanged`/`OnPlanetEventStarted`/`OnPlanetEventEnded`) |
| 미션 | `QuestManagerSubsystem` | 델리게이트 (`OnMainQuestChanged`/`OnSubQuests*`/`OnTutorial*`) |
| Sky 대사 | `QuestManagerSubsystem` + `FactoryAgentClientSubsystem` | 델리게이트 (튜토리얼 + AI 웹소켓) |
| 스탯 O2/HP/SUIT/TEMP | ❌ 백엔드 없음 | 범위 제외 |

## 생성/제어 주체 (C++ 무변경 — 참고)
- 생성: `AOJJ_Player::BeginPlay()` (`OJJ_Player.cpp:160-166`) — `MainHUDWidgetClass`로 CreateWidget+AddToViewport
- 빌드모드 토글: `:1148`(Collapsed)/`:1197`(Visible)
- 외부 계약 호출: `Cast<UUI_MainHUD>->OnRequestQuestsClicked()`(`:1667`), `->ToggleQuestWindow()`(`:1689`)
- DialogueBalloon: `GetAllWidgetsOfClass(UUI_DialogueBalloon)` + `ET_OperatorInput`(`:1702~1750`)

→ 위 "절대 유지" 계약만 지키면 OJJ_Player 측 변경 불필요.

---

## 영역 / 커밋 규칙
- 소유: HUD 위젯 = **LDJ(이동진)** 영역 (`UI_BuildModeMain`만 SSR). **LDJ 합의 완료.**
- C++ 변경 0 → **`WBP_MainHUD.uasset` 단일 커밋** (+ DialogueBalloon 이동/NamedSlot 추가가 별 WBP 건드리면 해당 .uasset 포함)
- 커밋/PR 본문: **"LDJ 합의"** 명시 + 리뷰어 **`L-DongJin`**
- ⚠️ git 작업 전 Unreal 에디터 닫기 (.uasset 잠금)

## 백로그 (이번 범위 밖)
- 스탯(O2/HP/SUIT/TEMP) **C++ 백엔드 시스템** — 현재 전무, 신규 설계 필요
- **Sky Live2D** — `NS_SkyAvatar` 슬롯에 들어갈 Live2D 아바타 구현
- **#353 돔 내부 자기폭풍 VFX 국소차폐** — VFX가 전역(DayNightController: 화면 PostProcess + 월드 파티클 + 하늘)이라 위치 차폐는 별도 설계 필요
