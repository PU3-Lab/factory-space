# Exec Commands

## ClearWarehouse

- 목적: 플레이어 창고에 저장된 모든 물자를 제거하고 즉시 저장합니다.
- 실제 입력: `ClearWarehouse`
- 로그: 성공 시 Output Log에 `[ClearWarehouse] Warehouse cleared.` 메시지가 출력됩니다.
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

게임 코드 기준으로 현재 사용 중인 `exec` 커맨드 중, `SetBuildMode`, `TutorialLog`를 뺀 목록입니다.

## TutorialAdvance

- 목적: 현재 튜토리얼 퀘스트를 테스트용으로 즉시 완료 처리합니다.
- 실제 입력: `TutorialAdvance`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## SetMachineLevel

- 목적: 특정 머신 타입의 레벨을 지정한 값으로 바로 설정합니다.
- 인자:
  - `MachineTypeName`: 머신 타입 이름
  - `NewLevel`: 1 이상의 목표 레벨
- 실제 입력: `SetMachineLevel <MachineTypeName> <NewLevel>`
- 예시: `SetMachineLevel Smelter 3`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## UpgradeMachineLevel

- 목적: 특정 머신 타입의 레벨을 원하는 횟수만큼 올립니다.
- 인자:
  - `MachineTypeName`: 머신 타입 이름
  - `UpgradeCount`: 올릴 횟수, 생략 시 `1`
- 실제 입력: `UpgradeMachineLevel <MachineTypeName> [UpgradeCount]`
- 예시: `UpgradeMachineLevel Smelter 2`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## ResetGame

- 목적: 저장 데이터를 초기화한 뒤 현재 레벨을 다시 엽니다.
- 실제 입력: `ResetGame`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## TriggerPlanetEvent

- 목적: 행성 이벤트를 강제로 시작하거나 종료합니다.
- 인자:
  - `EventName`: `magnetic`, `magneticstorm`, `sand`, `sandstorm`, `none`, `clear`
  - `Severity`: 이벤트 강도, 생략 시 `1.0`
  - `DurationSeconds`: 지속 시간, 생략 시 `-1.0`
- 실제 입력: `TriggerPlanetEvent <EventName> [Severity] [DurationSeconds]`
- 예시: `TriggerPlanetEvent magnetic 1.5 30`
- 비고: `none` 또는 `clear`를 넣으면 현재 활성 이벤트를 종료합니다.
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`
