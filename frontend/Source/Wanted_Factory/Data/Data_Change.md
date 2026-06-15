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
