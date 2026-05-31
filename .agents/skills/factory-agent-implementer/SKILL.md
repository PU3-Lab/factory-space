---
name: factory-agent-implementer
description: Implement one Factory Space backend agent task using test-first or verification-first development.
---

# Factory Agent Implementer

## When to Use

- use this skill for a single bounded backend implementation task
- use it after `factory-agent-superpowers` has defined a task id and acceptance checks
- do not use it for broad architecture planning or final review

## Required Inputs

- task id
- request artifact path
- target files
- acceptance checks
- relevant decisions from `backend/src/DECISION_LOG.md`

## Workflow

1. Read the request artifact and relevant decision log entries.
2. For behavior changes, add or update the narrowest failing pytest first.
3. Run the targeted test and confirm it fails for the expected reason.
4. Implement the smallest code change.
5. Run the targeted test again.
6. Update docs only when the behavior or user-facing decision changed.
7. Write `_workspace/factory-agent/{task_id}_implementation.md`.

## Outputs

Implementation artifact format:

```markdown
# {task_id} Implementation

## Changed Files
- `path`: reason

## Red Check
- command:
- result:

## Green Check
- command:
- result:

## Notes
- constraints, tradeoffs, or follow-up risk
```

## Validation

- no production behavior is changed without a failing test first
- no unrelated refactor is included
- prompt-based routing remains prompt-based and is not replaced by keyword logic
- explicit `agent` and `sub_agent` inputs are validated instead of silently corrected
