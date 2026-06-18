# Task 3.1 RED: QuestRuleGenerator missing module

목표:
- `QuestRuleGenerator` 모듈 부재로 인한 실패 확인

RED 확인 명령:
```bash
uv run pytest tests/test_quest_rule_generator.py
```

실행 결과 및 실패 원인:
- `ModuleNotFoundError: No module named 'agents.quest_generator.rule_generator'`
- 아직 `backend/src/agents/quest_generator/rule_generator.py` 파일이 존재하지 않아 수집 단계에서 에러 발생.
