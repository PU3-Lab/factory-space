# superpowers-harness Verification

## Commands

Executed from `backend/`.

```bash
uv run --extra dev pytest
```

Result:

```text
25 passed in 0.39s
```

```bash
uv run --extra dev ruff check .
```

Result:

```text
All checks passed!
```

## Structure Checks

- `.agents/skills/factory-agent-superpowers/SKILL.md` exists with YAML frontmatter.
- `.agents/skills/factory-agent-implementer/SKILL.md` exists with YAML frontmatter.
- `.agents/skills/factory-agent-spec-reviewer/SKILL.md` exists with YAML frontmatter.
- `.agents/skills/factory-agent-quality-reviewer/SKILL.md` exists with YAML frontmatter.
- `docs/harness/factory-agent/team-spec.md` defines handoff artifacts and review gate.
- `backend/src/DECISION_LOG.md` records the Superpowers harness decision.

## Re-run Reason

Verification was rerun after fixing `backend/AGENTS.md` to match the runtime `Agent` protocol.
