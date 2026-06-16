# operator_guide RAG Sprint 15 기획서 (Current Game State Tool & Context Need Classifier)

## 1. 개요
현재 operator_guide RAG 챗봇은 공장 매뉴얼(RAG)만을 근거로 정적인 질문에 답하고 있습니다. 그러나 플레이어가 "왜 기계가 안 돌지?", "철괴가 왜 안 만들어져?"와 같이 현재 게임 안의 문제(Troubleshooting)를 물어볼 때는, 플레이어가 바라보고 있는 기계의 전력 상태나 인벤토리 등 **현재 게임 상태(Current Game State)**를 함께 연동해야 정확한 고장 원인을 짚어낼 수 있습니다.

따라서 Sprint 15에서는 질문의 의도를 분석하여 게임 상태 정보가 필요한지 판단하는 **Context Need Classifier**와, 필요한 상태 정보를 선별 수집하는 **Current Game State Tool**을 설계하고 런타임에 연동합니다.

## 2. 목표
- **Context Need Classifier** 결과 구조 확정:
  - `requiresCurrentGameState`: bool
  - `requiredStateScopes`: list[str] (예: `["selectedMachine", "inputInventory", "outputInventory", "powerStatus", "currentRecipe"]`)
- **Current Game State Tool** 스키마 정의 및 Mock 구현:
  - 플레이어가 제공한 context 딕셔너리에서 필요한 Scope에 해당하는 데이터만 선별하여 `availableScopes`와 함께 챗봇 엔진에 전달합니다.
- **서비스 및 프롬프트 연동**:
  - `ManualQAService`와 `ManualQAPromptBuilder`에 게임 상태 정보 연동.
  - 응답 metadata에 `requiresCurrentGameState`, `usedCurrentGameState`, `requiredStateScopes`, `availableScopes` 정보를 투명하게 노출.
- **핵심 요구 검증**:
  - "기어는 어떻게 만들어?" -> 게임 상태 도구 미호출, RAG 단독 사용.
  - "철괴가 안 만들어져. 왜 그래?" -> 게임 상태 도구 호출, RAG + 상태 연동 사용.

---

## 3. 상세 기획 및 구현 명세

### 3.1. Context Need Classifier 결과 구조 ([schemas.py](file:///c:/factory-space/backend/src/agents/operator_guide/schemas.py))
- `ManualQAResult`와 `to_metadata()`에 다음 필드들을 확장합니다:
  - `requires_current_game_state`: bool
  - `used_current_game_state`: bool
  - `required_state_scopes`: list[str]
  - `available_scopes`: list[str]
- 챗봇 응답 시 최종 metadata 딕셔너리에는 Unreal API 규약에 따라 카멜케이스 키(`requiresCurrentGameState`, `usedCurrentGameState`, `requiredStateScopes`, `availableScopes`)로 변환되어 전달되도록 매핑합니다.

### 3.2. Context Need Classifier 구현 ([question_classifier.py](file:///c:/factory-space/backend/src/agents/operator_guide/question_classifier.py))
- 질문의 성격과 분류된 intent 정보를 바탕으로 게임 상태 정보의 필요성 유무를 판별하는 `ContextNeedClassifier`를 작성합니다:
  - 질문이 `troubleshooting_question` 계열이거나 문제 분석이 필요한 경우 `requiresCurrentGameState`를 `True`로 설정하고 필요한 Scope들(`["selectedMachine", "inputInventory", "outputInventory", "powerStatus", "currentRecipe"]`)을 활성화합니다.
  - 그 외의 제작법, 역할 설명 등 정적인 지식 질문에서는 `requiresCurrentGameState`를 `False`, 스코프를 빈 리스트 `[]`로 반환합니다.

### 3.3. Current Game State Tool 설계 및 연동 ([service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py) & [prompt_builder.py](file:///c:/factory-space/backend/src/agents/operator_guide/prompt_builder.py))
- `CurrentGameStateTool` 클래스를 mock 형태로 구현합니다.
- 플레이어가 전달한 `context` 딕셔너리에 `current_game_state` 구조가 들어있을 때, `requiredStateScopes`에 부합하는 필드들만 발라내어 LLM 프롬프트에 주입할 컨텍스트 텍스트를 빌드합니다.
- `ManualQAPromptBuilder`에 `[CURRENT_GAME_STATE]` 섹션을 신설하여, 제공받은 게임 상태 데이터를 LLM이 문맥 근거로 추론할 수 있게 전달합니다.
- `ManualQAPromptContext` 데이터 클래스에 `current_game_state_text` 및 관련 통계 필드들을 편입시킵니다.

---

## 4. 검증 계획

### 4.1. 유닛 테스트 작성 ([test_operator_guide_rag_sprint15.py](file:///c:/factory-space/backend/tests/test_operator_guide_rag_sprint15.py))
- `test_context_need_classifier_troubleshooting`: 문제 분석 질문 시 게임 상태 연동 필요(`requiresCurrentGameState=True`)로 판단하는지 검증.
- `test_context_need_classifier_static_question`: 일반 제작법 질문 시 RAG만 필요하고 상태 연동이 차단(`requiresCurrentGameState=False`)되는지 검증.
- `test_game_state_tool_filtering`: 플레이어 상태 context에서 필요한 scope만 잘 선별하고 `availableScopes`로 잡히는지 검증.
- `test_prompt_state_injection`: 빌드된 프롬프트에 `[CURRENT_GAME_STATE]` 섹션이 포함되고, 해당 섹션에 mock 장비 상태(예: 전력 부족, 인벤토리 누락)가 잘 전달되는지 검증.

### 4.2. 실행 및 린터 검증
```powershell
uv run pytest tests/test_operator_guide_rag_sprint15.py -v
uv run pytest -q
uv run ruff check
```
