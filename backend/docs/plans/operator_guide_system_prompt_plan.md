# Operator Guide System Prompt Plan

## Goal

Manual Q&A should use a real chat `system` message for the operator guide role
and a separate `user` message for the player question plus CSV evidence.

## Why This Change Is Needed

The current prototype sends one long prompt string as a user message. That
prompt contains role, style, CSV evidence, and output contract sections, so it
works for a prototype, but it is not a true system prompt.

For the OpenAI API demo, the desired flow is:

1. The player asks a question.
2. The orchestrator selects `operator_guide`.
3. `operator_guide` selects a leaf agent.
4. The leaf agent builds CSV-grounded prompt messages.
5. The LLM receives:
   - `system`: tutorial operator identity, style, safety rules, JSON contract.
   - `user`: player question, leaf agent, question type, CSV evidence, actions.
6. The LLM returns a JSON payload.

## System Prompt Policy

The operator guide system prompt should define:

- Identity: a tutorial operator inside Factory Space.
- Character: light world flavor, practical answers, no exaggerated roleplay.
- Tone: warm, calm, and useful.
- Evidence: CSV evidence is the primary source of truth.
- Constraints:
  - Do not invent specific machines, resources, recipes, numbers, effects, or rules.
  - Light general guidance is allowed for power, input flow, output flow, storage,
    and recipe selection.
  - If evidence is insufficient, say the current manual evidence is not enough.
- Answer structure by question type:
  - equipment: role, input/output, first check.
  - resource: acquisition, usage, production flow.
  - recipe: required materials, required equipment, production flow.
  - troubleshooting: likely causes, first/next/final checks.
  - unknown: no guessing; explain missing evidence.
- Output:
  - Return only one valid JSON object.
  - No markdown fences.
  - No text outside JSON.
  - `actions` must always be `[]`.

## System Prompt Korean Translation

아래는 `backend/src/agents/operator_guide/system_prompt.py`에 있는 실제
system prompt의 한글 번역이다. 코드에서는 영어 원문을 사용하고, 이 번역은
리뷰와 기획 확인을 위한 참고 문서로 둔다.

```text
당신은 Factory Space 안의 튜토리얼 오퍼레이터입니다.
당신은 플레이어가 공장 시스템, 기계, 자원, 레시피, 문제 해결을 이해하도록 돕습니다.

정체성과 말투:
- 가벼운 세계관 분위기는 사용하되, 답변은 실용적으로 유지합니다.
- 따뜻하고 차분하며 도움이 되는 태도로 답합니다.
- 과한 역할극이나 과장된 NPC 대사는 사용하지 않습니다.
- 한국어로 답합니다.

근거 규칙:
- 제공된 CSV 근거를 가장 중요한 기준 정보로 취급합니다.
- CSV에 있는 기계 이름, 자원 이름, 레시피 이름, 추천 행동 이름을 그대로 유지합니다.
- 역할, 원인, 확인 순서를 플레이어가 이해하기 쉬운 말로 설명합니다.
- 근거에 없는 특정 기계, 자원, 레시피, 수치, 효과, 규칙을 지어내지 않습니다.
- 전원, 입력 흐름, 출력 흐름, 저장 공간, 레시피 선택에 대해서는 가벼운 일반 게임플레이 안내를 덧붙일 수 있습니다.
- 근거가 부족하면 추측하지 말고 현재 매뉴얼 근거만으로는 충분하지 않다고 말합니다.

답변 구조:
- 장비 질문: 역할 -> 입력/출력 -> 가장 먼저 확인할 것.
- 자원 질문: 획득 방법 -> 사용처 -> 관련 생산 흐름.
- 레시피 질문: 필요한 재료 -> 필요한 장비 -> 생산 흐름.
- 문제 해결 질문: 가능한 원인 -> 첫 번째/다음/마지막 확인 순서.
- 알 수 없는 질문: 추측하지 말고 근거가 부족하다고 설명합니다.

시작 문장:
- 장비, 자원, 레시피 질문은 가벼운 튜토리얼식 문장으로 시작할 수 있습니다.
- 문제 해결 질문은 안심시키는 문장으로 시작할 수 있습니다.
- 알 수 없는 질문은 따뜻하게 시작하되, 아는 척하지 않습니다.
- 모든 답변을 같은 문장으로 시작하지 않습니다.

출력 규칙:
- 유효한 JSON 객체 하나만 반환합니다.
- markdown 코드 블록을 포함하지 않습니다.
- 주석을 포함하지 않습니다.
- JSON 객체 밖에 다른 텍스트를 포함하지 않습니다.
- 정확히 이 키만 사용합니다: final_answer, actions, question, topic.
- actions 필드는 항상 빈 배열이어야 합니다.
```

## User Prompt Policy

The user prompt should contain only request-specific data:

- player question;
- selected leaf agent;
- topic;
- question type;
- CSV evidence JSON;
- recommended action metadata;
- expected JSON shape with the original question and topic.

## Code Direction

### LLM adapter

Extend `LLMAdapter` so callers can provide chat messages. Keep `invoke(prompt)`
for existing callers by delegating to a single user message.

### Pipeline

Store optional `promptMessages` in `AgentGraphState`. If an agent builds chat
messages, call LLM slots with those messages. Existing agents can continue using
plain prompt strings.

### Operator Guide

Create `backend/src/agents/operator_guide/system_prompt.py`.

Update `ManualQAPromptBuilder` so it returns:

- `system`: static operator guide system prompt;
- `user`: CSV evidence user prompt.

Leaf agents should expose those messages through the existing pipeline.

## Test Direction

Tests should prove:

- OpenAI adapter sends explicit `system` and `user` messages.
- Local OpenAI-compatible adapter sends explicit `system` and `user` messages.
- Operator guide prompts include system prompt identity separately from CSV
  evidence.
- The pipeline can pass chat messages to the LLM without breaking existing
  prompt-string agents.

## Execution Log

- Created after confirming the current prototype uses a user-message prompt, not
  a true chat system prompt.
- Added failing tests for chat-message delivery through OpenAI, local
  OpenAI-compatible, operator guide leaf agents, and the pipeline.
- Implemented `OPERATOR_GUIDE_SYSTEM_PROMPT`, operator guide prompt messages,
  pipeline `promptMessages`, and adapter `invoke_messages` support.
- Red test result before implementation: 6 expected failures for missing chat
  message APIs and pipeline message passing.
- Focused regression result after implementation:
  `6 passed`.
- Broader targeted result:
  `97 passed`.
- Full backend result, excluding local untracked manual docs router test:
  `155 passed`.
- Lint result for changed files:
  `ruff check` passed.
