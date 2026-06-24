# 코드 리뷰: process_optimizer Sprint 1 (상태 입력 계약과 Agent 뼈대)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `main` |
| 리뷰 일자 | 2026-06-24 |
| 리뷰 범위 | process_optimizer 상태 입력 계약 검증 및 Agent 파이프라인 뼈대 연동 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **요청 검증(Request Validation) 보강**:
  - [runtime.py](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py)의 `validate_process_payload` 함수를 수정하여 `factoryRevision` 및 `factory_state` 필드에 대한 검증 로직을 구현했습니다.
  - `state_update` 요청일 경우 두 필드가 필수값으로 포함되어 있는지 확인하며, `analyze` 요청인 경우에도 값이 제공된다면 타입 유효성 및 Pydantic 스키마 검증을 일관되게 수행합니다.
  - `factoryRevision`은 0 이상의 양의 정수 형태여야 하며, `factory_state`는 `FactoryState` 스키마 규격을 만족하지 못할 시 `INVALID_REQUEST_PAYLOAD` 에러를 즉각 반환하도록 예외 처리를 조기에 실행합니다.
  - 이를 통해 잘못된 정보가 메모리나 에이전트 내부에 저장되는 것을 미들웨어 수준에서 선제 차단합니다.
- **임포트 구조 최적화 (AGENTS.md 규칙 준수)**:
  - 파일 하단의 에이전트 분기 내부에서 수행되던 `FactoryState`, `ProcessOptimizerPayload`, `process_optimizer_memory` 관련 로컬 임포트 코드를 제거하고, [runtime.py](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py) 상단에 전역 임포트로 통합 배치했습니다.
- **테스트 케이스 추가 및 검증**:
  - [test_process_optimizer.py](file:///c:/factory-space/backend/tests/test_process_optimizer.py) 내부에 `test_process_optimizer_invalid_payload_validation` 테스트 케이스를 신설했습니다.
  - `machines` 필드가 리스트 형식이 아닌 문자열(invalid factory_state)인 경우와 `factoryRevision`이 음수인 경우 각각 `INVALID_REQUEST_PAYLOAD` 코드를 담은 `agent.error` 봉투가 정상 반환되는지 성공적으로 검증했습니다.
- **Sprint 1 보완 구현**:
  - 빈 `{}` 형태의 `factory_state`도 유효한 공장 snapshot으로 인정되도록 존재 여부 검사를 `is None` 기준으로 보정했습니다.
  - `process_optimizer_state_update` 내부의 로컬 임포트를 제거하고 파일 상단 전역 임포트 구조로 정리했습니다.
  - `analyze` 응답은 `ProcessOptimizerResponse` 스키마를 통과하도록 검증을 추가해, 기본 응답 구조가 깨진 JSON이 그대로 반환되지 않게 했습니다.
  - 빈 `factory_state`를 가진 `state_update` 요청이 정상 처리되는 회귀 테스트를 추가했습니다.
- **린트 및 포맷 검사 통과**:
  - `ruff check --fix`를 활용하여 새로 수정한 코드 영역의 임포트 순서 정리 및 안 쓰는 임포트 제거 등 모든 정적 분석 규칙을 만족시켰습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
`process_optimizer` 관련 단위 및 통합(smoke) 테스트가 모두 그린 빌드(Green Build)를 기록했습니다.
- `uv run pytest tests/test_process_optimizer.py tests/test_process_optimizer_smoke.py` 통과 (11 passed)
- `uv run ruff check src/agents/pipeline/runtime.py tests/test_process_optimizer.py` 코드 포맷 및 린트 검사 통과 (All checks passed)

### 2.2. 테스트 결과 출력 전문
```text
============================= test session starts =============================
platform win32 -- Python 3.12.12, pytest-8.4.2, pluggy-1.6.0
rootdir: C:\factory-space\backend
configfile: pyproject.toml
plugins: anyio-4.13.0, langsmith-0.8.5
collected 11 items

tests\test_process_optimizer.py ........                                 [ 72%]
tests\test_process_optimizer_smoke.py ...                                [100%]

============================= 11 passed in 3.71s ==============================
```

---

## 3. 종합 평가

스프린트 1에서 정의한 상태 입력 계약 및 뼈대 구축 작업이 안정적으로 마무리되었습니다.
특히, Unreal Engine으로부터 유입되는 다양한 공장 상태 정보의 정합성을 보장하기 위해 파이프라인 시작점(`validate_process_payload`)에서 Pydantic 모델을 통한 구조적 유효성 검증을 전면 수행하도록 설계함으로써 런타임 단계에서 예기치 못한 데이터 유실이나 타입 에러를 미연에 방지할 수 있게 되었습니다.
또한, 잘못된 payload 입력 시 올바른 프로토콜 규격인 `AgentErrorEnvelope` 형태(`agent.error` 타입 및 `INVALID_REQUEST_PAYLOAD` 에러 코드)로 예외 응답을 클라이언트에 명확하게 반환하도록 보완되어 통합 완성도를 높였습니다.
