# korean-agent-prompts Implementation

## Changed Files

- `backend/src/agents/orchestrator.py`: top-level routing prompt를 한글로 변경
- `backend/src/agents/manual_qa/agent.py`: manual Q&A sub-agent routing prompt를 한글로 변경
- `backend/src/agents/quest_generator/agent.py`: quest sub-agent routing prompt를 한글로 변경
- `backend/src/agents/process_optimizer.py`: 공정 최적화 prompt를 한글로 변경
- `backend/src/agents/new_material_generator.py`: 신소재 생성 prompt를 한글로 변경
- `backend/src/agents/manual_qa/*.py`: manual QA leaf prompt를 한글로 변경
- `backend/src/agents/quest_generator/*.py`: quest leaf prompt를 한글로 변경
- `backend/tests/test_message_router.py`: routing prompt 한글 검증으로 변경
- `backend/tests/test_agent_leaf_behaviors.py`: leaf prompt 한글 검증으로 변경
- `backend/src/DECISION_LOG.md`: prompt 언어 결정 기록

## Notes

- agent id, sub_agent id, JSON key는 protocol 계약이므로 영어 식별자를 유지했다.
- prompt 지시문과 역할 설명만 한글로 바꿨다.
