# 코드 리뷰: process_optimizer Power Sprint 8: Missing State Request

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `main` |
| 리뷰 일자 | 2026-06-30 |
| 리뷰 범위 | 상태 정보 부족 시 추가 스냅샷 요청을 위한 `need_more_state` 응답 프로토콜 구현 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **스펙 보강 및 스키마 추가**:
  - **[schemas.py](file:///c:/factory-space/backend/src/agents/process_optimizer/schemas.py)**:
    - `FactoryAnalysisReport` 모델에 `need_more_state` 딕셔너리 필드를 추가하여, 정보가 부족할 때 필요한 스코프 목록과 Unreal 요청 힌트를 포함할 수 있도록 했습니다.
  - **[graph_state.py](file:///c:/factory-space/backend/src/agents/process_optimizer/graph_state.py)**:
    - `ProcessOptimizerGraphState`에 `need_more_state_payload` 필드를 추가하여 Graph State 노드 간 데이터 흐름을 추적할 수 있게 했습니다.

- **정보 부족 조건 판정**:
  - **[analyzer.py](file:///c:/factory-space/backend/src/agents/process_optimizer/analyzer.py)**:
    - 백엔드가 추측하여 원인을 판단하는 대신 Unreal에 필요한 스냅샷 범위를 요청하기 위한 4대 판단 기준을 연동했습니다.
      1. **입력 부족 + `storages` 없음**: `storage_inventory` 요구 (`storages` include 힌트 제공)
      2. **철광석 부족 + `resource_nodes` 없음**: `resource_nodes` 요구 (`resource_nodes` include 힌트 제공)
      3. **기계 idle + 원인 불분명 + `durability`/`condition` 없음**: `machine_condition` 요구 (`machine_condition` include 힌트 제공)
      4. **전력 부족 + `power_grid.nodes` 없음**: `power_grid` 요구 (`power_grid` include 힌트 제공)

- **제안 및 서브퀘스트 우회 처리**:
  - **[suggestion.py](file:///c:/factory-space/backend/src/agents/process_optimizer/suggestion.py)**:
    - `need_more_state`가 감지되면 최적화 제안 생성을 건너뛰고 빈 제안 목록을 반환하도록 예외 분기 처리했습니다.
  - **[subquest_alert.py](file:///c:/factory-space/backend/src/agents/process_optimizer/subquest_alert.py)**:
    - `need_more_state` 응답이 필요한 상태에서는 플레이어 서브퀘스트 경고 알림의 빌드를 우회하도록 구현했습니다.

- **그래프 노드 처리 및 최종 응답 조립**:
  - **[nodes.py](file:///c:/factory-space/backend/src/agents/process_optimizer/nodes.py)**:
    - `calculate_metrics` 단계에서 리포트 내 `need_more_state`를 감지하여 Graph State에 기록합니다.
    - `return_preview_plan` 단계에서 `need_more_state_payload`가 존재할 시, 일반 최적화 제안 미리보기(`preview` 상태) 대신 `need_more_state` 타입의 조기 Payload를 반환하도록 조립 로직을 이원화했습니다.

- **단위 및 통합 시나리오 테스트 구축**:
  - **[test_process_optimizer.py](file:///c:/factory-space/backend/tests/test_process_optimizer.py)**:
    - `test_power_grid_missing_state_request_workflow` 단위 테스트를 추가해 6가지 시나리오를 통합 검증했습니다.
      - 시나리오 1: 입력 부족 상황에서 `storages`가 없어 `storage_inventory` 재요청이 발생하는지 확인
      - 시나리오 2: 철광석 부족 상황에서 `resource_nodes`가 없어 `resource_nodes`와 `storage_inventory` 동시 요청이 발생하는지 확인
      - 시나리오 3: 원인 미상의 기계 idle 상태에서 `durability` 등이 누락되어 `machine_condition` 재요청이 발생하는지 확인
      - 시나리오 4: 전력 부족 상황에서 송전탑 노드들이 없어 `power_grid` 재요청이 발생하는지 확인
      - 시나리오 5: 모든 필요 정보가 존재해 정상 분석(alert 또는 preview)이 원활하게 구동되는지 확인
      - 시나리오 6: LangGraph Graph State 상의 호출 흐름을 모방하여 Unreal이 부족 상태를 전송한 후 스토리지 정보를 받아 재분석 시 Sprint 6 재고 분기 제안으로 원활히 복구되는지 검증

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
스프린트 8 상태 누락에 따른 스냅샷 재요청 관련 분석 파이프라인 및 그래프 예외 분기 테스트가 `test_process_optimizer.py` 내에 완벽하게 빌드되어 성공적으로 완료되었습니다.
- `uv run pytest tests/test_process_optimizer.py` 통과 (20 passed)

### 2.2. 테스트 결과 출력 전문
```text
============================= test session starts =============================
platform win32 -- Python 3.12.12, pytest-8.4.2, pluggy-1.6.0
rootdir: C:\factory-space\backend
configfile: pyproject.toml
plugins: anyio-4.13.0, langsmith-0.8.5
collected 20 items

tests\test_process_optimizer.py ....................                     [100%]

============================= 20 passed in 3.73s ==============================
```

---

## 3. 종합 평가

스프린트 8의 핵심 철학인 **"백엔드는 정보가 부족할 때 원인을 단정하거나 예측하여 무리하게 제안을 생성하지 않고, Unreal 클라이언트에 정교한 요구 범위(`required_state_scopes`)와 Operation 힌트를 제공하여 추가 정보를 명시적으로 요청한다"**는 스펙이 예외 없이 준수되어 설계 및 구현되었습니다.
이를 통해 백엔드가 Unreal과 상호작용할 때 데이터의 무결성과 예측 가능성을 보장하고, 컨베이어의 단선이나 자원의 절대 생산 부족과 같이 헷갈리기 쉬운 복합 요인을 디버깅하기에 매우 효과적인 안전판이 마련되었습니다.
또한 기존 스프린트 6의 인벤토리 재고 판단 흐름 및 LangGraph 프레임워크와의 유기적 통합 역시 완벽하게 매끄럽게 이루어졌음을 확인했습니다.
