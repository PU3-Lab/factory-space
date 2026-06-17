# 코드 리뷰: operator_guide RAG Sprint 15 (Current Game State Tool & Context Need Classifier)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-game-state` |
| 리뷰 일자 | 2026-06-16 |
| 리뷰 범위 | ContextNeedClassifier, CurrentGameStateTool 설계 및 연동, 프롬프트 주입 및 메타데이터 전송 |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- **Game State 메타데이터 스키마 확장**: 
  - [schemas.py](file:///c:/factory-space/backend/src/agents/operator_guide/schemas.py)의 `ManualQAResult` 모델 및 `to_metadata()` API 응답 데이터 모델에 Unreal 및 API 클라이언트 규약을 위한 `requiresCurrentGameState`, `usedCurrentGameState`, `requiredStateScopes`, `availableScopes` 키를 카멜케이스로 매핑하여 추가했습니다.
- **Prompt Context 구조 업데이트**:
  - [manual_context_builder.py](file:///c:/factory-space/backend/src/agents/operator_guide/manual_context_builder.py)의 `ManualQAPromptContext` 데이터 모델에 실시간 게임 상태 문자열과 상태 여부/스코프 메타데이터 필드들을 신설했습니다.
- **Context Need Classifier 구현**:
  - [question_classifier.py](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py) 내부에 `ContextNeedClassifier`를 새로 정의하고, 질문의 유형이 트러블슈팅성 질문일 경우 `requires_current_game_state`를 `True`로 판단하고 필수 5대 스코프(`["selectedMachine", "inputInventory", "outputInventory", "powerStatus", "currentRecipe"]`)를 리턴하는 분류 규칙을 구현했습니다.
- **Current Game State Tool 모의 구현 및 서비스 통합**:
  - [service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py)에 `CurrentGameStateTool` 클래스를 mock 형태로 구현했습니다. 이 도구는 플레이어가 제공한 원시 게임 상태 context 딕셔너리에서 필요한 Scope 필드들만 발라내어 정형 텍스트로 직렬화하고 `available_scopes`를 검출합니다.
  - `ManualQAService.build_prompt_context` 메서드 파이프라인에서 `ContextNeedClassifier`와 `CurrentGameStateTool`을 순차 연동하여 수집한 상태를 `ManualQAPromptContext`와 `ManualQAResult`에 매핑하도록 개량했습니다.
- **Prompt State Injection 구현**:
  - [prompt_builder.py](file:///c:/factory-space/backend/src/agents/operator_guide/prompt_builder.py)에 `_current_game_state_section`을 추가해 RAG와 동일하게 게임 상태 정보 텍스트가 유효하게 존재하는 경우에만 `[CURRENT_GAME_STATE]` 섹션을 프롬프트에 동적 삽입하도록 연동했습니다.
- **단위 테스트 추가 및 린트 검증**:
  - [test_operator_guide_rag_sprint15.py](file:///c:/factory-space/backend/tests/test_operator_guide_rag_sprint15.py)를 신설하여 상태 판별 로직, 스코프 필터링, 성공 기준(정적 질문 "기어는 어떻게 만들어?" -> 미호출 / 문제 분석 "철괴가 안 만들어져. 왜 그래?" -> 호출 및 주입)을 철저히 검증하고 ruff 및 전체 백엔드 테스트(254개)를 전부 통과시켰습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
총 254개의 백엔드 전체 테스트 케이스가 성공적으로 통과하였습니다.
- `uv run pytest tests/test_operator_guide_rag_sprint15.py -v` 통과
- `uv run pytest -q` 전체 백엔드 테스트 suite 통과 (254 passed)
- `uv run ruff check` 전체 코드 포맷 및 린트 검사 통과

### 2.2. 테스트 결과 출력 전문
```text
tests/test_operator_guide_rag_sprint15.py::test_context_need_classifier_troubleshooting PASSED [ 20%]
tests/test_operator_guide_rag_sprint15.py::test_context_need_classifier_static_question PASSED [ 40%]
tests/test_operator_guide_rag_sprint15.py::test_game_state_tool_filtering PASSED [ 60%]
tests/test_operator_guide_rag_sprint15.py::test_sprint_15_success_criteria_gear PASSED [ 80%]
tests/test_operator_guide_rag_sprint15.py::test_sprint_15_success_criteria_iron_ingot_stopped PASSED [100%]

============================== 5 passed in 0.06s ==============================
```

---

## 3. 종합 평가

이번 Sprint 15 작업을 통하여 정적 매뉴얼(RAG) 지식에만 한정되어 있던 에이전트의 답변 엔진이 플레이어의 현재 기계 상황, 전력, 아이템 인벤토리 등 **실시간 게임 정황(Current Game State)**을 지능적으로 함께 반영하여 고장 원인을 짚어줄 수 있는 구조로 확장되었습니다.
`ContextNeedClassifier`를 통한 동적 상태 연동 판별로 정적 지식 질문의 불필요한 도구 호출을 차단하고, `CurrentGameStateTool`을 이용한 스코프별 데이터 필터링을 통해 컨텍스트 크기를 효율적으로 격리했습니다. 모든 단위 및 연동 시나리오 테스트가 안정적으로 통과했으므로 머지를 적극 승인합니다.
