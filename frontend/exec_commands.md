# Exec Commands

## ClearWarehouse

- 紐⑹쟻: ?뚮젅?댁뼱 李쎄퀬????λ맂 紐⑤뱺 臾쇱옄瑜??쒓굅?섍퀬 利됱떆 ??ν빀?덈떎.
- ?ㅼ젣 ?낅젰: `ClearWarehouse`
- 濡쒓렇: ?깃났 ??Output Log??`[ClearWarehouse] Warehouse cleared.` 硫붿떆吏媛 異쒕젰?⑸땲??
- 援ы쁽 ?꾩튂: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

寃뚯엫 肄붾뱶 湲곗??쇰줈 ?꾩옱 ?ъ슜 以묒씤 `exec` 而ㅻ㎤??以? `SetBuildMode`, `TutorialLog`瑜?類 紐⑸줉?낅땲??

## TutorialAdvance

- 紐⑹쟻: ?꾩옱 ?쒗넗由ъ뼹 ?섏뒪?몃? ?뚯뒪?몄슜?쇰줈 利됱떆 ?꾨즺 泥섎━?⑸땲??
- ?ㅼ젣 ?낅젰: `TutorialAdvance`
- 援ы쁽 ?꾩튂: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## SetMachineLevel

- 紐⑹쟻: ?뱀젙 癒몄떊 ??낆쓽 ?덈꺼??吏?뺥븳 媛믪쑝濡?諛붾줈 ?ㅼ젙?⑸땲??
- ?몄옄:
  - `MachineTypeName`: 癒몄떊 ????대쫫
  - `NewLevel`: 1 ?댁긽??紐⑺몴 ?덈꺼
- ?ㅼ젣 ?낅젰: `SetMachineLevel <MachineTypeName> <NewLevel>`
- ?덉떆: `SetMachineLevel Smelter 3`
- 援ы쁽 ?꾩튂: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## UpgradeMachineLevel

- 紐⑹쟻: ?뱀젙 癒몄떊 ??낆쓽 ?덈꺼???먰븯???잛닔留뚰겮 ?щ┰?덈떎.
- ?몄옄:
  - `MachineTypeName`: 癒몄떊 ????대쫫
  - `UpgradeCount`: ?щ┫ ?잛닔, ?앸왂 ??`1`
- ?ㅼ젣 ?낅젰: `UpgradeMachineLevel <MachineTypeName> [UpgradeCount]`
- ?덉떆: `UpgradeMachineLevel Smelter 2`
- 援ы쁽 ?꾩튂: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## ResetGame

- 紐⑹쟻: ????곗씠?곕? 珥덇린?뷀븳 ???꾩옱 ?덈꺼???ㅼ떆 ?쎈땲??
- ?ㅼ젣 ?낅젰: `ResetGame`
- 援ы쁽 ?꾩튂: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## TriggerPlanetEvent

- 紐⑹쟻: ?됱꽦 ?대깽?몃? 媛뺤젣濡??쒖옉?섍굅??醫낅즺?⑸땲??
- ?몄옄:
  - `EventName`: `magnetic`, `magneticstorm`, `sand`, `sandstorm`, `none`, `clear`
  - `Severity`: ?대깽??媛뺣룄, ?앸왂 ??`1.0`
  - `DurationSeconds`: 吏???쒓컙, ?앸왂 ??`-1.0`
- ?ㅼ젣 ?낅젰: `TriggerPlanetEvent <EventName> [Severity] [DurationSeconds]`
- ?덉떆: `TriggerPlanetEvent magnetic 1.5 30`
- 鍮꾧퀬: `none` ?먮뒗 `clear`瑜??ｌ쑝硫??꾩옱 ?쒖꽦 ?대깽?몃? 醫낅즺?⑸땲??
- 援ы쁽 ?꾩튂: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`

## Give

- 목적: 창고에 원하는 아이템을 즉시 지급해 테스트에 사용합니다.
- 인자:
  - `ItemID`: 지급할 아이템 ID
  - `Count`: 지급할 개수, `1` 이상
- 실제 입력: `Give <ItemID> <Count>`
- 예시: `Give iron_ingot 10`
- 로그: 성공 시 Output Log에 `[Give] Added iron_ingot x10 to warehouse.` 형태의 메시지가 출력됩니다.
- 구현 위치: `frontend/Source/Wanted_Factory/Private/OJJ_Player.cpp`
