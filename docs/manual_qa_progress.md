# Manual Q&A Agent 진행 기록

마지막 업데이트: 2026-05-28

## 현재 상태

Manual Q&A Agent 프로토는 기존 `operator_guide` agent 내부에 구현되어 있다.

현재는 프로토 완료 전 정리 단계로, 구현 구조와 실제 처리 흐름을 초보자도 이해할 수 있도록 문서화하고 있다. 처리 흐름 설명은 `docs/manual_qa_runtime_flow.md`에 따로 정리했으며, PostgreSQL과 pgvector를 사용하는 최종 아키텍처도 함께 추가했다.

외부 호출 구조는 그대로 유지한다.

```text
Unreal UI / Front
-> agent="operator_guide" 요청
-> MessageRouter
-> AgentOrchestrator
-> AgentRegistry에서 operator_guide 조회
-> QAChatbotAgent.process()
-> operator_guide 내부 Manual Q&A 로직 실행
```

아래 공통 라우팅 파일은 의도적으로 수정하지 않았다.

- `src/factory_space/core/agents/orchestrator.py`
- `src/factory_space/core/agents/registry.py`
- `src/factory_space/messages/router.py`

## 구현 구조

이번 프로토는 CSV 데이터를 직접 읽고 템플릿 기반 답변을 만든다. PostgreSQL, pgvector, Embedding, Markdown RAG, LLM Judge, 복잡한 `player_state` 분석은 사용하지 않는다.

실행 흐름은 다음과 같다.

```text
QAChatbotAgent.process()
-> QAChatbotPayload 검증
-> ManualQAService.answer()
-> ManualQAQuestionClassifier
-> CsvManualQARepository
-> ManualQAResponseBuilder
-> AgentResponsePayload
```

주요 파일 역할은 다음과 같다.

- `backend/src/agents/operator_guide/agent.py`: 기존 `operator_guide` agent 진입점. service 결과를 `AgentResponsePayload`로 변환한다.
- `backend/src/agents/operator_guide/service.py`: 질문 분류, CSV 조회, 답변 생성을 연결한다.
- `backend/src/agents/operator_guide/question_classifier.py`: 질문을 `equipment_question`, `resource_question`, `recipe_question`, `troubleshooting_question`, `unknown_question` 중 하나로 분류한다.
- `backend/src/agents/operator_guide/csv_repository.py`: 지정된 5개 CSV만 읽고, 장비/자원/레시피/문제해결/action policy 조회 함수를 제공한다.
- `backend/src/agents/operator_guide/repository.py`: 기존 import 경로를 위한 얇은 호환 export 파일이다.
- `backend/src/agents/operator_guide/response_builder.py`: CSV 조회 결과를 사용자 답변, source, 추천 행동 metadata로 만든다.
- `backend/src/agents/operator_guide/schemas.py`: `operator_guide` payload, intent, source, 추천 행동, 결과 schema를 정의한다.

## 사용하는 CSV

프로토에서 읽는 CSV는 아래 5개뿐이다.

- `data/game/equipment.csv`
- `data/game/resources.csv`
- `data/game/recipes.csv`
- `data/game/troubleshooting_rules.csv`
- `data/game/action_policy.csv`

`data/game` 아래에 백업용 CSV가 있더라도 읽지 않는다.

CSV 경로는 프로젝트 루트 기준으로 찾기 때문에, 테스트나 실행 위치가 바뀌어도 경로가 깨지지 않도록 했다.

## 응답 구조

프로토에서는 Unreal이 실제로 실행할 action과 추천 행동을 분리한다.

- `payload.final_answer`: 사용자에게 보여줄 답변
- `payload.actions`: 기본적으로 빈 배열 `[]`
- `payload.metadata.recommended_actions`: `action_policy.csv` 기반 추천 행동 목록
- `payload.metadata.sources`: 답변 근거로 사용한 CSV record 목록
- `payload.metadata.question_type`: 질문 분류 결과
- `payload.metadata.confidence`: `high`, `medium`, `low` 중 하나

이번 프로토에서는 새로운 Unreal 실행 action을 추가하지 않는다.

## 정리 작업 완료

`uv.lock` merge conflict는 `pyproject.toml` 기준으로 `uv lock`을 다시 실행해 해결했다. 현재 conflict marker는 남아 있지 않고, `uv run` 명령도 정상 실행된다.

Markdown 지식 문서의 issue ID는 현재 `data/game/troubleshooting_rules.csv` 기준으로 정규화했다.

- `issue_power_shortage` -> `issue_no_power`
- `issue_storage_full` -> `issue_output_full`

관련 Markdown front matter와 knowledge reference에 모두 반영했다. RAG evaluation question의 source ID도 `issue_power_shortage`에서 `issue_no_power`로 맞췄다.

`tests/test_websocket_endpoint.py`는 유지한다. `operator_guide`이 더 이상 stub이 아니므로 기존의 `metadata.status == "stub"` 검증은 현재 계약과 맞지 않는다. 지금 테스트는 unknown 질문에 대한 실제 프로토 계약인 `metadata.question_type == "unknown_question"`과 `payload.actions == []`를 확인한다.

## 수정 또는 추가된 파일

Manual Q&A runtime:

- `backend/src/agents/operator_guide/agent.py`
- `backend/src/agents/operator_guide/service.py`
- `backend/src/agents/operator_guide/schemas.py`
- `backend/src/agents/operator_guide/repository.py`
- `backend/src/agents/operator_guide/csv_repository.py`
- `backend/src/agents/operator_guide/question_classifier.py`
- `backend/src/agents/operator_guide/response_builder.py`

테스트:

- `tests/test_manual_qa_agent_smoke.py`
- `tests/test_manual_qa_knowledge_base.py`
- `tests/test_websocket_endpoint.py`

문서와 lock file:

- `docs/manual_qa_progress.md`
- `docs/manual_qa_runtime_flow.md`
- `uv.lock`
- `docs/knowledge/` 아래 issue ID 정합성 관련 문서
- `docs/knowledge/rag/evaluation_questions.json`

## 최종 테스트 결과

요청한 테스트 명령은 모두 `uv` 기준으로 실행된다.

```text
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
9 passed
```

```text
uv run --extra dev pytest tests/test_message_router.py tests/test_agent_contracts.py -v
7 passed
```

```text
uv run --extra dev pytest -v
27 passed
```

## 남은 이슈

현재 알려진 테스트 실패는 없다.

다음 항목은 의도적으로 이번 프로토 범위에서 제외했다.

- PostgreSQL repository
- pgvector
- Embedding
- Markdown RAG runtime
- LLM Judge
- 복잡한 `player_state` 분석
- 새 Global Orchestrator
- `manual_qa_orchestrator.py`
# 2026-06-01 구조 정리: operator_guide -> operator_guide

최신 `main`의 agent 구조와 `_workspace/factory-agent/compound.md`의 네이밍 결정에 맞춰 Manual Q&A 프로토를 `operator_guide` 패키지에서 `operator_guide` 내부 구현으로 정리했다.

이유:

- 최신 backend source root는 `backend/src`다.
- 매뉴얼 Q&A 도메인의 공식 이름은 `operator_guide`보다 `operator_guide`가 더 적절하다.
- `compound.md`에 `manual_qa` 계열 이름을 `operator_guide`로 정리한다는 결정이 남아 있다.
- 질문 유형 분류는 agent routing이 아니므로 `question_classifier.py` 대신 `question_classifier.py`라는 이름을 사용한다.

현재 Manual Q&A 프로토 위치:

```text
backend/src/agents/operator_guide/
```

현재 구성:

```text
agent.py                  # operator_guide 도메인 routing prompt
machine_help.py           # 설비 도움 leaf agent
recipe_explainer.py       # 레시피/자원 설명 leaf agent
troubleshooter.py         # 문제 해결 leaf agent
service.py                # Manual Q&A 질문 분류, 조회, 답변 생성 연결
question_classifier.py    # 질문 유형 분류
csv_repository.py         # 지정된 5개 CSV 직접 조회
repository.py             # repository import 호환 export
response_builder.py       # 템플릿 기반 답변, sources, recommended_actions 생성
schemas.py                # Manual Q&A 내부 schema
```

제거한 이전 패키지:

```text
backend/src/agents/operator_guide/
```

테스트와 디버그 스크립트도 `operator_guide` 기준 import를 사용하도록 정리했다.

검증 결과:

```text
cd backend
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
-> 10 passed

uv run --extra dev pytest tests/test_agent_contracts.py tests/test_message_router.py tests/test_agent_leaf_behaviors.py -v
-> 27 passed

uv run --extra dev pytest -v
-> 115 passed

uv run python scripts/manual_qa_debug.py
-> 대표 질문 5개의 text, actions, metadata 출력 확인
```

## 2026-06-01 출력 필드 정리: final_answer 추가

프로토, 알파, 베타, 최종 단계에서 공통으로 사용할 사용자 표시 답변 필드를 `final_answer`로 정했다.

현재 프로토에서는 `ManualQAResult.final_answer`가 `answer`와 같은 값을 반환한다.  
디버그 JSON 출력에도 `final_answer`를 추가했다.

의미:

```text
final_answer: UI가 사용자에게 보여줄 최종 답변
text: 기존 호환용 텍스트
answer: 내부 결과 모델의 답변 본문
actions: 실제 Unreal 실행 action. 프로토에서는 빈 배열
metadata.recommended_actions: 추천 확인 행동
```

검증 결과:

```text
cd backend
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
-> 10 passed

uv run --extra dev pytest -v
-> 115 passed

uv run python scripts/manual_qa_debug.py
-> 대표 질문 5개 출력에 final_answer 포함 확인
```

남은 주의점:

- 현재 로컬 `main`은 원격 `origin/main`보다 앞선 커밋과 뒤처진 커밋이 함께 있는 상태다. push 또는 추가 pull/merge 전에 Git 상태를 다시 확인해야 한다.

## 2026-06-03 LangGraph/operator_guide 경로 연결

Manual Q&A CSV 프로토를 실제 `operator_guide` leaf agent fallback 경로에 연결했다.

작업 계획과 구현 순서는 `docs/manual_qa_operator_guide_routing_plan.md`에 별도로 정리했다.
현재 구조도는 `docs/manual_qa_current_operator_guide_structure.md`에 따로 정리했다.
작업 중 발생한 문제와 해결 과정은 `docs/manual_qa_operator_guide_troubleshooting.md`에 정리했다.

이전 상태:

```text
테스트/디버그 스크립트
-> ManualQAService.answer()
-> CSV 조회
-> ManualQAResult 반환
```

이번 작업 후 상태:

```text
AgentPipeline / LangGraph
-> Orchestrator가 operator_guide 선택
-> operator_guide leaf agent 선택
   - operator_guide.machine_help
   - operator_guide.recipe_explainer
   - operator_guide.troubleshooter
-> leaf agent fallback
-> ManualQAService.answer()
-> 5개 CSV 조회
-> final_answer, actions, metadata 반환
```

수정한 파일:

- `backend/src/agents/operator_guide/service.py`
  - `build_manual_qa_agent_result()`를 추가해 `ManualQAService` 결과를 `AgentRunResult`로 변환한다.
- `backend/src/agents/operator_guide/machine_help.py`
  - 장비 도움 leaf fallback에서 CSV Manual Q&A 결과를 반환한다.
- `backend/src/agents/operator_guide/recipe_explainer.py`
  - 레시피 설명 leaf fallback에서 CSV Manual Q&A 결과를 반환한다.
- `backend/src/agents/operator_guide/troubleshooter.py`
  - 문제 해결 leaf fallback에서 CSV Manual Q&A 결과를 반환한다.
- `backend/tests/test_message_router.py`
  - LangGraph 경로에서 `operator_guide.machine_help`가 선택되었을 때 CSV 기반 `final_answer`, `sources`, `recommended_actions`가 응답에 포함되는지 확인한다.
- `backend/tests/test_agent_leaf_behaviors.py`
  - operator_guide leaf fallback이 Manual Q&A metadata를 포함하는 새 응답 구조를 검증하도록 갱신했다.

응답 구조:

```text
payload.final_answer: UI에 보여줄 최종 답변
payload.actions: 프로토에서는 빈 배열
payload.metadata.question_type: 질문 유형
payload.metadata.sources: CSV 기반 출처
payload.metadata.recommended_actions: 추천 행동
payload.metadata.selectedAgent: LangGraph가 선택한 상위 agent
payload.metadata.selectedLeafAgent: LangGraph가 선택한 leaf agent
```

검증 결과:

```text
cd backend
uv run --extra dev pytest tests/test_message_router.py::test_pipeline_operator_guide_fallback_returns_manual_qa_csv_answer -v
-> 1 passed

uv run --extra dev pytest tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks tests/test_message_router.py -v
-> 19 passed

uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
-> 10 passed

uv run --extra dev pytest -v
-> 140 passed
```

현재 남은 작업:

- 필요하면 WebSocket 실제 요청 예시에서도 `operator_guide` CSV 응답을 확인한다.

## 2026-06-03 최종 Manual Q&A Agent 흐름 문서화

프로토와 최종 버전의 차이를 초보자도 이해할 수 있도록 `docs/manual_qa_agent_final_ai_explanation.md`를 다시 정리했다.

정리한 내용:

- 프로토 단계의 실제 흐름
  - `AgentPipeline / LangGraph`
  - `Orchestrator`
  - `operator_guide`
  - leaf agent
  - `ManualQAService`
  - 5개 CSV 직접 조회
  - 템플릿 기반 `final_answer` 반환
- 최종 단계의 목표 흐름
  - 질문 + `player_state`
  - PostgreSQL 구조화 데이터 조회
  - embedding 생성
  - pgvector 기반 Markdown 매뉴얼 검색
  - LLM 근거 기반 답변 생성
  - `diagnosis`, `sources`, `recommended_actions` 반환
- 프로토에서 알파, 베타, 최종으로 갈 때의 고도화 계획

단계별 핵심 목표:

```text
프로토: CSV 직접 조회로 기본 답변 흐름 검증
알파: CSV Repository를 PostgreSQL Repository로 교체
베타: Markdown 매뉴얼 chunk와 pgvector RAG 연결
최종: player_state와 LLM을 결합해 상황 맞춤 답변 생성
```

최종 성공 기준:

```text
플레이어가 질문했을 때 현재 상황을 읽고, 그 질문에 맞게 정확히 답해주는 것.
```

## 2026-06-03 최종 operator_guide LangGraph 구조 문서화

최종 `operator_guide` Agent를 LangChain/LangGraph 관점에서 설명하는 문서를 추가했다.

추가 문서:

- `docs/operator_guide_final_langgraph_architecture.md`

정리한 내용:

- Top-level Orchestrator Router가 4개 Agent 중 `operator_guide`를 선택하는 흐름
- `operator_guide` 내부 Question Type Router 구조
- 최종 단계에서 사용할 수 있는 tools
  - `equipment_lookup_tool`
  - `resource_lookup_tool`
  - `recipe_lookup_tool`
  - `troubleshooting_lookup_tool`
  - `action_policy_tool`
  - `player_state_analyzer_tool`
  - `manual_rag_search_tool`
- 최종 단계에서 들어갈 수 있는 middleware
  - Input Normalization Middleware
  - Retrieval Filter Middleware
  - Response Validation Middleware
- 플레이어가 "제련기가 왜 안 돌아가?"라고 질문했을 때의 최종 처리 시나리오
- 프로토와 최종의 차이

추가 보정:

- 최종 구조를 `User Question + Player State`가 항상 들어오는 방식에서 `User Question`을 먼저 판단하는 방식으로 수정했다.
- `Question Type Router` 뒤에 `State Requirement Router`를 추가했다.
- 상태가 필요한 질문에서만 State Tools를 호출하도록 정리했다.
- 설명형 질문은 `no_state_needed`로 처리하고, 문제 해결 질문만 선택 장비, 재고, 전력, 생산 라인 상태를 필요한 만큼 조회하도록 문서화했다.

## 2026-06-04 CSV 질문/답변 예시 정합성 보정

`docs/manual_qa_csv_question_answer_examples.md`의 문제 해결 예시 중 `제련기가 왜 안 돌아가?` 답변을 현재 프로토 런타임 응답과 맞췄다.

확인 기준:

```text
cd backend
uv run python scripts/manual_qa_debug.py
```

현재 실제 `final_answer`:

```text
제련기가 멈췄다면 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인합니다.
확인 순서는 check_power, check_input, check_output, check_conveyor, check_recipe입니다.
```

정리:

- 짧은 문서 예시는 과거 요약 답변이었다.
- 현재 기준으로는 `scripts/manual_qa_debug.py`에서 나오는 `final_answer`가 실제 플레이어 화면에 표시될 값이다.
- 문서 예시도 이 값과 일치하도록 수정했다.

## 2026-06-04 문제 해결 답변 방향 결정

프로토의 troubleshooting 답변은 기존 체크리스트형 구조를 유지하고, 존댓말 톤만 보정하기로 결정했다.

이유:

- 프로토의 목표는 CSV 기반 기본 답변 흐름 검증이다.
- 실제 `player_state`를 보지 않는 상태에서 원인을 넓게 확장하면 답변 범위가 커질 수 있다.
- 원인 후보 우선 안내와 상태 기반 진단은 최종 단계에서 `player_state` tool이 붙은 뒤 고도화한다.

수정한 파일:

- `backend/src/agents/operator_guide/response_builder.py`
- `backend/tests/test_manual_qa_agent_smoke.py`
- `docs/manual_qa_csv_question_answer_examples.md`

현재 실제 `final_answer`:

```text
제련기가 멈췄다면 전력 상태, 입력 자원, 출력 저장 공간, 컨베이어 연결, 레시피 설정을 순서대로 확인합니다.
확인 순서는 check_power, check_input, check_output, check_conveyor, check_recipe입니다.
```

검증 결과:

```text
cd backend
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
-> 12 passed

uv run python scripts/manual_qa_debug.py
-> 대표 질문 5개 출력 확인, "제련기가 왜 안 돌아가?" final_answer가 존댓말 체크리스트 문장으로 출력됨
```

## 2026-06-04 답변 존댓말 톤 통일

플레이어 화면에 표시되는 `final_answer`와 추천 행동 설명이 `한다` 평서체로 나오지 않도록 `합니다/입니다` 톤으로 통일했다.

수정 내용:

- 자원 생산 답변의 `제련한다`, `가공한다` 문장을 `제련합니다`, `가공합니다`로 변환한다.
- 문제 해결 답변의 `확인한다` 문장을 `확인합니다`로 변환한다.
- 추천 행동 metadata의 `description`도 `확인합니다`, `설명합니다` 톤으로 변환한다.
- smoke test에 `final_answer`와 추천 행동 설명에 `한다`가 남지 않는지 확인하는 검증을 추가했다.

## 2026-06-04 문제 해결 답변 사용자 친화 보정

플레이어 피드백을 반영해 troubleshooting `final_answer`에서는 내부 확인 단계 ID를 직접 보여주지 않도록 변경했다.

변경 이유:

- `check_power`, `check_input` 같은 내부 ID는 개발자와 평가용 metadata에는 유용하지만, 플레이어 화면에서는 이해하기 어렵다.
- 프로토도 단순 FAQ처럼 보이지 않도록 실제 게임 오브젝트 이름을 포함해 안내한다.
- 실제 상태 진단은 최종 단계에서 `player_state` tool과 LLM을 붙인 뒤 고도화하되, 프로토에서는 CSV 지식 안에서 플레이어가 바로 따라 할 수 있는 문장으로 안내한다.

현재 실제 `final_answer` 방향:

```text
제련기가 멈췄다면 먼저 전력이 제대로 들어오는지 확인해보세요.
전력이 괜찮다면 철광석, 구리광석 같은 입력 자원이 제련기 안으로 들어오고 있는지 살펴보고,
만들어진 철괴, 구리괴 같은 출력 자원이 컨베이어나 저장고로 빠져나갈 수 있는지도 확인하면 됩니다.
마지막으로 제련기에 올바른 레시피가 선택되어 있는지 확인하세요.
```

정리:

- 플레이어 화면에는 `final_answer`만 표시한다.
- `check_*` 또는 `action_*` 같은 내부 ID는 `metadata.recommended_actions`에만 둔다.
- 테스트는 `check_power`가 `final_answer`에 노출되지 않는지 확인한다.

## 2026-06-04 대화형 답변 톤 적용

플레이어 피드백을 추가로 반영해 Manual Q&A 프로토의 `final_answer`를 단순 FAQ 문장보다 대화형 안내에 가깝게 바꿨다.

변경 방향:

- 정보형 질문은 `좋아요.`로 질문을 받아주고 핵심 설명 뒤에 바로 확인할 행동을 붙인다.
- 레시피 질문은 필요한 재료와 장비를 말한 뒤 `확인해볼까요?`처럼 다음 행동을 유도한다.
- 문제 해결 질문은 `멈췄군요.`처럼 상황을 받아주고, 가능한 원인과 확인 순서를 플레이어 언어로 안내한다.
- unknown 질문은 단순히 모른다고 끝내지 않고, 어떤 질문을 도와줄 수 있는지 예시를 제시한다.
- `check_*`, `action_*` 같은 내부 ID는 여전히 `metadata`에만 둔다.

예시:

```text
Q. 기어 만들려면 뭐가 필요해?
A. 좋아요. 기어를 만들려면 철괴 2개가 필요합니다.
   조립기에서 제작하고, 흐름은 철광석 채굴 > 철괴 제련 > 조립기에서 기어 제작 순서로 보면 됩니다.
   먼저 필요한 재료가 장비까지 들어오는지 확인해볼까요?
```

검증 결과:

```text
cd backend
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
-> 13 passed

uv run --extra dev pytest tests/test_message_router.py::test_pipeline_operator_guide_fallback_returns_manual_qa_csv_answer -v
-> 1 passed

uv run --extra dev pytest tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks -v
-> 3 passed
```

## 2026-06-04 프로토 요청 ID 운영 문서화

`docs/manual_qa_csv_question_answer_examples.md`에 프로토 단계의 request envelope 기준을 추가했다.

정리한 기준:

```text
request_id
-> 프로토에서는 생략 가능
-> 서버가 자동 생성한다

session_id
-> 프로토에서는 필수 사용하지 않는다
-> 생략하면 응답에서도 null이다

client_id
-> Unreal UI / Front가 보내는 값이다
-> 프로토에서는 unreal-ui-001 같은 고정값으로 충분하다
```

프로토 화면 표시 기준도 함께 명시했다.

```text
payload.final_answer: Unreal UI가 플레이어 화면에 표시할 공식 답변
```

## 2026-06-04 agent.response 답변 필드 단일화

Unreal UI / Front로 나가는 `agent.response.payload`에서는 플레이어에게 보여줄 답변 본문을 `final_answer` 하나로 통일했다.

변경 이유:

- `final_answer`, `text`, `answer`에 같은 문장이 반복되면 응답 구조가 불필요하게 복잡해진다.
- 프로토와 최종 모두 화면 표시 기준은 `payload.final_answer`로 가져가는 것이 명확하다.
- `answer`는 `ManualQAResult` 내부 모델에서만 유지하고, 외부 agent 응답에는 노출하지 않는다.

현재 외부 응답 기준:

```text
payload.final_answer: 플레이어 화면에 표시할 공식 답변
payload.actions: 프로토에서는 빈 배열
metadata: sources, recommended_actions, question_type 등 추적용 정보
```

`payload.text`와 `payload.answer`는 현재 `operator_guide` agent 응답에서 제외한다.

