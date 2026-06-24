# Request

> 인코딩: 이 문서는 `UTF-8`로 저장하고 수정합니다. PowerShell이나 에디터에서 열고 저장할 때도 `UTF-8` 인코딩을 유지해 주세요.

## 작성 규칙
- 이 문서는 담당자가 선행한 작업이나 다른 담당자에게 요청한 작업을 기록합니다.
- 여러 담당자가 각자 자신의 AI를 사용할 수 있으니 파일 충돌에 유의해 주세요.
- 새 작업을 추가할 때는 기존 번호 다음 숫자를 사용해 `### n. 요청 요약` 형식으로 작성합니다.
- `요청 요약`에는 요청 내용을 한눈에 이해할 수 있도록 짧게 적습니다.
- 각 작업에는 아래 항목을 포함합니다: `작성자`, `작성일`, `요청 대상`, `요청 내용`, `참고 사항`
- 기존 작업 내용은 지우지 말고, 새 요청은 새 번호로 추가해 주세요.
- 작성일은 `YYYY-MM-DD` 형식으로 적습니다.

## 작업 목록

### 1. 메인퀘스트 이름 UI 표시
- 작성자: 이찬
- 작성일: 2026-06-11
- 요청 대상: 이동진
- 요청 내용: 퀘스트매니저 서브시스템에서 만들고 있는 메인퀘스트의 이름을 UI로 볼 수 있도록 구현이 필요합니다.
- 참고 사항:

### 2. 인벤토리 UI 이미지 표시 확인
- 작성자: 이찬
- 작성일: 2026-06-11
- 요청 대상: 이동진
- 요청 내용: UI 이미지가 데이터에 업데이트되었으니 인벤토리 UI에서 아이템 이미지가 정상적으로 표시되는지 확인 및 필요 시 UI 작업 부탁드립니다.
- 참고 사항:

### 3. 머신 수리 버튼 생성
- 작성자: 이찬
- 작성일: 2026-06-12
- 요청 대상: 이동진
- 요청 내용: 머신 상호작용 UI에 수리 버튼을 추가해 주세요. 버튼 클릭 시 현재 구현된 머신 수리 함수를 호출해서 창고 자원을 사용해 내구도를 회복할 수 있도록 연결이 필요합니다.
- 참고 사항:
  - `AMachineBase::RepairUsingWarehouse()`
  - `AMachineBase::GetRepairCostQtyForCurrentDurability()`
  - `AMachineBase::GetMaxRepairCostQty()`
  - `UUI_MachineInteract::OnRepairClicked()`
  - `UUI_MachineInteract`에 `BTN_Repair`를 추가하면 C++에서 바인딩되도록 구현되어 있습니다.

### 4. 튜토리얼 퀘스트/대사 UI 표시
- 작성자: 이찬
- 작성일: 2026-06-16
- 요청 대상: 이동진
- 요청 내용: 튜토리얼 퀘스트 step과 스카이 대사를 UI에 표시할 수 있도록 연결이 필요합니다. UI 쪽에서는 `QuestManagerSubsystem`이 열어둔 튜토리얼 전용 API를 사용해 주세요. 권장 방식은 `처음 한 번 현재 상태를 조회`하고, 이후에는 `이벤트 바인딩`으로 갱신하는 방식입니다.
- 참고 사항:
  - 현재 step 조회:
    - `UQuestManagerSubsystem::GetCurrentTutorialQuestStep(FTutorialQuestStep& OutStep)`
  - 특정 퀘스트의 대사 조회:
    - `UQuestManagerSubsystem::GetTutorialDialogueLines(const FString& QuestId, const FString& TriggerType, TArray<FTutorialQuestDialogueLine>& OutLines)`
  - 최근 로그된 대사 조회:
    - `UQuestManagerSubsystem::GetLastTutorialDialogueLog(FString& OutQuestId, FString& OutTriggerType, TArray<FTutorialQuestDialogueLine>& OutLines)`
  - 이벤트 바인딩:
    - `UQuestManagerSubsystem::OnTutorialStepChanged`
    - `UQuestManagerSubsystem::OnTutorialDialogueLogged`
  - 권장 UI 흐름:
    - 위젯 생성 시 `GetCurrentTutorialQuestStep`, `GetLastTutorialDialogueLog`로 초기값 반영
    - 이후 `OnTutorialStepChanged`에서 제목/설명 갱신
    - `OnTutorialDialogueLogged`에서 스카이 대사 영역 갱신
### 5. 창고 아이템 지급용 exec 추가
- 작성자: 이찬
- 작성일: 2026-06-22
- 요청 대상: 이동진
- 요청 내용: 디버그 및 테스트 편의를 위해 콘솔에서 창고에 아이템을 바로 넣을 수 있는 exec 명령을 추가해 주세요. `give iron_ingot 10`처럼 입력하면 창고에 해당 아이템 10개가 지급되는 형태입니다.
- 참고 사항:
  - `AOJJ_Player::Give(const FString& ItemID, int32 Count)`
  - `UPlayerWarehouseSubsystem::AddItem(FName ItemID, int32 Count)`
  - 사용 예시: `give iron_ingot 10`
