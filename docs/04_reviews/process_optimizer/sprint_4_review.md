# 코드 리뷰: process_optimizer Sprint 4 (LLM 설명 생성과 프롬프트 방어)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `main` |
| 리뷰 일자 | 2026-06-24 |
| 리뷰 범위 | LLM 설명 생성, 프롬프트 인젝션 방어 검증, 유효성 검증 및 Fallback 연동 |
| 리뷰어 | Antigravity |

## 1. 변경 요약

- **공장 상태 데이터 유효성 검증 완성**:
  - `runtime.py` 내의 `validate_process_payload` 노드를 수정하여 Pydantic 모델을 통한 Payload 유효성 검사, `factoryRevision` 음수 값 방지, `factory_state` 데이터 구조 형식 정밀 점검을 통합했습니다. (Sprint 1 미결 뼈대 완수)
- **세션 메모리 업데이트 자동화**:
  - `process_optimizer_state_update` 노드를 LangGraph 파이프라인에 추가 및 등록하여 `state_update` 명령이 들어왔을 때 오케스트레이터 LLM을 거치지 않고 세션 메모리를 즉시 동기화한 후 `success` 응답을 전달하도록 보강했습니다.
- **LLM 예외 대응 및 Fallback 복구 결합**:
  - `parse_llm_response`와 `validate_response_schema` 노드에서 `process_optimizer` 에이전트 실행 도중 LLM이 잘못된 포맷의 JSON을 생성하거나(JSONDecodeError), 프롬프트 인젝션 공격에 노출되어 금지된 명령어(`set_recipe`, `move_machine` 등)를 suggestions 페이로드에 섞어 반환하는 보안 유효성 검증 실패 상황을 완벽하게 잡아내도록 수정했습니다.
  - 검증에 실패할 경우 파이프라인 크래시를 유발하는 대신 결정론적인 Fallback 동작(`ProcessOptimizerAgent.fallback`)을 즉시 호출하도록 설계하여, 사용자에게 항상 규격화된 안전한 제안을 보장합니다.
- **테스트 케이스 확장 (Smoke Test 추가)**:
  - **인젝션 공격 차단 및 Fallback 복구 검증 (`test_process_optimizer_prompt_injection_defense_smoke`)**: LLM 응답에 악의적인 실행 제안이 섞여 들어왔을 때 최종 출력 검증 단계에서 감지되어 Fallback 처리되는 시나리오를 통합 검증합니다.
  - **비정상 응답 Fallback 복구 검증 (`test_process_optimizer_invalid_json_fallback_smoke`)**: LLM이 JSON 형식을 따르지 않는 일반 깨진 텍스트를 돌려주었을 때도, 파이프라인에서 JSON 디코딩 실패를 복구하여 결정론적 제안 데이터를 포함한 정상 봉투를 리턴함을 증명합니다.

---

## 2. 이슈 목록

### ⚪ Nit: 탑 레벨 에이전트 direct routing 하드코딩 분기 리팩토링
- **위치**: [runtime.py:265](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py#L265)
- **내용**: `envelope.agent in {"operator_guide", "process_optimizer"}` 처럼 direct routing(오케스트레이터 우회)을 처리하기 위해 에이전트 ID 목록이 조건문에 직접 하드코딩되어 있습니다.
- **영향**: 향후 새로운 탑 레벨 에이전트가 추가되거나 오케스트레이터를 우회해야 하는 대상이 변경될 때, 이 소스 코드를 수동으로 편집하여 수정해야 하는 유지보수 상의 번거로움과 휴먼 에러의 소지가 있습니다.
- **제안**: `TOP_LEVEL_AGENT_IDS` 상수나 설정값 리스트를 조회하여 동적으로 우회 여부를 판별하도록 변경하거나, 오케스트레이터의 라우팅 유연성을 보완하는 구조로 리팩토링하는 것을 권장합니다.

---

## 3. 검증 결과

### 3.1. 자동화 테스트 결과
`process_optimizer` 관련 테스트 세트가 총 32개로 대폭 강화되었으며, 100% 성공(그린 빌드) 및 Ruff 린트/포맷 검사를 완벽하게 통과했습니다.
- **통합 및 Smoke 테스트**: `uv run pytest tests/test_process_optimizer.py tests/test_process_optimizer_smoke.py` 통과
- **도구 및 프롬프트 단위 테스트**: `uv run pytest tests/test_process_optimizer_prompt.py tests/test_process_optimizer_analyzer.py tests/test_process_optimizer_suggestion.py` 통과
- **정적 분석 및 포맷팅**: `ruff check --fix .` 및 `ruff format .` 통과 (All checks passed)

### 3.2. 테스트 결과 출력 전문
```text
============================= test session starts =============================
platform win32 -- Python 3.12.12, pytest-8.4.2, pluggy-1.6.0
rootdir: C:\factory-space\backend
configfile: pyproject.toml
plugins: anyio-4.13.0, langsmith-0.8.5
collected 32 items

tests\test_process_optimizer.py ........                                 [ 25%]
tests\test_process_optimizer_analyzer.py .........                       [ 53%]
tests\test_process_optimizer_prompt.py ....                              [ 65%]
tests\test_process_optimizer_smoke.py .....                              [ 81%]
tests\test_process_optimizer_suggestion.py ......                        [100%]

============================= 32 passed in 8.12s ==============================
```

---

## 4. 종합 평가

스프린트 4의 핵심 목표인 **"LLM 설명 생성 시 프롬프트 인젝션 및 비정상 응답에 대한 완벽한 방어선 구축"**이 대단히 성공적으로 완수되었습니다.
LLM이 프롬프트 공격에 취약할 수밖에 없는 현실적인 한계를 인정하고, 파이프라인의 최종 스키마 검증과 `SuggestionValidationTool`을 활용한 차단 메커니즘을 2단계로 연동함으로써 높은 견고함을 갖추었습니다.
네트워크 오류 혹은 LLM JSON Decode 실패 시에도 클라이언트에 에러를 전송하지 않고 deterministic fallback을 수행하여 공장의 일관된 분석 리포트를 제공할 수 있게 되었으며, 이로써 배포 가능한 완성도 높은 에이전트 파이프라인이 완성되었습니다.
