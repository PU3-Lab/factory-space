# superpowers-harness Implementation

## Changed Files

- `docs/harness/factory-agent/team-spec.md`: Superpowers 기반 Pipeline + Producer-Reviewer harness 계약 추가
- `.agents/skills/factory-agent-superpowers/SKILL.md`: coordinator workflow 추가
- `.agents/skills/factory-agent-implementer/SKILL.md`: test-first 구현자 skill 추가
- `.agents/skills/factory-agent-spec-reviewer/SKILL.md`: 요청/결정 로그 준수 review skill 추가
- `.agents/skills/factory-agent-quality-reviewer/SKILL.md`: 품질/테스트/경계 일관성 review skill 추가
- `_workspace/factory-agent/README.md`: handoff artifact naming 규칙 추가
- `backend/AGENTS.md`: 현재 `backend/src` 구조와 Superpowers harness 진입점 연결
- `backend/src/DECISION_LOG.md`: Superpowers harness 구성 결정 기록

## Red Check

- command: not applicable for production behavior
- result: structure/documentation acceptance checks were used instead

## Notes

- Superpowers 플러그인은 캐시에 있으나 현재 세션 스킬 목록에 직접 노출되지 않는다.
- 따라서 Superpowers 원칙을 repo-local skill과 team spec으로 고정했다.
