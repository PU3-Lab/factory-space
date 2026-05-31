---
name: factory-agent-superpowers
description: Coordinate Factory Space backend agent work with Superpowers-style TDD, staged review, and fresh verification.
---

# Factory Agent Superpowers

## When to Use

- use this skill for backend agent pipeline, protocol, WebSocket, LangGraph, or routing work
- use it when a task should follow test-first development and staged review
- use it when the user asks to use Superpowers, harness, or sub-agent review for this repository

## Required Inputs

- the user request
- relevant files under `backend/src`
- relevant tests under `backend/tests`
- decisions in `backend/src/DECISION_LOG.md`
- team contract in `docs/harness/factory-agent/team-spec.md`

## Workflow

1. Create or update `_workspace/factory-agent/{task_id}_request.md` with the exact task and acceptance checks.
2. For behavior changes, write a failing test first and record the command/output in `{task_id}_red.md`.
3. Implement the smallest change that satisfies the test and the documented decision.
4. Record changed files and reasoning in `{task_id}_implementation.md`.
5. Run spec review with `factory-agent-spec-reviewer`.
6. Only after spec review passes, run quality review with `factory-agent-quality-reviewer`.
7. Run fresh verification from `backend/`:

```bash
uv run --extra dev pytest
uv run --extra dev ruff check .
```

8. Record verification output summary in `{task_id}_verification.md`.

## Outputs

- `_workspace/factory-agent/{task_id}_request.md`
- `_workspace/factory-agent/{task_id}_red.md`
- `_workspace/factory-agent/{task_id}_implementation.md`
- `_workspace/factory-agent/{task_id}_spec_review.md`
- `_workspace/factory-agent/{task_id}_quality_review.md`
- `_workspace/factory-agent/{task_id}_verification.md`

## Validation

- every behavior change has a test or an explicit reason why the change is documentation-only
- every review artifact has status `pass`, `fix`, or `redo`
- verification commands are fresh and complete
- decisions that answer user questions are recorded in `backend/src/DECISION_LOG.md`

## References

- `docs/harness/factory-agent/team-spec.md`
- Superpowers principles applied here: test-driven-development, subagent-driven-development, requesting-code-review, verification-before-completion
