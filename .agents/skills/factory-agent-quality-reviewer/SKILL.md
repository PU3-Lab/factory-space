---
name: factory-agent-quality-reviewer
description: Review Factory Space backend agent changes for correctness, maintainability, tests, and boundary coherence after spec approval.
---

# Factory Agent Quality Reviewer

## When to Use

- use this skill only after spec review passes
- use it before final verification or commit
- use it for correctness, maintainability, test strength, and integration boundaries

## Required Inputs

- implementation artifact
- passing spec review artifact
- git diff
- relevant tests
- protocol and agent role docs

## Workflow

1. Verify spec review status is `pass`.
2. Inspect boundary pairs together:
   - protocol schema and pipeline validation
   - pipeline response and WebSocket gateway
   - orchestrator prompt contract and parser
   - cache key inputs and prompt inputs
   - tests and behavior they claim to cover
3. Report bugs, regressions, missing tests, or over-complexity first.
4. Produce status `pass`, `fix`, or `redo`.

## Outputs

Write `_workspace/factory-agent/{task_id}_quality_review.md`:

```markdown
# {task_id} Quality Review

Status: pass|fix|redo

## Findings
- severity:
- file:
- issue:
- impact:
- required fix:

## Test Gaps
- gap:
- recommended test:

## Approval Notes
- only when status is pass
```

## Validation

- no approval without checking tests and integration boundaries
- no style-only findings unless they affect maintainability or correctness
- missing tests are blocking when behavior changed
