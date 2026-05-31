# superpowers-harness Red Check

이번 작업은 문서와 repo-local skill 구성 작업이다. production behavior 변경이 아니므로 failing pytest를 만들지 않는다.

대신 최초 상태에서 다음 항목이 없었다.

- `docs/harness/factory-agent/team-spec.md`
- `.agents/skills/factory-agent-superpowers/SKILL.md`
- `.agents/skills/factory-agent-implementer/SKILL.md`
- `.agents/skills/factory-agent-spec-reviewer/SKILL.md`
- `.agents/skills/factory-agent-quality-reviewer/SKILL.md`
- `_workspace/factory-agent/README.md`

검증 기준:

- 위 파일들이 생성되어야 한다.
- 모든 `SKILL.md`는 YAML frontmatter의 `name`, `description`을 가져야 한다.
- team spec은 handoff artifact와 review gate를 정의해야 한다.
