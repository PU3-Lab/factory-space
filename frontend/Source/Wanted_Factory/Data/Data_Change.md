# Data Change Log

## 2026-06-11

### `Source/Wanted_Factory/Data/MachineTable.csv`
- `Synthesizer` 머신 레벨 데이터 3건(`Synthesizer_Lv1`, `Synthesizer_Lv2`, `Synthesizer_Lv3`)을 추가했다.
- 추가된 항목의 공통 스펙:
  - 크기 `3 x 3`
  - 입력 포트 `2`
  - 출력 포트 `2`
  - 입력 버퍼 `1`
  - 출력 버퍼 `1`
  - 전력 소모 `10`
  - 내구도 `1000`

### `Content/DataTable/MachineTable.csv`
- 콘텐츠용 데이터테이블에 `Synthesizer` 행 1건을 추가했다.
- 스펙은 `3 x 3`, 입력 포트 `2`, 출력 포트 `2`, 입력 버퍼 `1`, 출력 버퍼 `1`로 설정했다.

## 2026-06-12

### `Source/Wanted_Factory/Data/tutorial_quest_steps.csv`
- 초반 튜토리얼과 메인 목표 흐름을 `objective`, `reward` 컬럼 구조의 CSV로 신규 추가했다.
- 조작 학습, 빌드 모드, 철거, 채굴, 전력 연결, 제련, 자동화, 통신탑, 신호 증폭기까지를 실제 체크 가능한 단계로 세분화했다.
- 주요 단계는 다음 구간으로 구성했다:
  - 기본 조작 및 UI 적응
  - 기초 생산 라인 구축
  - 제련 및 물류 자동화
  - 중간 생산 확장 목표
  - 통신탑 및 신호 증폭기 설치 목표
- 보상은 기존 기획안 기준에 맞춰 일부 단계에 `iron ingot 10`을 배치했고, 나머지 단계는 후속 밸런싱을 위해 공란으로 유지했다.
