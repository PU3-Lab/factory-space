# 코드 리뷰: process_optimizer Power Sprint 6: Storage Inventory Analysis

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `main` |
| 리뷰 일자 | 2026-06-30 |
| 리뷰 범위 | 창고 재고 현황을 고려한 기계 입력 부족 병목 분석 및 맞춤 제안/서브퀘스트 구현 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **창고 배치 상태(StorageState) 스키마 정의 및 연동**:
  - **[schemas.py](file:///c:/factory-space/backend/src/agents/process_optimizer/schemas.py)**:
    - `StorageState` 및 `StorageInventoryItem` 데이터 계약 정의를 추가하여, 공장 스냅샷(`FactoryState`)과 분석 결과(`FactoryAnalysisReport`)에 `storages` 목록을 유지하도록 확장했습니다.
    - 기계의 부족 자원 매핑 정보인 `input_shortages_items: dict[str, str]` 를 보강했습니다.
    - `TargetDescriptor` 의 type 리터럴에 `"storage"`를 추가했습니다.

- **창고 재고 기반 부족 원인 분석 다각화**:
  - **[analyzer.py](file:///c:/factory-space/backend/src/agents/process_optimizer/analyzer.py)**:
    - `analyze` 진행 과정에서 입력 부족 기기가 감지될 경우, 해당 시점에 부족했던 자원의 `item_id`를 `input_shortages_items` 맵에 짝지어 기록하여 반환하도록 설계했습니다.
  - **[suggestion.py](file:///c:/factory-space/backend/src/agents/process_optimizer/suggestion.py)**:
    - 부족한 `item_id`에 대해 창고들의 재고량을 합산하여 분기 판정을 추가했습니다:
      - **창고 재고 있음 (합산량 > 0.0)**: 공급 경로 상의 장애(컨베이어 끊어짐 등)로 보고 `inspect_{item_id}_supply_{machine_id}` (공급 라인 점검, risk: low) 제안을 발행합니다.
      - **창고 재고 없음 (합산량 <= 0.0)**: 자원 생산량 자체가 부족한 것으로 보고 `expand_{item_id}_production_{machine_id}` (생산량 확충, risk: medium) 제안을 발행합니다.
      - **창고 정보 없음**: 레거시 일반 입력 부족 제안(`suggest_input_{machine_id}`)으로 폴백합니다.

- **원인에 따른 최적화 서브퀘스트(Subquest) 목표 세분화**:
  - **[subquest_alert.py](file:///c:/factory-space/backend/src/agents/process_optimizer/subquest_alert.py)**:
    - `SubquestAlertBuilder.build_alert` 의 기계 입력 부족 처리부에서 창고 재고에 따라 분기를 적용했습니다:
      - **창고 재고 있음**: 서브퀘스트 제목을 `"{item_name_ko} 공급 라인 점검"`으로 지정하고, 목표에 창고와 기계 사이 벨트 수동 연결을 유도합니다.
      - **창고 재고 없음**: 서브퀘스트 제목을 `"{item_name_ko} 생산량 확충"`으로 지정하고, 목표에 채굴기나 생산시설 신축을 수동 지시합니다.
      - **창고 정보 없음**: 레거시 일반 입력 라인 복구 경고로 롤백합니다.

- **창고 상태 분기 분석 및 총량 합산 유닛 테스트 구축**:
  - **[test_process_optimizer.py](file:///c:/factory-space/backend/tests/test_process_optimizer.py)**:
    - `test_power_grid_inventory_analysis_workflow` 유닛 테스트를 추가했습니다.
    - 재고 있음 상태(시나리오 1), 재고 부족 상태(시나리오 2), 스토리지 생략 폴백 상태(시나리오 3), 복수 창고 분산 재고 합산 분석 상태(시나리오 4)의 제안 ID 및 서브퀘스트 타이틀/목표를 정밀 검증하는 테스트 코드를 구축했습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
스토리지 재고 연계 입력 부족 판별 및 분산 합산, 폴백 테스트를 포함하여 `test_process_optimizer.py` 내의 모든 18개 테스트가 완벽히 통과되었습니다.
- `uv run pytest tests/test_process_optimizer.py` 통과 (18 passed)

### 2.2. 테스트 결과 출력 전문
```text
============================= test session starts =============================
platform win32 -- Python 3.12.12, pytest-8.4.2, pluggy-1.6.0
rootdir: C:\factory-space\backend
configfile: pyproject.toml
plugins: anyio-4.13.0, langsmith-0.8.5
collected 18 items

tests\test_process_optimizer.py ..................                       [100%]

============================= 18 passed in 4.19s ==============================
```

---

## 3. 종합 평가

스프린트 6의 핵심 목표인 **"창고 재고와 기계 입력 버퍼 연계를 통한 정밀한 공급 병목 분석 및 맞춤 제안 도출"**이 기획 사양을 충족하여 견고하게 구현되었습니다.
기존의 획일적인 입력 부족 정보 대신, 자원 재고 존재 여부를 파악해 컨베이어 수리를 유도할지 혹은 채굴 설비 추가를 권고할지 똑똑하게 나누어 가이드해 줌으로써 플레이어의 게임 내 문제 해결을 효율적이고 유기적으로 돕도록 시스템 완성도가 극대화되었습니다.
