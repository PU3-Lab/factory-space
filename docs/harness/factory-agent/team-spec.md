# Factory Agent Superpowers Harness

이 문서는 Factory Space backend agent 개발을 Superpowers 방식으로 진행하기 위한 repo-local harness 계약이다.

## 목적

backend agent 작업은 다음 순서를 따른다.

1. 요청을 작은 작업 단위로 나눈다.
2. 각 작업은 실패하는 테스트 또는 명확한 문서 검증 기준으로 시작한다.
3. 구현 후 spec compliance review를 진행한다.
4. spec 통과 후 code quality review를 진행한다.
5. 전체 테스트와 lint를 fresh run으로 검증한 뒤 완료 또는 커밋한다.

## 선택한 Harness 패턴

패턴: Pipeline + Producer-Reviewer

이유:

- agent pipeline 구현은 protocol, routing, LangGraph state, WebSocket gateway가 순서대로 맞물린다.
- 각 작업은 구현 산출물이 먼저 필요하고, 그 뒤 spec review와 code quality review가 가능하다.
- 사용자 요구사항 중 "에이전트 쓰는 곳은 로직을 쓰지마", "답변은 문서에 기록", "테스트 코드" 같은 품질 기준은 별도 review gate가 있어야 누락을 줄일 수 있다.

## 역할

### Coordinator

사용 스킬: `.agents/skills/factory-agent-superpowers/SKILL.md`

책임:

- 사용자 요청을 작업 단위로 나눈다.
- `_workspace/factory-agent/` 아래에 작업 산출물 이름을 정한다.
- implementer, spec reviewer, quality reviewer 순서를 강제한다.
- 완료 전 fresh verification을 실행한다.

### Implementer

사용 스킬: `.agents/skills/factory-agent-implementer/SKILL.md`

책임:

- 테스트 또는 문서 검증 기준을 먼저 만든다.
- 최소 구현으로 테스트를 통과시킨다.
- 기존 사용자 변경을 되돌리지 않는다.
- 작업 결과를 `_workspace/factory-agent/{task_id}_implementation.md`에 기록한다.

### Spec Reviewer

사용 스킬: `.agents/skills/factory-agent-spec-reviewer/SKILL.md`

책임:

- 원 요청, 결정 로그, 구현 diff를 비교한다.
- spec 미충족, 과구현, 문서 누락을 찾는다.
- 결과를 `_workspace/factory-agent/{task_id}_spec_review.md`에 기록한다.

### Quality Reviewer

사용 스킬: `.agents/skills/factory-agent-quality-reviewer/SKILL.md`

책임:

- spec review 통과 후에만 실행한다.
- 코드 구조, 테스트 신뢰도, protocol 경계, prompt routing 원칙을 검토한다.
- 결과를 `_workspace/factory-agent/{task_id}_quality_review.md`에 기록한다.

## Handoff Artifacts

작업별 파일 이름은 다음을 사용한다.

- `_workspace/factory-agent/{task_id}_request.md`
- `_workspace/factory-agent/{task_id}_red.md`
- `_workspace/factory-agent/{task_id}_implementation.md`
- `_workspace/factory-agent/{task_id}_spec_review.md`
- `_workspace/factory-agent/{task_id}_quality_review.md`
- `_workspace/factory-agent/{task_id}_verification.md`

`task_id`는 날짜나 임의 id가 아니라 작업 의미를 드러내는 짧은 kebab-case를 사용한다.

예:

- `prompt-routing-validation`
- `websocket-envelope-errors`
- `quest-sub-agent-routing`

## Required Checks

backend 작업의 기본 검증 명령:

```bash
uv run --extra dev pytest
uv run --extra dev ruff check .
```

명령은 `backend/`에서 실행한다.

## Factory Agent Rules

- runtime package 구조는 `backend/src` 기준이다.
- `backend/src/factory_space` 아래에 새 구현을 만들지 않는다.
- `agents/orchestrator.py`는 top-level agent만 prompt로 선택한다.
- `manual_qa/agent.py`와 `quest_generator/agent.py`는 domain sub-orchestrator다.
- agent 또는 sub-agent 추론은 keyword, if/else, score table로 구현하지 않는다.
- 명시된 `agent` 또는 `sub_agent`는 검증 후 사용한다.
- 잘못된 명시 값은 LLM routing으로 우회하지 않고 error로 종료한다.
- `pipeline.py`는 orchestrator agent가 아니라 LangGraph 실행 파이프라인이다.
- WebSocket gateway는 transport만 담당한다.
- 질문과 결정은 `backend/src/DECISION_LOG.md` 또는 관련 harness 문서에 기록한다.

## Review Gate

각 작업은 다음 조건을 모두 만족해야 완료할 수 있다.

1. red artifact가 있거나, 문서 전용 작업이면 검증 기준이 명시되어 있다.
2. 구현 artifact가 변경 파일과 의도를 설명한다.
3. spec review가 `pass`다.
4. quality review가 `pass`다.
5. fresh verification command 결과가 기록되어 있다.

## Failure Policy

- spec review가 `fix`이면 quality review로 넘어가지 않는다.
- quality review가 `fix`이면 같은 task 안에서 수정 후 quality review를 다시 수행한다.
- routing이 애매하면 prompt를 수정하거나 explicit input contract를 바꾼다. 코드 fallback으로 숨기지 않는다.
- 테스트 없이 behavior를 변경한 경우 해당 변경은 미완료로 본다.
