# operator_guide agent-test 응답 로그 스크롤 수정 계획

## 목표

`/agent-test` 테스트 콘솔에서 응답 로그가 길어질 때 페이지 전체가 밀리지 않고, 응답 로그 영역 안에서 독립적으로 스크롤되도록 수정한다.

## 문제

현재 `/agent-test` 화면은 `backend/src/docs_router.py`의 `_render_test_page()`에서 HTML/CSS/JS를 문자열로 렌더링한다.

응답 로그 영역은 `#log`이고, WebSocket 응답은 JavaScript `addCard()` 함수가 `#log` 안에 카드 형태로 추가한다.

긴 JSON 응답이 들어오면 `<pre>` 안의 긴 줄 때문에 화면 전체 가로/세로 스크롤이 생기고, 사용자가 응답 로그만 편하게 스크롤하기 어렵다.

## 수정 방향

- `#log`를 고정 높이의 독립 스크롤 영역으로 만든다.
- `#log`에 `overflow-y: auto`와 `overscroll-behavior: contain`을 둔다.
- JSON `<pre>`에는 `white-space: pre-wrap`과 `word-break: break-word`를 적용한다.
- 각 카드의 `<pre>`도 너무 길어지면 내부 스크롤이 가능하게 한다.

## 검증

- `/agent-test` HTML에 응답 로그 스크롤 CSS가 포함되는지 테스트한다.
- `backend/tests/test_docs_router.py`만 실행한다.

## 작업 로그

- 2026-06-15: `/agent-test` 응답 로그 스크롤 문제를 확인하고 수정 범위를 `backend/src/docs_router.py`로 한정했다.
- 2026-06-15: `#log`를 독립 스크롤 영역으로 만들고, JSON `<pre>`에 줄바꿈과 내부 스크롤을 추가했다.
- 2026-06-15: `/agent-test` HTML에 스크롤 CSS가 포함되는지 확인하는 테스트를 추가했다.

## 검증 로그

- 2026-06-15: RED 확인 `uv run pytest tests/test_docs_router.py -q` 실패. 원인: `overscroll-behavior: contain` CSS가 아직 없음.
- 2026-06-15: GREEN 확인 `uv run pytest tests/test_docs_router.py -q` 통과.
- 2026-06-15: lint `uv run ruff check src/docs_router.py tests/test_docs_router.py` 통과.

## 트러블슈팅 로그

- 2026-06-15: 실행 중인 `127.0.0.1:18000/agent-test` 서버는 아직 수정 전 HTML을 반환했다. 서버가 reload 없이 실행 중이면 브라우저 확인 전 backend 서버 재시작이 필요하다.
