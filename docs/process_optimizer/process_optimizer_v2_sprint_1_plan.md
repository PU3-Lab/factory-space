# Process Optimizer v2 Sprint 1 계획

## 목표

`process_optimizer` 전용 LangGraph의 최소 뼈대를 만든다.

이 sprint의 목적은 최적화 기능 전체를 완성하는 것이 아니라, 이후 sprint에서 기능을 붙일 수 있는 `graph_state.py`, `nodes.py`, `graph.py` 구조를 먼저 안정적으로 만드는 것이다.

## 구현 범위

```text
- ProcessOptimizerGraphState 정의
- 최소 graph node 함수 정의
- analyze 요청을 받아 기본 preview 응답을 만드는 graph 구성
- graph compile 함수 작성
- 기존 v1 공통 AgentPipeline 흐름은 유지
```

## 추가 파일

```text
backend/src/agents/process_optimizer/graph_state.py
backend/src/agents/process_optimizer/nodes.py
backend/src/agents/process_optimizer/graph.py
backend/tests/test_process_optimizer_graph.py
```

## Graph 흐름

```text
START
-> validate_graph_input
-> build_empty_preview
-> return_preview
-> END
```

## 구현 세부

### graph_state.py

```text
- envelope 또는 payload 원본
- operation
- factory_state
- factoryRevision
- goal
- previewPayload
- error
```

### nodes.py

```text
- validate_graph_input
- build_empty_preview
- return_preview
```

### graph.py

```text
- StateGraph(ProcessOptimizerGraphState) 생성
- node 등록
- edge 연결
- compile_process_optimizer_graph 함수 제공
```

## 성공 기준

```text
- graph.py import가 가능하다.
- analyze payload를 graph에 넣으면 status: "preview" 응답이 나온다.
- 기존 process_optimizer v1 테스트가 깨지지 않는다.
- 아직 apply, undo, measure는 구현하지 않는다.
```

## 테스트 계획

```text
uv run pytest backend/tests/test_process_optimizer_graph.py -q
uv run pytest backend/tests/test_process_optimizer*.py -q
```

## 완료 후 확인 질문

```text
- 전용 graph 파일이 생겼는가?
- graph가 독립적으로 compile 되는가?
- v1 AgentPipeline 동작을 깨지 않았는가?
- 다음 sprint에서 analyze -> preview 실제 로직을 붙일 준비가 되었는가?
```

