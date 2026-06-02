# korean-agent-prompts Request

## User Request

Agent에 들어간 prompt를 한글로 작성한다.

## Acceptance Checks

- orchestrator routing prompt가 한글 지시문을 사용한다.
- manual Q&A sub-agent routing prompt가 한글 지시문을 사용한다.
- quest generator sub-agent routing prompt가 한글 지시문을 사용한다.
- leaf agent `build_prompt()` 지시문이 한글을 사용한다.
- agent id, sub_agent id, JSON key 이름은 protocol 식별자이므로 유지한다.
- prompt 변경은 테스트로 검증한다.
