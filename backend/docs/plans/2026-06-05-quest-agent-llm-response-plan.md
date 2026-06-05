# Quest Agent LLM Response Plan

## 언어 작성 원칙

- 퀘스트 `title`, `description`, `objective`처럼 플레이어에게 노출될 수 있는 문장은 기본적으로 한글로 작성한다.
- quest agent의 LLM prompt도 가능한 한 한글로 작성해서 모델이 한글 문장을 우선 반환하도록 유도한다.
- JSON key, agent id, item id, protocol field처럼 Unreal/backend 계약에 해당하는 값은 기존 영문 snake_case를 유지한다.
- fallback 문구와 LLM 응답 검증 에러도 Unreal에 노출될 수 있으므로 한글을 기본값으로 둔다.
- 기존 10개 퀘스트 선택지의 제목과 설명은 한글 원문을 유지하고, LLM은 이를 새로 만들거나 번역하지 않고 id만 선택한다.

## 프로토 범위 메모

이 문서는 현재 프로토 단계에서 "퀘스트 에이전트가 LLM 경로를 탄다"는 최소 변경을 추적하기 위한 문서다. 아래 후속 작업 후보는 그대로 구현하지 않는다. schema 검증 강화, fallback payload 통일, RAG/CSV 근거 주입은 별도 결정이 있을 때만 진행한다.

## 목표

퀘스트 에이전트가 빈 `payload` 요청에서 룰베이스드 fallback으로 바로 응답하지 않고, 기존 agent pipeline의 LLM 경로를 거쳐 퀘스트 응답을 생성하도록 한다.

기본 흐름은 다음과 같다.

1. top-level orchestrator가 `quest_generator`를 선택한다.
2. quest domain orchestrator가 leaf agent를 선택한다.
3. 선택된 leaf agent가 기존 quest option 10개를 보여주는 선택 prompt를 만든다.
4. LLM은 `selected_quest_ids` 5개만 고른다.
5. backend가 선택된 id를 기존 quest option payload로 변환한다.
6. LLM 응답이 없을 때만 deterministic fallback을 사용한다.

## 배경

기존 빈 퀘스트 요청은 다음과 같은 전용 우회 경로를 탔다.

- `agent == "quest_generator"`이고 `payload`가 비어 있으면 top-level routing을 건너뛰었다.
- quest sub-agent routing에서도 `quest_generator.production_quest`를 직접 선택했다.
- `skipLlm` 상태값으로 leaf prompt와 LLM 호출을 건너뛰고 deterministic fallback으로 바로 내려갔다.

이 경로는 안정적인 예제 퀘스트를 빠르게 반환한다는 장점이 있지만, 퀘스트 에이전트 응답이 LLM 기반 생성 경로를 타지 않는 문제가 있었다.

## 범위

이번 변경 범위:

- 빈 퀘스트 요청의 LLM 우회 제거
- `skipLlm` direct fallback 분기 제거
- production quest leaf prompt에 10개 option 중 5개 id 선택 계약 추가
- 테스트에서 LLM이 고른 id가 기존 quest option payload로 변환되는지 검증

이번 변경에서 제외한 범위:

- LLM 응답에 대한 `QuestResponse` Pydantic schema 검증 강화
- RAG 또는 CSV 기반 quest 근거 주입
- Unreal 클라이언트 프로토콜 변경
- deterministic fallback 제거

## 구현 체크리스트

- [x] `runtime.py`에서 빈 `quest_generator` 요청의 top-level routing 우회 제거
- [x] `runtime.py`에서 빈 quest payload의 production leaf 직접 선택 제거
- [x] `state.py`에서 `skipLlm` 상태 필드 제거
- [x] `graph_edges.py`에서 `direct -> agent.middleware.fallback` 분기 제거
- [x] `ProductionQuestAgent.build_prompt()`가 기존 10개 quest option 중 5개 id 선택 계약을 명시하도록 변경
- [x] `EconomyQuestAgent.build_prompt()`가 5개 economy quest JSON 계약을 명시하도록 변경
- [x] 빈 quest 요청 테스트가 top-level routing, leaf routing, LLM selection prompt를 모두 확인하도록 변경
- [x] quest pipeline 테스트가 fallback 결과가 아니라 LLM 선택 id 기반 quest payload를 확인하도록 변경
- [x] leaf agent prompt 테스트를 새 prompt 계약에 맞게 갱신
- [x] LLM 실패 시 기존 deterministic fallback 경로는 유지

## 검증 체크리스트

- [x] 빈 `agent: "quest_generator"` 요청에서 LLM prompt가 3회 호출된다.
- [x] `quest_generator.production_quest` leaf prompt가 생성된다.
- [x] LLM이 반환한 `{"selected_quest_ids":[...]}`가 기존 quest option payload로 변환된다.
- [x] response metadata에 `llm: "used"`가 포함된다.
- [x] LLM이 응답하지 않으면 기존 fallback quest 응답을 반환한다.
- [x] 기존 invalid sub-agent 검증은 유지된다.
- [x] backend 전체 pytest가 통과한다.
- [x] 수정 파일 대상 ruff check가 통과한다.

## 실행한 검증

```bash
cd backend
uv run --extra dev pytest -q
```

결과:

```text
144 passed
```

```bash
cd backend
uv run --extra dev ruff check src/agents/pipeline/runtime.py src/agents/pipeline/state.py src/agents/pipeline/graph_edges.py src/agents/quest_generator/production_quest.py src/agents/quest_generator/economy_quest.py tests/test_message_router.py tests/test_pipeline_edges.py tests/test_agent_leaf_behaviors.py
```

결과:

```text
All checks passed!
```

참고: 전체 `ruff check src tests`는 이번 변경과 무관한 `src/agents/operator_guide/csv_repository.py` import 정렬 문제로 실패했다.

## 후속 작업 후보

- [ ] LLM quest 응답을 `QuestResponse`로 검증하고 실패 시 fallback 또는 error로 처리한다.
- [ ] production/economy fallback payload shape을 `quests` 중심으로 통일한다.
- [ ] prompt에 현재 플레이어 진행도, 보유 자원, 해금된 장비 정보를 주입한다.
- [ ] CSV/RAG 기반 quest 근거를 prompt에 넣고 `metadata.sources`로 반환한다.
- [ ] smoke script에서 quest 응답이 LLM 기반인지 metadata로 확인하는 옵션을 추가한다.

## 성공 기준

- 빈 quest 요청이 더 이상 룰베이스드 direct path로 끝나지 않는다.
- 정상 모델 응답이 있으면 LLM이 고른 id를 기준으로 기존 quest option payload가 response payload가 된다.
- 모델 미응답 상황에서는 기존 fallback으로 서비스 안정성을 유지한다.
- 기존 WebSocket envelope는 유지된다. 정상 LLM 응답 경로는 `payload.quests`를 사용한다.
