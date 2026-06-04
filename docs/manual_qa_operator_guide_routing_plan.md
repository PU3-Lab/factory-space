# Manual Q&A operator_guide 라우팅 연결 플랜

## 목적

이 문서는 Manual Q&A CSV 프로토를 실제 `operator_guide` Agent 실행 경로에 연결하기 위해 어떤 계획으로 작업했는지 정리한다.

기존 프로토는 `ManualQAService.answer()`를 테스트나 디버그 스크립트에서 직접 호출해 CSV 기반 답변이 가능한지만 검증했다.

이번 작업의 목표는 다음과 같다.

```text
플레이어 질문
-> AgentPipeline / LangGraph
-> Orchestrator가 operator_guide 선택
-> operator_guide leaf agent 선택
-> ManualQAService가 CSV 기반 답변 생성
-> 실제 agent.response payload로 반환
```

즉, 단순 서비스 테스트를 넘어서 실제 Agent 실행 흐름에서도 Manual Q&A 답변이 나오게 만드는 것이 목적이다.

## 작업 전 상태

Manual Q&A CSV 프로토는 이미 아래 파일들로 구현되어 있었다.

```text
backend/src/agents/operator_guide/service.py
backend/src/agents/operator_guide/question_classifier.py
backend/src/agents/operator_guide/csv_repository.py
backend/src/agents/operator_guide/response_builder.py
backend/src/agents/operator_guide/schemas.py
```

하지만 실제 LangGraph 경로에서는 아직 이 서비스가 직접 사용되지 않았다.

기존 테스트 흐름:

```text
tests/test_manual_qa_agent_smoke.py
-> ManualQAService().answer(question)
-> CSV 조회
-> ManualQAResult 검증
```

기존 LangGraph 흐름:

```text
AgentPipeline
-> operator_guide 선택
-> operator_guide.machine_help / recipe_explainer / troubleshooter 선택
-> 각 leaf agent의 fallback 응답 반환
```

이때 leaf agent fallback은 고정된 안내 문장만 반환하고 있었다.

## 설계 방향

새로운 Global Orchestrator를 만들지 않는다.

기존 LangGraph / AgentPipeline 구조를 유지한다.

```text
AgentPipeline
-> OrchestratorAgent
-> OperatorGuideAgent
-> operator_guide leaf agent
```

Manual Q&A CSV 로직은 `operator_guide` 내부에서만 사용한다.

따라서 가장 작은 연결 지점은 각 leaf agent의 `fallback()`이다.

선택한 구조:

```text
operator_guide.machine_help.fallback()
operator_guide.recipe_explainer.fallback()
operator_guide.troubleshooter.fallback()
-> build_manual_qa_agent_result()
-> ManualQAService.answer()
```

이 구조를 선택한 이유:

- 기존 LangGraph 라우팅 구조를 바꾸지 않는다.
- `operator_guide` 내부 책임으로만 작업 범위를 제한할 수 있다.
- LLM 응답이 없을 때도 CSV 기반 deterministic fallback을 제공할 수 있다.
- 프로토 단계에서 실제 WebSocket/Agent 응답 구조에 가까운 결과를 검증할 수 있다.

## 구현 순서

### 1. 실패하는 테스트 추가

먼저 LangGraph 경로에서 `operator_guide`가 선택되었을 때 CSV 답변이 나오는지 검증하는 테스트를 추가했다.

추가 위치:

```text
backend/tests/test_message_router.py
```

추가한 테스트:

```text
test_pipeline_operator_guide_fallback_returns_manual_qa_csv_answer
```

테스트 시나리오:

```text
질문: 제련기는 뭐야?
top-level routing 결과: operator_guide
leaf routing 결과: operator_guide.machine_help
LLM 답변: 없음
기대 결과: ManualQAService가 CSV에서 제련기 정보를 찾아 final_answer 반환
```

처음 테스트는 실패했다.

실패 이유:

```text
response["payload"]["final_answer"]가 없음
```

즉, 기존 leaf fallback은 아직 Manual Q&A 결과를 반환하지 않고 있었다.

### 2. ManualQAService 결과를 AgentRunResult로 변환하는 함수 추가

추가 위치:

```text
backend/src/agents/operator_guide/service.py
```

추가한 함수:

```text
build_manual_qa_agent_result()
```

역할:

```text
payload에서 question 추출
-> ManualQAService.answer(question) 호출
-> ManualQAResult를 AgentRunResult로 변환
```

반환 payload 구조:

```text
answer
final_answer
text
actions
question
topic
```

반환 metadata 구조:

```text
question
question_type
sources
recommended_actions
confidence
primary_manual
supporting_manuals
target_ids
fallback
sub_agent
```

중요한 원칙:

```text
payload.actions는 빈 배열로 유지한다.
추천 행동은 metadata.recommended_actions에 넣는다.
```

### 3. operator_guide leaf agent fallback 연결

수정한 파일:

```text
backend/src/agents/operator_guide/machine_help.py
backend/src/agents/operator_guide/recipe_explainer.py
backend/src/agents/operator_guide/troubleshooter.py
```

변경 전:

```text
각 leaf agent fallback이 고정 안내 문장을 반환
```

변경 후:

```text
각 leaf agent fallback이 build_manual_qa_agent_result() 호출
```

연결 방식:

```text
machine_help -> topic="machine"
recipe_explainer -> topic="recipe"
troubleshooter -> topic="troubleshooting"
```

### 4. 기존 leaf fallback 테스트 갱신

수정한 파일:

```text
backend/tests/test_agent_leaf_behaviors.py
```

기존 테스트는 fallback metadata가 아래처럼 단순한지만 확인했다.

```text
{"fallback": True, "sub_agent": "..."}
```

변경 후에는 Manual Q&A metadata가 추가되므로 테스트를 갱신했다.

확인 항목:

```text
payload.actions == []
payload.final_answer == payload.text
metadata.fallback == True
metadata.sub_agent == leaf agent id
metadata.question_type 존재
metadata.sources 존재
metadata.recommended_actions 존재
```

## 최종 실행 흐름

현재 프로토에서 실제 흐름은 다음과 같다.

```text
1. Front / Unreal이 agent.request 전송
2. AgentPipeline이 요청 수신
3. OrchestratorAgent가 operator_guide 선택
4. OperatorGuideAgent가 leaf agent 선택
5. 선택된 leaf agent fallback 실행
6. fallback이 ManualQAService.answer() 호출
7. ManualQAService가 질문 유형 분류
8. CsvManualQARepository가 5개 CSV 조회
9. ManualQAResponseBuilder가 템플릿 답변 생성
10. AgentPipeline이 agent.response로 감싸서 반환
```

## 대표 예시

입력:

```json
{
  "type": "agent.request",
  "request_id": "request-operator-guide-manual-qa",
  "agent": "operator_guide",
  "payload": {
    "question": "제련기는 뭐야?"
  }
}
```

라우팅:

```text
selectedAgent: operator_guide
selectedLeafAgent: operator_guide.machine_help
```

출력 핵심:

```json
{
  "final_answer": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다. 입력 자원은 철광석, 구리광석이고, 출력 자원은 철괴, 구리괴입니다. 필요 전력은 10입니다.",
  "text": "제련기는 광석을 금속 자원으로 변환하는 생산 장비입니다. 입력 자원은 철광석, 구리광석이고, 출력 자원은 철괴, 구리괴입니다. 필요 전력은 10입니다.",
  "actions": [],
  "metadata": {
    "question_type": "equipment_question",
    "sources": [
      {
        "doc_id": "equipment_smelter",
        "type": "equipment",
        "title": "제련기"
      }
    ],
    "recommended_actions": [
      {
        "action_id": "action_explain_equipment_role",
        "label": "장비 역할 설명"
      }
    ],
    "selectedAgent": "operator_guide",
    "selectedLeafAgent": "operator_guide.machine_help"
  }
}
```

## 테스트 계획

### 단위/스모크 테스트

Manual Q&A CSV 엔진 자체 검증:

```bash
cd backend
uv run --extra dev pytest tests/test_manual_qa_agent_smoke.py -v
```

기대 결과:

```text
10 passed
```

### LangGraph 경로 테스트

operator_guide 라우팅 후 CSV 답변 반환 검증:

```bash
cd backend
uv run --extra dev pytest tests/test_message_router.py::test_pipeline_operator_guide_fallback_returns_manual_qa_csv_answer -v
```

기대 결과:

```text
1 passed
```

### 관련 회귀 테스트

operator_guide leaf fallback과 message router 전체 검증:

```bash
cd backend
uv run --extra dev pytest tests/test_agent_leaf_behaviors.py::test_operator_guide_leaf_agents_return_normalized_fallbacks tests/test_message_router.py -v
```

기대 결과:

```text
19 passed
```

### 전체 테스트

```bash
cd backend
uv run --extra dev pytest -v
```

기대 결과:

```text
140 passed
```

## 완료 기준

이번 연결 작업의 완료 기준은 다음과 같다.

- `operator_guide`가 선택된 LangGraph 경로에서 ManualQAService가 호출된다.
- CSV 기반 답변이 `payload.final_answer`와 `payload.text`로 반환된다.
- 실제 Unreal 실행 action은 만들지 않고 `payload.actions`는 빈 배열이다.
- 추천 행동은 `metadata.recommended_actions`에 들어간다.
- 답변 근거는 `metadata.sources`에 들어간다.
- `selectedAgent`, `selectedLeafAgent`가 metadata에 남는다.
- Manual Q&A smoke test가 통과한다.
- message router 경로 테스트가 통과한다.
- 전체 백엔드 테스트가 통과한다.

## 남은 확장 방향

프로토 이후에는 다음 단계로 확장할 수 있다.

```text
CSV Repository
-> PostgreSQL Repository
```

```text
템플릿 답변
-> PostgreSQL 구조화 조회 + pgvector 검색 + LLM 근거 기반 답변
```

```text
단순 질문
-> question + player_state 기반 상황 진단
```

최종 목표는 플레이어가 질문했을 때 현재 플레이 상황까지 함께 보고, 가장 가능성 높은 원인과 먼저 확인할 행동을 짧고 정확하게 알려주는 것이다.
