# Quest Select Tool 전환 계획

## 목표

`parse_llm_response`에 박혀 있는 `quest_generator.production_quest` special-case 로직을
LangGraph tool 인프라로 이전한다.

현재 `runtime.py`의 `parse_llm_response`는 특정 leaf agent를 알아야만 하는 결합이 있다.
`generate_quest_json_from_ids` 호출을 tool로 분리하면 이 결합이 사라지고,
tool_node 인프라가 기존 방식대로 처리한다.

## 변경 전 흐름

```
build_prompt → LLM → {"selected_quest_ids": [1,2,3,4,5]}
→ parse_llm_response (special-case: production_quest 분기)
  → QuestAgentService().generate_quest_json_from_ids(ids)
  → responsePayload
```

## 변경 후 흐름

```
build_prompt → LLM → {"tool_call": {"name": "quest.select_by_ids", "args": {"ids": [1,2,3,4,5]}}}
→ prepare_tool_call → agent.tool_node
  → QuestSelectTool.invoke() → QuestAgentService().generate_quest_json_from_ids(ids)
→ build_tool_followup_prompt → call_llm.tool_followup
  → LLM → {"quests": [...]}
→ parse_llm_response (special-case 없음, 범용 JSON 파싱만)
  → responsePayload
```

## 패턴 참조

| 카테고리 | 참조 위치 | 패턴 |
|----------|-----------|------|
| Tool 등록 | `agents/agent_catalog.py:29` | `AgentTool` Protocol — `name`, `invoke(payload, context, args)` |
| Tool 이름 | `agents/agent_catalog.py:71` | dot notation (`agent_catalog.get_capabilities`) |
| Tool 반환 | `agents/pipeline/tool_node.py:201` | 에러 시 `{"status": "error", "code": ..., "message": ...}` |
| Agent tools 등록 | `agents/base.py:46` | `tools: tuple[AgentTool, ...]` |
| 테스트 스타일 | `tests/test_agent_leaf_behaviors.py:1` | pytest fixture, `AgentContext`, 직접 assert |

## 범위

**이번 변경:**
- `ProductionQuestAgent`에 `QuestSelectTool` 등록
- `build_prompt()`가 tool_call 형태로 ID 선택을 요청하도록 변경
- `parse_llm_response`의 `production_quest` special-case 분기 제거
- 테스트 갱신

**제외:**
- `EconomyQuestAgent` 변경 없음 (LLM 직접 생성 방식 유지)
- `QuestAgentService` 내부 로직 변경 없음
- fallback 경로 변경 없음

## 구현 체크리스트

### Step 1 — `QuestSelectTool` 작성 (`production_quest.py`)

- [ ] `QuestSelectTool` 클래스 추가
  - `name = "quest.select_by_ids"`
  - `invoke(payload, context, args)` — `args["ids"]` 를 받아 `QuestAgentService().generate_quest_json_from_ids(ids)` 호출
  - `ids` 검증 실패 시 `{"status": "error", "code": "INVALID_TOOL_ARGS", "message": ...}` 반환
- [ ] `ProductionQuestAgent.tools` 에 `QuestSelectTool()` 등록
- [ ] `build_prompt()` 변경
  - 6개 퀘스트 옵션 제공 유지
  - LLM 출력 계약을 `{"selected_quest_ids": [...]}` → `{"tool_call": {"name": "quest.select_by_ids", "args": {"ids": [...]}}}` 로 변경

### Step 2 — `parse_llm_response` special-case 제거 (`runtime.py`)

- [ ] `selectedLeafAgent == "quest_generator.production_quest"` 분기 전체 제거
- [ ] `from agents.quest_generator.service import QuestAgentService` import가 이 분기에만 쓰였다면 제거

### Step 3 — 테스트 갱신

- [ ] `test_agent_leaf_behaviors.py`
  - `ProductionQuestAgent.build_prompt()` 가 `tool_call` 계약을 포함하는지 확인
  - `QuestSelectTool.invoke()` 가 유효한 ids로 퀘스트 JSON을 반환하는지 확인
  - `QuestSelectTool.invoke()` 가 잘못된 ids로 error dict를 반환하는지 확인
- [ ] `test_quest_agent_service.py` 또는 `test_pipeline_edges.py`
  - production_quest 요청이 tool_call → tool_node → tool_followup 경로를 타는지 확인
  - tool 결과가 responsePayload로 변환되는지 확인
- [ ] `parse_llm_response` special-case 관련 기존 테스트 제거 또는 갱신

## 검증 명령

```bash
cd backend
uv run --extra dev pytest -q

uv run --extra dev ruff check \
  src/agents/quest_generator/production_quest.py \
  src/agents/pipeline/runtime.py \
  tests/test_agent_leaf_behaviors.py
```

## 리스크

| 리스크 | 가능성 | 대응 |
|--------|--------|------|
| LLM 호출 1회 증가 (tool followup) | 확실 | 허용 범위 — 계획 의도 |
| tool_followup LLM이 quests를 wrapping 없이 그대로 반환 | 보통 | prompt output contract 명시로 방지 |
| `parse_llm_response` 제거 후 다른 테스트 깨짐 | 낮음 | pytest 전체 통과로 검증 |

## 수용 기준

- [ ] `parse_llm_response`에 `quest_generator.production_quest` 분기 없음
- [ ] `ProductionQuestAgent.tools`에 `QuestSelectTool` 등록됨
- [ ] production_quest 요청이 tool_node 경로를 통해 퀘스트 JSON을 반환함
- [ ] LLM 미응답 시 기존 fallback 경로 유지
- [ ] pytest 전체 통과, ruff check 통과
