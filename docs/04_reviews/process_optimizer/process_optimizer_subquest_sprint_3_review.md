# 코드 리뷰 보고서: Process Optimizer Subquest Sprint 3 (Unreal Quest Integration)

이 문서는 `docs/process_optimizer/process_optimizer_subquest_sprint_3_unreal_quest_plan.md` 기준으로 Sprint 3 구현을 검토한 결과를 정리한다.

---

## 1. 변경 요약

- **서브퀘스트 후보 계약 보강**
  - `OptimizationAlert.suggested_subquest`가 Unreal UI에서 바로 후보 카드로 사용할 수 있도록 `target`, `severity`, `next_request` 정보를 포함하도록 정리되었다.
  - `SuggestedSubquestNextRequest`는 `agent`, `operation`, `goal`, `request_source`, `target`을 포함해, Unreal이 최신 `factoryRevision`과 `factory_state`만 다시 붙여 `analyze` 요청을 만들 수 있는 형태다.

- **state_update -> analyze -> apply smoke 흐름 검증**
  - `backend/tests/test_process_optimizer_subquest_unreal_flow.py`를 추가해 Sprint 3의 핵심 연결 흐름을 테스트한다.
  - `state_update`는 `optimization_alert`와 `suggested_subquest`만 반환하고, `commands`, `plan_id`, `changes`를 만들지 않는 것을 확인한다.
  - Unreal이 `suggested_subquest.next_request`에 최신 공장 상태를 붙여 `analyze` 요청을 보내면 `preview`가 반환되는 것을 확인한다.
  - 플레이어 승인 이후 `apply approval=true` 요청에서만 `execute_ready`와 `commands`가 반환되는 것을 확인한다.

- **문서 예시 보강**
  - `backend/docs/agent_test_operator_process_examples.md`에 agent-test용 Sprint 3 예시를 추가했다.
  - `backend/docs/unreal_agent_json_examples.md`에 Unreal 연동용 서브퀘스트 후보 흐름을 추가했다.
  - `docs/process_optimizer/unreal_websocket_contract.md`에 Sprint 3 서브퀘스트 후보 연동 규칙을 추가했다.

---

## 2. 이슈 목록

### Minor: 문서의 기존 한글 인코딩 표시가 일부 깨져 보임

- **위치**:
  - `backend/docs/agent_test_operator_process_examples.md`
  - `backend/docs/unreal_agent_json_examples.md`
  - `docs/process_optimizer/unreal_websocket_contract.md`
- **내용**: 기존 문서 일부가 PowerShell 출력에서 깨진 한글로 보인다. 이번 Sprint 3 섹션은 정상 UTF-8 한글로 추가했지만, 문서 전체의 가독성은 별도 정리 여지가 있다.
- **영향**: 코드 동작에는 영향이 없다. 다만 포트폴리오/협업 문서로 읽을 때 가독성이 떨어질 수 있다.
- **권고**: Sprint 3 기능과 별개로 문서 인코딩/문장 정리 작업을 후속 문서 정리 백로그로 분리하는 것을 권장한다.

### Minor: Quest Generator 직접 연동은 아직 포함하지 않음

- **위치**: Sprint 3 구현 범위 전체
- **내용**: 기획서 권장안에 따라 이번 Sprint에서는 Option A, 즉 `process_optimizer`가 직접 `suggested_subquest` 문구와 다음 요청 정보를 반환하는 방식만 구현했다.
- **영향**: 현재 Unreal UI 후보 표시는 가능하지만, quest_generator의 보상/체인/분류 로직과는 아직 연결되지 않는다.
- **권고**: 장기 퀘스트 체인과 보상이 필요해지는 시점에 별도 Sprint로 Option B 연동을 설계한다.

---

## 3. 우선순위 권고

- **즉시 조치 필요 없음**
  - Sprint 3의 필수 목표인 `state_update` 후보 생성, 후보 클릭 후 `analyze`, 승인 후 `apply` 흐름은 테스트로 검증되었다.

- **후속 백로그 권장**
  - 기존 문서 한글 인코딩/문장 정리
  - Quest Generator 직접 연동 여부 재검토
  - Sprint 1/2에서 남긴 `subquest_mode` 기본값 합의, 장비/아이템 이름 매핑 분리, suggestion category 필드화

---

## 4. 긍정적인 부분

- `state_update`가 여전히 자동 실행 명령을 만들지 않아, "플레이어 검토 후 승인"이라는 핵심 안전 원칙을 유지한다.
- `suggested_subquest.next_request`가 Unreal이 다음 요청을 만들기 쉬운 구조로 정리되어, UI 후보 카드와 백엔드 분석 흐름이 자연스럽게 연결된다.
- `analyze` preview와 `apply` 승인 경계가 테스트에서 함께 검증되어, 딸각 자동 변경 흐름으로 변하지 않도록 막고 있다.

---

## 5. 검증 결과

새로 추가한 Sprint 3 테스트:

```text
backend/tests/test_process_optimizer_subquest_unreal_flow.py ..          [100%]

2 passed in 3.23s
```

관련 process_optimizer 회귀 테스트:

```text
backend/tests/test_process_optimizer.py
backend/tests/test_process_optimizer_state_update_alert.py
backend/tests/test_process_optimizer_target_analyze.py
backend/tests/test_process_optimizer_subquest_unreal_flow.py
backend/tests/test_process_optimizer_analyzer.py

35 passed in 4.13s
```
