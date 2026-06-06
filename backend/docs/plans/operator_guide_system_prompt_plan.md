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
