<!-- This file should be saved in UTF-8 encoding. -->

# Data Change Log

## 2026-06-11

### `Source/Wanted_Factory/Data/MachineTable.csv`
- `Synthesizer` 머신 데이터 3종(`Synthesizer_Lv1`, `Synthesizer_Lv2`, `Synthesizer_Lv3`)을 추가했다.
- 공통 스펙은 크기 `3 x 3`, 입력 포트 `2`, 출력 포트 `2`, 입력 버퍼 `1`, 출력 버퍼 `1`, 전력 소모 `10`, 내구도 `1000`으로 설정했다.

### `Content/DataTable/MachineTable.csv`
- 콘텐츠용 데이터테이블에 `Synthesizer` 1건을 추가했다.
- 스펙은 `3 x 3`, 입력 포트 `2`, 출력 포트 `2`, 입력 버퍼 `1`, 출력 버퍼 `1`로 설정했다.

## 2026-06-12

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 초반 튜토리얼과 메인 목표 흐름을 `objective`, `reward` 컬럼 구조의 CSV로 추가했다.
- 조작 학습, 빌드 모드, 채굴, 전력 연결, 제련, 자동화, 통신탑, 신호 증폭기까지를 실제 체크 가능한 단계로 세분화했다.

### `Source/Wanted_Factory/Data/MachineTable.csv`
- 발전소 파생 머신 5종(`ThermalPowerPlant`, `HydroPowerPlant`, `NuclearPowerPlant`, `WindPowerPlant`, `SolarPowerPlant`) 행을 추가했다.
- 공통 스펙은 기존 `PowerPlant`와 동일하게 비용 `iron_ingot 10`, 내구도 `1000`, 전력 값 `30`, 크기 `4 x 4`로 맞췄다.

## 2026-06-14

### `Source/Wanted_Factory/Data/ResouceTable.csv`
- 파이프 액체 색상 관리를 위해 `PipeColor` 컬럼을 추가했다.
- 비액체 row는 `PipeColor`를 비워 두고, 액체 5종만 색상 값을 기록했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 퀘스트 데이터를 `quest_id,next_quest_id,group,title,description,reward` 구조로 정리했다.
- 스카이 대사는 별도 대사 테이블로 분리해 퀘스트 본체 데이터와 역할을 분리했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- `quest_id`, `trigger_type`, `line_order`, `speaker`, `dialogue` 컬럼 구조의 대사 전용 CSV를 추가했다.
- 각 퀘스트에 `on_start`, `on_complete` 기준의 다중 대사를 연결할 수 있도록 정리했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 탈출 포드 관련 흐름에서 `대상 선택` 단계를 제거하고 `철거 모드 진입` 다음에 바로 `탈출 포드 철거`가 이어지도록 단순화했다.
- `주변 철 광맥 확인` 단계는 광맥 발견 자체를 확인하는 용도로 유지하되, 설명을 더 명확하게 다듬었다.
- 발전소, 제련기, 통신탑, 신호 증폭기의 `설치 위치 지정/위치 도달/배치 위치 찾기` 성격의 단계를 제거해 실제 플레이 흐름에 맞게 정리했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 탈출 포드 선택 단계 삭제에 맞춰 빌드 구간 대사를 재연결했다.
- 광맥 확인 단계의 대사를 `광맥에 접근하거나 조준해 확인하는 단계`로 구체화했다.
- 삭제된 배치 위치/설치 위치 관련 퀘스트에 대응하던 대사를 제거하고, 남은 퀘스트에 맞게 순서를 다시 정리했다.

## 2026-06-15

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- 튜토리얼 퀘스트 완료 조건 매핑이 현재 `tutorial_quest_steps.csv` 순서와 어긋나 있던 문제를 수정했다.
- `TUT_BUILD`, `TUT_MINING`, `TUT_POWER`, `TUT_SMELT` 구간의 완료 조건을 현재 퀘스트 ID 구조에 맞게 다시 연결했다.
- 이 수정으로 발전소 설치 퀘스트가 완료되지 않던 문제와 함께, 같은 원인으로 한 단계씩 밀려 있던 송전탑/송전선/제련기 관련 판정도 현재 단계 흐름에 맞게 정리했다.

## 2026-06-15

### `Source/Wanted_Factory/Private/OJJ_Player.cpp`
- 빌드 카메라 회전 입력에서 `Q`, `E` 축 방향과 튜토리얼 이벤트(`RotateViewLeft`, `RotateViewRight`) 매핑이 반대로 전달되던 문제를 수정했다.
- 실제 입력 축 기준에 맞춰 화면 회전 튜토리얼 완료 이벤트가 올바른 퀘스트를 완료하도록 정정했다.

## 2026-06-15

### `Source/Wanted_Factory/UI/UI_BuildModeMain.cpp`
- 빌드 UI 버튼 클릭 시에는 배치 모드만 전환되고 튜토리얼 선택 이벤트가 전달되지 않던 문제를 수정했다.
- 창고, 컨베이어, 제련기, 채굴기, 발전소, 송전탑, 송전선 버튼 클릭 시 키보드 단축키와 동일한 `NotifyTutorialEvent`가 발생하도록 연결했다.
- 이 수정으로 기계 선택 퀘스트가 키보드뿐 아니라 UI 클릭으로도 완료되도록 정리했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 채굴기 설치 이후 물류 튜토리얼 순서를 `입력용 창고 포트 설치 -> 채굴기에서 창고 포트까지 컨베이어 연결 -> 철 광석 자동 저장 확인` 흐름으로 재구성했다.
- 입력용 창고 포트는 별도 출력 자원 설정 없이 들어오는 물품을 자동 저장하는 설명으로 정리했다.
- `채굴기 전력 공급 확인`과 `철 광석 창고 저장 확인` 문구를 현재 구현 가능한 완료 기준에 맞게 다듬었다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 물류 구간 스카이 대사를 입력용 창고 포트 자동 저장 흐름에 맞춰 전면 수정했다.
- 전력 공급 확인 단계는 현재 구현 기준상 송전선 연결 완료로 판정된다는 맥락이 드러나도록 보조 설명을 추가했다.

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- `POWER -> LOGI -> SMELT` 구간의 튜토리얼 완료 조건을 새 흐름에 맞춰 다시 매핑했다.
- 입력용 창고 포트 선택/설치, 컨베이어 선택/설치, 철 광석 1개 창고 저장 확인이 실제 진행 순서대로 완료되도록 조건을 재배치했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- `Q`, `E` 화면 회전 퀘스트의 제목과 설명 문구를 실제 완료 판정 기준에 맞게 정정했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- `Q`, `E` 화면 회전 관련 스카이 대사 문구를 현재 입력 동작과 일치하도록 수정했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 빌드 조작 튜토리얼에서 `Q`, `E` 화면 회전 방향이 실제 조작과 반대로 들어가 있던 문구를 수정했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- `Q`, `E` 화면 회전 방향 수정에 맞춰 스카이의 시작/완료 대사 표현도 함께 정정했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 채굴기 설치 조건 문구를 `광맥 위`가 아닌 `철 광맥 옆`에 설치하는 실제 규칙에 맞게 수정했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 채굴기 관련 스카이 대사를 `광맥 위 설치` 표현에서 `광맥 옆 설치` 표현으로 정정했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 빌드 구간의 `설치물 미리보기 확인` 퀘스트를 제거하고 `빌드 모드 진입 -> 화면 회전`으로 바로 이어지도록 정리했다.
- `TUT_MINING_001`을 `채굴기 위치 확인` 단계로 재정의하고, 흐름을 `채굴기 선택 -> 채굴기 위치 확인 -> 채굴기 설치` 순서로 재연결했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 빌드 미리보기 퀘스트 삭제에 맞춰 관련 스카이 대사를 제거하고 화면 회전 안내로 자연스럽게 이어지도록 수정했다.
- `TUT_MINING_001`의 스카이 대사를 `광맥 옆 설치 가능 위치 확인` 의미에 맞게 다시 작성했다.

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- 더 이상 사용하지 않는 `BuildPreviewSeen` 튜토리얼 완료 조건을 제거했다.
- `TUT_MINING_001`의 완료 조건을 `ValidMinerPlacement` 이벤트로 연결해 채굴기 유효 설치 위치 확인 단계가 실제로 완료되도록 했다.

### `Source/Wanted_Factory/Private/OJJ_BuildController.cpp`
- 채굴기 배치 모드에서 광맥 옆 유효 위치가 미리보기로 확인되면 `ValidMinerPlacement` 튜토리얼 이벤트를 보내도록 추가했다.

### `Source/Wanted_Factory/Private/OJJ_Player.cpp`
- 빌드 모드 진입 시 더 이상 사용하지 않는 `BuildPreviewSeen` 이벤트 전송을 제거했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- `TUT_POWER_007` 전력 공급 확인 퀘스트가 `TUT_POWER_006`과 완료 기준이 겹쳐 중복되던 문제를 정리했다.
- 전력 구간을 `송전선 연결 완료` 이후 바로 물류 단계로 이어지도록 단순화하고, 보상은 `TUT_POWER_006`에 통합했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 중복된 `TUT_POWER_007` 스카이 대사를 제거하고, `TUT_POWER_006` 완료 대사가 바로 물류 안내로 이어지도록 조정했다.

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- `TutorialEventPowerLineConnected` 완료 조건을 `TUT_POWER_006`에만 남기고 `TUT_POWER_007` 중복 매핑을 제거했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 완료 조건이 없던 `TUT_LOGI_010`을 제거하고 `TUT_LOGI_009 -> TUT_LOGI_011`로 바로 이어지도록 물류 흐름을 단순화했다.
- 창고 포트 관련 퀘스트 제목과 설명에서 `입력용/출력용`처럼 역할이 고정된 느낌의 표현을 줄이고, 상황에 맞는 연결/설정 중심 문구로 조정했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- `TUT_LOGI_010` 관련 스카이 대사를 제거했다.
- 창고 포트 관련 스카이 대사에서 `입력용/출력용 창고 포트` 표현을 줄이고, 위치와 설정에 따라 쓰임이 달라지는 장치라는 느낌이 드러나도록 문구를 수정했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 제련기 설치 설명에 `활성화된 송전탑 범위 안` 조건을 명시해 제련기 전력 공급 개념이 튜토리얼에 드러나도록 보강했다.
- 제련용 물류 구간을 `창고 포트 설치 -> 철 광석 출력 자원 설정 -> 제련기 연결 -> 결과물 회수용 창고 포트 설치 -> 결과물 회수용 컨베이어 연결 -> 철 주괴 창고 저장 확인` 흐름으로 세분화했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 제련기 전력 범위, 창고 포트의 철 광석 출력 설정, 제련기 결과물 회수용 창고 포트/컨베이어 설치 흐름이 스카이 대사에 드러나도록 물류 후반부 대사를 전면 재정리했다.

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- `TUT_LOGI_008`의 완료 조건으로 `WarehouseOutputItemSet + iron_ore` 이벤트를 추가했다.
- `TUT_LOGI_012 ~ TUT_LOGI_016` 구간에 창고 포트 선택/설치, 컨베이어 선택/설치, 철 주괴 창고 저장 확인 완료 조건을 추가했다.

### `Source/Wanted_Factory/UI/UI_MachineInteract.cpp`
- 창고 포트 출력 자원을 드롭으로 설정했을 때 `WarehouseOutputItemSet` 튜토리얼 이벤트를 보내도록 연결했다.

## 2026-06-16

### `Source/Wanted_Factory/Machines/MinerMachine.h`
- 채굴기가 전력 상태 변화에 맞춰 채굴 재시작/정지를 점검할 수 있도록 `Tick` 오버라이드 선언을 추가했다.

### `Source/Wanted_Factory/Machines/MinerMachine.cpp`
- 채굴기를 `bNeedPower = true`로 설정해 전력 소비 장비로 등록했다.
- `CanMine()`에 전력 부족 체크를 추가해 전기가 없으면 채굴을 시작하거나 유지하지 않도록 수정했다.
- `Tick()`에서 전력 복구 시 채굴을 다시 시작하고, 전력 상실 시 활성 채굴 타이머를 정지하도록 보강했다.

## 2026-06-16

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 기본 발전기 1기로는 채굴기와 제련기를 동시에 가동하기 어려운 밸런스를 반영해, 제련기 설치 전에 `추가 발전소 선택 -> 추가 발전소 설치` 퀘스트를 추가했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 제련기 진입 전 스카이 대사에 `채굴기 전력 10 + 제련기 전력 10` 설명과 추가 발전기 설치 필요성을 반영했다.

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- `TUT_POWER_007`, `TUT_POWER_008` 완료 조건을 각각 `발전소 선택`, `발전소 설치` 이벤트로 연결했다.

## 2026-06-16

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 튜토리얼이 어느 정도 확정된 시점에 맞춰 비어 있거나 역순으로 보이던 번호를 실제 진행 순서대로 다시 압축했다.
- `BUILD` 구간은 `001~006`, `MINING` 구간은 `001~003`, 제련 물류 후반부는 `LOGI_009~015`로 이어지도록 `next_quest_id`까지 함께 정리했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 빌드, 채굴, 제련 물류 후반부 스카이 대사의 `quest_id`를 새 번호 체계에 맞춰 일괄 정리했다.

### `Source/Wanted_Factory/Private/QuestManagerSubsystem.cpp`
- 번호 재정리에 맞춰 `BUILD`, `MINING`, `LOGI` 구간의 튜토리얼 완료 조건 매핑을 같은 순서로 다시 맞췄다.

## 2026-06-16

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 사용자가 대사 문장을 다듬은 뒤 `line_order`가 비연속 상태로 남아 있던 `TUT_BASIC_005 / on_start`를 `1`부터 시작하도록 정리했다.
