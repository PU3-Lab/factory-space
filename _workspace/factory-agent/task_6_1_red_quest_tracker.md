# TDD RED: QuestProgressTracker

QuestProgressTracker 구현 전에 작성한 테스트 파일이 기대한 대로 실패(RED)함을 확인했습니다.

## 실행 명령
```bash
uv run pytest tests/test_quest_tracker.py
```

## 에러 내용
```
ImportError while importing test module '/Users/kimkyungpyo/Workspaces/projests/factory-space/backend/tests/test_quest_tracker.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
../../../../.local/share/uv/python/cpython-3.12.13-macos-aarch64-none/lib/python3.12/importlib/__init__.py:90: in import_module
    return _bootstrap._gcd_import(name[level:], package, level)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
tests/test_quest_tracker.py:11: in <module>
    from agents.quest_generator.tracker import QuestProgressTracker
E   ModuleNotFoundError: No module named 'agents.quest_generator.tracker'
```
