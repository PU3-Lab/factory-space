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
- 조작 학습, 빌드 모드, 채굴, 전력 연결, 제련, 자동화 학습까지 실제 체크 가능한 단계로 나눴다.
- 주요 구간은 기본 조작 및 UI 적응, 기초 생산 라인 구축, 전력 및 물류 연결, 중반 생산 확장 목표, 자동화 이해 목표로 구성했다.
- 보상은 기존 기획 기준에 맞춰 일부 단계에 `iron ingot 10`을 배치하고 나머지는 후속 밸런싱 여지를 남겨 두었다.

### `Source/Wanted_Factory/Data/MachineTable.csv`
- 발전소 파생 머신 5종 `ThermalPowerPlant`, `HydroPowerPlant`, `NuclearPowerPlant`, `WindPowerPlant`, `SolarPowerPlant` 행을 추가했다.
- 공통 스펙은 기존 `PowerPlant`와 동일하게 비용 `iron_ingot 10`, 내구도 `1000`, 전력 값 `30`, 크기 `4 x 4`로 맞췄다.
- `ThermalPowerPlant`, `NuclearPowerPlant`는 연료 투입형 발전기이므로 입력 포트 `1`, 입력 버퍼 `1`로 설정했다.
- `HydroPowerPlant`, `WindPowerPlant`, `SolarPowerPlant`는 비투입형 발전기이므로 입력 포트 `0`, 입력 버퍼 `0`으로 설정했다.
- 신규 행의 `ImgAsset`, `StaticMeshAsset`, `MaterialAsset` 값은 비워 두었다.
