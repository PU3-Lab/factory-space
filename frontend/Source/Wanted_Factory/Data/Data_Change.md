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
- 빌드 조작 튜토리얼에서 `Q`, `E`를 설치물 회전이 아닌 화면 회전으로 수정했다.
- 설치물 회전 조작은 `R` 입력으로 별도 단계에 반영했다.
- 발전소와 제련기의 `설치 위치 지정` 단계를 제거해 실제 플레이 흐름에 맞게 퀘스트 단계를 단순화했다.

### `Source/Wanted_Factory/Data/tutorial_quest_dialogue.csv`
- 빌드 조작 관련 스카이 대사를 `화면 회전`과 `설치물 회전` 기준으로 다시 정리했다.
- 제거된 `설치 위치 지정` 단계에 대응하던 대사를 삭제하고, 변경된 퀘스트 순서에 맞게 완료 대사를 재연결했다.
