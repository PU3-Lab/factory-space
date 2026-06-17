# TDD RED: Quest API Router (Phase 7)

Quest API 라우터 구현 및 앱 등록 전에 작성한 E2E 테스트 파일이 기대한 대로 404 Not Found로 실패(RED)함을 확인했습니다.

## 실행 명령
```bash
uv run pytest tests/test_quest_router.py
```

## 에러 내용
```
tests/test_quest_router.py FF                                            [100%]

=================================== FAILURES ===================================
__________________________ test_quest_router_flow_e2e __________________________
...
        # 2. GET /api/v1/factories/factory_001/quests -> 빈 목록 [] 확인
        with unittest.mock.patch("db.engine.get_db_session", return_value=mock_session_ctx):
            # 2.1 GET 목록 조회
            response = client.get("/api/v1/factories/factory_001/quests")
>           assert response.status_code == 200
E           assert 404 == 200
E            +  where 404 = <Response [404 Not Found]>.status_code

tests/test_quest_router.py:52: AssertionError
```
