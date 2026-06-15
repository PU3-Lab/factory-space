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
- 주요 구간은 기본 조작 및 UI 적응, 기초 생산 라인 구축, 전력 및 물류 연결, 중반 생산 확장 목표, 메인 통신 복구 목표로 구성했다.
- 보상은 기존 기획안 기준에 맞춰 일부 단계에 `iron ingot 10`을 배치하고, 나머지 단계는 후속 밸런싱을 위해 비워 두었다.

### `Source/Wanted_Factory/Data/MachineTable.csv`
- 발전소 파생 머신 5종(`ThermalPowerPlant`, `HydroPowerPlant`, `NuclearPowerPlant`, `WindPowerPlant`, `SolarPowerPlant`) 행을 추가했다.
- 공통 스펙은 기존 `PowerPlant`와 동일하게 비용 `iron_ingot 10`, 내구도 `1000`, 전력 값 `30`, 크기 `4 x 4`로 맞췄다.
- `ThermalPowerPlant`, `NuclearPowerPlant`는 연료 투입형 발전기이므로 입력 포트 `1`, 입력 버퍼 `1`로 설정했다.
- `HydroPowerPlant`, `WindPowerPlant`, `SolarPowerPlant`는 비투입형 발전기이므로 입력 포트 `0`, 입력 버퍼 `0`으로 설정했다.
- 신규 행의 `ImgAsset`, `StaticMeshAsset`, `MaterialAsset` 값은 비워 두었다.

## 2026-06-14

### `Source/Wanted_Factory/Data/ResouceTable.csv`
- 파이프 액체 색상 관리를 위해 `PipeColor` 컬럼을 추가했다.
- 비액체 row는 `PipeColor`를 비워 두고, 액체 5종만 색상 값을 기록했다.
- 적용 대상은 `water`, `waste_water`, `acid`, `petrolium`, `mercury`다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- CSV 컬럼 구조를 `objective,reward`에서 `title,description,reward`로 확장했다.
- 각 퀘스트 단계에 퀘스트 창 표시용 제목과 세부 설명을 추가해 데이터 테이블에서 직접 활용할 수 있도록 정리했다.
- 기존 진행 흐름은 유지하면서 조작, 건설, 전력, 자동화, 통신 복구 단계의 문구를 UI 노출 기준으로 다듬었다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- CSV 컬럼 구조를 `title,description,reward`에서 `title,description,reward,scaii_dialogue`로 확장했다.
- 각 퀘스트 단계에 스카이의 진행 대사를 추가해, 퀘스트 데이터와 튜토리얼 내레이션을 함께 관리할 수 있도록 정리했다.
- 대사는 퀘스트 목표를 반복하지 않고 현재 단계의 목적과 다음 행동을 안내하는 톤으로 구성했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- CSV 컬럼 구조를 `title,description,reward,scaii_dialogue`에서 `quest_id,next_quest_id,group,title,description,reward,scaii_dialogue`로 확장했다.
- 각 단계에 고유 `quest_id`를 부여하고, 순차 진행을 위한 `next_quest_id`를 연결했다.
- 기본 조작, 건설, 채굴, 전력, 제련, 물류, 확장, 통신탑, 신호 증폭기 구간을 `group` 값으로 구분해 UI와 진행 제어에서 묶어 사용할 수 있도록 정리했다.

## 2026-06-15

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 퀘스트 본체 데이터에서 `scaii_dialogue` 컬럼을 제거하고 `quest_id,next_quest_id,group,title,description,reward` 구조만 유지하도록 정리했다.
- 퀘스트 데이터는 단계 진행과 보상 관리에 집중하도록 역할을 분리했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 스카이 대사 전용 데이터 파일을 신규 추가했다.
- `quest_id`, `trigger_type`, `line_order`, `speaker`, `dialogue` 컬럼으로 구성해 한 퀘스트에 여러 줄 대사를 연결할 수 있도록 했다.
- 각 퀘스트에 `on_start`, `on_complete` 기준의 2줄 대사를 배치해, 튜토리얼 내레이션을 데이터 기반으로 확장 가능하게 정리했다.
## 2026-06-15

### `Source/Wanted_Factory/Data/ResouceTable.csv`
- 액체별 파이프 표현 차이를 위해 `PipeColor`의 알파값도 액체마다 다르게 조정했다.
- 물은 더 투명하게, 폐수와 석유는 더 탁하고 불투명하게, 수은은 가장 진하게 보이도록 기본값을 나눴다.
