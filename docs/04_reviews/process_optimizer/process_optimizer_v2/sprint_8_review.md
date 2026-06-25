# Process Optimizer v2 Sprint 8 코드 리뷰 문서

## 1. 리뷰 요약

Sprint 8의 목표는 Process Optimizer v2의 전체 흐름을 스모크 테스트로 검증하는 것입니다.

검토 대상 흐름은 다음과 같습니다.

```text
analyze -> preview plan 생성
apply -> approval/revision 검증 및 실행 준비
undo -> 실행 기록 기반 되돌리기 충돌 검증
measure -> 적용 효과 측정 조건 및 결과 검증
```

초기 구현은 v2 기능 자체는 대부분 연결되어 있었지만, 스모크 테스트 runner가 후속 v2 케이스를 정확히 전송하고 검증하는 부분에 보완이 필요했습니다.

## 2. 발견한 문제

### 2.1 치환된 plan_id 메시지가 실제 전송되지 않음

`run_profile()`은 analyze 응답에서 받은 `plan_id`를 `{PLAN_ID}` 자리에 치환하도록 구성되어 있었습니다.

하지만 기존 `request_websocket_case()`는 치환된 메시지가 아니라 `case.message` 원본을 전송하고 있었습니다.

결과적으로 local smoke에서 `apply`, `undo`, `measure`가 실제 preview plan과 연결되지 않을 수 있었습니다.

### 2.2 expected_status를 검증하지 않음

`SmokeCase`에 기대 상태값을 둘 수 있었지만, `validate_case_response()`에서 `payload.status`를 확인하지 않았습니다.

예를 들어 `measurement_ready`를 기대하는 케이스가 `measurement_not_ready`를 받아도 통과할 수 있는 false positive 위험이 있었습니다.

### 2.3 agent_test_sample.json 구조 변경 미반영

`agent_test_sample.json`은 Sprint 8에서 단일 요청 샘플이 아니라 analyze/apply/undo/measure 요청 묶음으로 확장되었습니다.

하지만 기존 테스트는 여전히 최상위 JSON을 단일 `agent.request` envelope로 보고 있어 계약 검증이 실패했습니다.

### 2.4 v2 smoke 흐름 자체에 대한 단위 검증 부족

`build_profile("local")`은 기존 agent 기본 케이스 4개를 유지하고, `run_profile()` 실행 시점에 v2 케이스를 동적으로 추가하는 구조입니다.

이 설계는 기존 테스트 호환성에는 좋지만, 실제 runner가 v2 케이스를 이어서 실행하는지 별도 테스트가 필요했습니다.

## 3. 보완 내용

### 3.1 WebSocket 전송 함수 보완

`request_websocket_case()`가 선택적으로 `message` 인자를 받도록 수정했습니다.

이제 `run_profile()`에서 `{PLAN_ID}`를 치환한 메시지를 만들면, 해당 메시지가 실제 WebSocket으로 전송됩니다.

### 3.2 status 검증 추가

`validate_case_response()`에 `expected_status` 검증을 추가했습니다.

이제 v2 smoke 케이스는 응답 타입과 agent뿐 아니라 `payload.status`까지 확인합니다.

### 3.3 v2 runner 회귀 테스트 추가

`test_smoke_agent_pipeline_script.py`에 다음 검증을 추가했습니다.

```text
- local profile 실행 시 analyze 이후 apply 3개, undo 1개, measure 2개가 이어서 실행되는지 확인
- analyze에서 받은 plan_id가 후속 요청에 치환되는지 확인
- 잘못된 payload.status가 오면 SmokeError가 발생하는지 확인
```

### 3.4 계약 샘플 테스트 수정

`test_process_optimizer.py`의 `agent_test_sample.json` 검증을 확장된 구조에 맞게 수정했습니다.

현재 검증 대상은 다음 네 가지입니다.

```text
- analyze_request
- apply_request
- undo_request
- measure_request
```

각 샘플은 `ProcessOptimizerPayload`로 파싱해 operation이 기대값과 일치하는지 확인합니다.

## 4. 검증 결과

아래 테스트를 실행했습니다.

```powershell
$env:PYTHONPATH='src'; uv run pytest tests/test_smoke_agent_pipeline_script.py tests/test_process_optimizer.py tests/test_process_optimizer_smoke.py tests/test_process_optimizer_effect_measurement.py tests/test_process_optimizer_apply.py tests/test_process_optimizer_undo.py tests/test_process_optimizer_graph.py -q
```

결과:

```text
60 passed
```

문법 검증도 통과했습니다.

```powershell
$env:PYTHONPATH='src'; uv run python -m py_compile scripts/smoke_agent_pipeline.py tests/test_smoke_agent_pipeline_script.py tests/test_process_optimizer.py
```

## 5. Sprint 8 완료 판단

Sprint 8은 기획한 통합 검증 관점에서 완료로 볼 수 있습니다.

현재 smoke runner는 다음을 검증합니다.

```text
- analyze preview 생성
- approval 없는 apply 차단
- 정상 apply 실행 준비
- revision 충돌 차단
- undo 충돌 차단
- measurement_not_ready 반환
- measurement_ready 반환
```

따라서 Process Optimizer v2의 핵심 lifecycle이 테스트로 고정되었습니다.

## 6. 포트폴리오용 개선 기록

```text
변경 전
- v2 smoke runner가 동적 plan_id를 만들더라도 후속 WebSocket 요청에는 반영되지 않을 수 있었다.
- 응답 JSON의 형태만 확인하고 status 의미 검증은 약했다.

변경 후
- analyze에서 생성된 plan_id가 apply/undo/measure 후속 6개 케이스에 연결된다.
- expected_status 검증으로 잘못된 상태 응답을 통과시키는 false positive를 줄였다.
- 관련 회귀 테스트가 57 passed에서 60 passed로 확장되었다.
```
