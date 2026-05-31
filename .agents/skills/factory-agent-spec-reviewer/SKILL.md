---
name: factory-agent-spec-reviewer
description: Review Factory Space backend agent changes for compliance with the user request, decision log, and harness contract.
---

# Factory Agent Spec Reviewer

## When to Use

- use this skill after implementation and before code quality review
- use it to confirm the change matches the requested scope
- do not use it to perform broad style review

## Required Inputs

- user request or request artifact
- implementation artifact
- git diff
- `backend/src/DECISION_LOG.md`
- `docs/harness/factory-agent/team-spec.md`

## Workflow

1. Compare the change against the exact request.
2. Check whether answers to user questions were recorded in docs when required.
3. Check whether `backend/src` remains the runtime source root.
4. Check whether routing decisions remain prompt/LLM based.
5. Check whether explicit invalid inputs fail instead of being rerouted.
6. Produce status `pass`, `fix`, or `redo`.

## Outputs

Write `_workspace/factory-agent/{task_id}_spec_review.md`:

```markdown
# {task_id} Spec Review

Status: pass|fix|redo

## Checked
- request:
- decisions:
- files:

## Findings
- severity:
- file:
- issue:
- required fix:

## Approval Notes
- only when status is pass
```

## Validation

- status `pass` means no blocking scope or requirement gaps remain
- status `fix` means targeted changes can satisfy the spec
- status `redo` means the implementation is directionally wrong
