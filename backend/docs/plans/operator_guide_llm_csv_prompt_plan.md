# Operator Guide LLM CSV Prompt Plan

## Goal

`operator_guide` should not build a fixed rule-based answer in Python. It should
use CSV rows as manual evidence, pass that evidence into the selected leaf
agent prompt, and let the default LLM slot write the player-facing answer.

## Current Problem

The current prototype routes questions into `ManualQAService`, then
`ManualQAResponseBuilder` assembles `final_answer` from hard-coded Korean
templates. That makes the answer deterministic, but it does not match the
desired agent behavior:

- the answer is effectively prewritten in code;
- prompt quality cannot shape the final response;
- the LLM does not learn from the selected CSV context;
- tests focus on fixed answer strings instead of prompt/evidence behavior.

## Target Behavior

When a player asks an operator guide question:

1. The pipeline selects `operator_guide`.
2. The pipeline selects one leaf agent:
   - `operator_guide.machine_help`
   - `operator_guide.recipe_explainer`
   - `operator_guide.troubleshooter`
3. The leaf agent builds an LLM prompt from:
   - the player question;
   - the leaf agent topic;
   - matched CSV evidence;
   - recommended action metadata;
   - the response contract.
4. The default LLM slot answers with JSON.
5. The pipeline returns the LLM answer.
6. If the LLM is unavailable, fallback returns a short safe response with the
   same CSV metadata.

## Prompt Policy

The selected leaf agent prompt must instruct the LLM to:

- speak like a friendly tutorial NPC in Factory Space;
- use a soft guidance tone instead of command-heavy wording;
- answer in Korean;
- write 4 to 6 sentences;
- use the provided CSV evidence as the main source of truth;
- avoid inventing concrete numbers, machines, recipes, effects, or rules that
  are not present in the evidence;
- lightly add general play guidance when useful, such as checking power, input,
  output, or storage flow;
- naturally include the recommended actions in the answer body;
- return only a JSON object.

## JSON Contract

The LLM response payload should be:

```json
{
  "final_answer": "친절한 튜토리얼 NPC 말투의 답변",
  "actions": [],
  "question": "원본 질문",
  "topic": "machine"
}
```

The pipeline already wraps this payload with runtime metadata.

## File Direction

### Remove rule-based answer construction

`backend/src/agents/operator_guide/response_builder.py` should no longer build
fixed answer sentences.

### Add CSV evidence context construction

Create a focused context builder that:

- classifies the question with the existing classifier;
- finds matching CSV records through `CsvManualQARepository`;
- builds `sources`;
- builds `recommended_actions`;
- exposes compact evidence dictionaries for the prompt;
- provides a short safe fallback answer for LLM-unavailable paths.

### Add prompt construction

Create a focused prompt builder that:

- receives the player question, topic, sub-agent id, and CSV context;
- renders the prompt policy;
- renders CSV evidence as JSON;
- renders the expected JSON output contract.

### Update leaf agents

`machine_help.py`, `recipe_explainer.py`, and `troubleshooter.py` should use the
prompt builder in `build_prompt()`.

### Update fallback

`ManualQAService` should return metadata and a short safe fallback, not a full
rule-based final answer.

## Testing Direction

Tests should verify behavior instead of fixed answer strings:

- prompts include the friendly tutorial NPC instruction;
- prompts include CSV evidence for matched equipment, recipes, resources, or
  troubleshooting rules;
- prompts include the JSON output contract;
- pipeline uses the LLM JSON answer when available;
- fallback keeps `sources`, `recommended_actions`, and `confidence` metadata
  when the LLM is unavailable;
- unknown questions do not hallucinate CSV sources.

## Execution Log

- Created after deciding that rule-based template answers are the wrong
  direction for the prototype.
- This plan intentionally keeps CSV lookup deterministic and moves final answer
  wording into the LLM prompt.
