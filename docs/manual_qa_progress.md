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

- `payload.text`: 사용자에게 보여줄 답변
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

