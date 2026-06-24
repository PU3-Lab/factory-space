# Process Optimizer Agent 스프린트 4 구현 기획서

이 문서는 `process_optimizer` Agent의 **스프린트 4. LLM 설명 생성과 프롬프트 방어**를 위한 구현 기획서입니다.

## 1. 목표 및 범위
- **목표**: 코드가 생성한 분석/제안 결과를 LLM이 친근한 어조로 윤색하되, 시스템 프롬프트 인젝션 및 허가되지 않은 자동 실행 명령 생성을 완벽히 차단하고, LLM 실패 시 안전한 fallback 제안을 반환하도록 설계합니다.
- **범위**:
  - `runtime.py` 내의 `validate_process_payload`에서 `factoryRevision` 및 `factory_state` 데이터의 세부 유효성 검증을 보강하여 Sprint 1의 뼈대 요구사항을 완성합니다.
  - `process_optimizer_state_update` 노드를 추가하여 `state_update` 요청 시 세션 메모리를 갱신하고 바로 응답을 반환하도록 처리합니다.
  - `validate_response_schema` 및 LLM 응답 파싱 단계에서 `SuggestionValidationTool`을 활용하여 반환 제안의 실행 가능 명령어(`set_recipe`, `connect_conveyor` 등) 포함 여부를 검증하고, 검증 실패 시 결정론적 Fallback(`ProcessOptimizerAgent.fallback`)을 수행하는 로직을 결합합니다.
  - 모의 LLM 및 파이프라인 연계 방어 기능을 검증하는 2가지 Smoke Test를 추가합니다.

## 2. 세부 설계 및 흐름

### A. 공장 상태 입력 데이터 검증 (`validate_process_payload`)
- 클라이언트로부터 `factoryRevision`과 `factory_state`가 들어왔을 때, Pydantic 스키마를 통해 데이터 타입 및 유효성을 엄격하게 체크합니다.
- `factoryRevision`이 음수이거나, `factory_state` 내의 `machines` 등 필드가 리스트가 아닌 문자열로 주어지는 등의 비정상 요청에 대해서는 즉시 `INVALID_REQUEST_PAYLOAD` 에러를 반환합니다.

### B. 세션 메모리 연동 및 상태 업데이트 (`process_optimizer_state_update`)
- `operation == "state_update"` 분기인 경우, 세션 메모리(`process_optimizer_memory`)에 `factory_state` 및 `factoryRevision`을 저장하고 즉시 `status: "success"` 형태의 응답을 반환합니다. 이 과정에서는 LLM을 호출하지 않습니다.

### C. LLM 응답 스키마 검증 및 프롬프트 인젝션 방어 (`validate_response_schema`)
- LLM 응답을 파싱한 후, `validate_response_schema`에서 반환된 페이로드의 형식을 `ProcessOptimizerResponse` 스키마로 검증합니다.
- 동시에 `SuggestionValidationTool.validate_suggestions`를 호출하여 제안된 항목들 중 `set_recipe`, `move_machine` 등의 실행 제안이 있는지 검사합니다.
- LLM이 잘못된 JSON을 반환했거나, 프롬프트 인젝션 등의 이유로 비정상 제안(실행 명령이 포함된 제안 등)을 생성한 경우, 예외를 터뜨리지 않고 결정론적으로 미리 정의된 `ProcessOptimizerAgent.fallback` 메서드를 호출하여 안전한 포맷의 제안 응답을 리턴하도록 처리합니다.

---

## 3. 구현 대상 파일 목록
- **[MODIFY]** `backend/src/agents/pipeline/runtime.py`:
  - `validate_process_payload` 함수 보강 (Pydantic 검증 적용)
  - `process_optimizer_state_update` 노드 함수 추가 및 LangGraph 등록
  - `validate_response_schema` 및 관련 흐름에서 `SuggestionValidationTool` 검증 연동 및 fallback 호출 로직 추가
- **[MODIFY]** `backend/tests/test_process_optimizer_smoke.py`:
  - `test_process_optimizer_prompt_injection_defense_smoke` 추가 (인젝션/실행명령 포함 시 fallback 작동 검증)
  - `test_process_optimizer_invalid_json_fallback_smoke` 추가 (LLM 응답 JSON 에러 시 fallback 작동 검증)

---

## 4. 검증 계획
- **자동 테스트**: `pytest backend/tests/test_process_optimizer.py` 및 `pytest backend/tests/test_process_optimizer_smoke.py` 실행
- **린트 검사**: `ruff check .` 및 `ruff format .` 실행하여 코드 정렬 및 오류 방지
