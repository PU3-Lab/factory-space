# Process Optimizer Agent 스프린트 5 기획서

이 문서는 `process_optimizer` Agent의 **스프린트 5. Unreal UI 연동 계약과 하이라이트** 확정을 위한 기획서입니다.

## 1. 목표
- Unreal Engine 클라이언트가 NPC 대화창을 통해 최적화 Agent를 호출하고, 그 결과를 UI에 시각화(하이라이트, 카드 형태 제안 표시 등)할 수 있도록 웹소켓 통신 규격(계약 사양)을 최종 확정합니다.
- 실제 동작 코드(`runtime.py`, `schemas.py`)의 구현 스펙과 계약 문서(`unreal_websocket_contract.md`, `agent_test_sample.json`) 간의 스키마 불일치(Payload 대 Context)를 해결하여 연동 계약 신뢰성을 확보합니다.

## 2. 주요 연동 사양 확정 계획

### A. 요청 데이터 위치 확정 (`factoryRevision` & `factory_state`)
- **이슈**: 기존 연동 문서 및 샘플 JSON은 `context` 하위에 `factoryRevision` 및 `factory_state`가 위치해 있으나, 실제 Python 에이전트 파이프라인 코드는 `payload` 하위에 이를 기대하고 있습니다.
- **해결 방안**: Unreal과의 연동 계약 간결성을 위해 현재 백엔드 구현 스펙인 **`payload` 하위 배치**로 일원화하여 통일합니다.
  - 이에 맞춰 `unreal_websocket_contract.md` 및 `agent_test_sample.json` 내의 데이터를 `payload` 하위로 이동하고, `context`는 공통 메타데이터(언어 등)만 남깁니다.
  - 기존 `tests/test_process_optimizer.py` 내의 `test_unreal_websocket_contract_sample_validation` 테스트 코드도 이에 부합하도록 수정합니다.

### B. UI 하이라이트 및 제안 타겟 스펙 (`ui_hints.highlight_targets`)
- 개선 제안 대상의 구체적 타겟(예: `smelter_1`, `conv_02`) 정보를 담는 `suggestions[].target` 및 `ui_hints.highlight_targets` 스키마를 최종 승인합니다.
- 스키마 형태:
  - `target.type`: `"machine" | "conveyor" | "other"`
  - `target.id`: `string`
  - `ui_hints.highlight_targets`: `list[string]` (제안에 관련된 식별자 목록)

### C. NPC 메뉴 흐름 시퀀스 문서화 (Mermaid 활용)
- `unreal_websocket_contract.md`에 Unreal Engine 클라이언트가 '일반 질문하기(`operator_guide`)'와 '공장 최적화 제안(`process_optimizer`)' 메뉴를 어떻게 구분하여 호출하고, 시각적 하이라이트를 온/오프하는지에 대한 시퀀스 다이어그램과 UI 표시 체크리스트를 상세 추가합니다.

---

## 3. 구현 및 수정 대상
- **[MODIFY]** `docs/process_optimizer/unreal_websocket_contract.md`:
  - `factoryRevision` 및 `factory_state`를 `payload` 하위로 이동하는 요청 예시 최신화.
  - NPC 메뉴 연동 흐름에 대한 시퀀스 다이어그램(Mermaid) 및 UI 표시 체크리스트 보강.
- **[MODIFY]** `docs/process_optimizer/agent_test_sample.json`:
  - `factoryRevision` 및 `factory_state` 필드를 `payload` 하위로 이동하도록 최신화.
- **[MODIFY]** `backend/tests/test_process_optimizer.py`:
  - `test_unreal_websocket_contract_sample_validation` 테스트를 최신화된 `agent_test_sample.json` 구조를 검증하도록 수정.

---

## 4. 검증 계획
- `pytest backend/tests/test_process_optimizer.py` 및 `pytest backend/tests/test_process_optimizer_smoke.py` 실행을 통해 계약 검증용 통합 테스트가 정상 작동함을 확인합니다.
- `ruff check` 검사를 수행합니다.
