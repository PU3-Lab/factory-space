# Exec Commands

현재 게임에서 사용할 수 있는 `exec` 커맨드 정리 문서입니다.  
이 문서에서는 `SetBuildMode`, `TutorialLog`를 제외한 항목만 다룹니다.

## ClearWarehouse

- 목적: 창고에 들어 있는 모든 아이템을 비워 테스트 상태를 빠르게 초기화합니다.
- 명령어: `ClearWarehouse`
- 확인 방법: Output Log에 `[ClearWarehouse] Warehouse cleared.` 로그가 출력됩니다.
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## TutorialAdvance

- 목적: 현재 튜토리얼 퀘스트를 테스트용으로 즉시 완료 처리합니다.
- 명령어: `TutorialAdvance`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## SetMachineLevel

- 목적: 특정 머신 타입의 레벨을 지정한 값으로 바로 설정합니다.
- 인자:
  - `MachineTypeName`: 머신 타입 이름
  - `NewLevel`: 목표 레벨, `1` 이상
- 명령어: `SetMachineLevel <MachineTypeName> <NewLevel>`
- 예시: `SetMachineLevel Smelter 3`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## UpgradeMachineLevel

- 목적: 특정 머신 타입의 레벨을 원하는 횟수만큼 올립니다.
- 인자:
  - `MachineTypeName`: 머신 타입 이름
  - `UpgradeCount`: 올릴 횟수, 기본값 `1`
- 명령어: `UpgradeMachineLevel <MachineTypeName> [UpgradeCount]`
- 예시: `UpgradeMachineLevel Smelter 2`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## ResetGame

- 목적: 저장 데이터를 초기화한 뒤 현재 레벨을 다시 엽니다.
- 명령어: `ResetGame`
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## TriggerPlanetEvent

- 목적: 행성 이벤트를 강제로 시작하거나 종료합니다.
- 인자:
  - `EventName`: `magnetic`, `magneticstorm`, `sand`, `sandstorm`, `none`, `clear`
  - `Severity`: 이벤트 강도, 기본값 `1.0`
  - `DurationSeconds`: 지속 시간, 기본값 `-1.0`
- 명령어: `TriggerPlanetEvent <EventName> [Severity] [DurationSeconds]`
- 예시: `TriggerPlanetEvent magnetic 1.5 30`
- 비고: `none` 또는 `clear`를 입력하면 현재 활성 이벤트를 종료합니다.
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## Give

- 목적: 테스트용으로 원하는 아이템을 창고에 즉시 지급합니다.
- 인자:
  - `ItemID`: 지급할 아이템 ID
  - `Count`: 지급할 개수, `1` 이상
- 명령어: `Give <ItemID> <Count>`
- 예시: `Give iron_ingot 10`
- 확인 방법: Output Log에 `[Give] Added iron_ingot x10 to warehouse.` 형태의 로그가 출력됩니다.
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`
