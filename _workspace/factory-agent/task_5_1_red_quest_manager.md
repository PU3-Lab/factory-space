# Task 5.1 RED: QuestManager, Repository, RewardResolver missing modules

목표:
- `QuestManager`, `QuestRepository`, `QuestRewardResolver` 모듈 부재로 인한 실패 확인

RED 확인 명령:
```bash
uv run pytest tests/test_quest_manager.py
```

실행 결과 및 실패 원인:
- `ModuleNotFoundError: No module named 'agents.quest_generator.reward_resolver'`
- 아직 `backend/src/agents/quest_generator/repository.py`, `reward_resolver.py`, `manager.py` 파일들이 존재하지 않아 수집 단계에서 에러 발생.
