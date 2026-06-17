# operator_guide agent-test 서버 연결 가이드 작성 계획

## 목표

Windows 환경에서 사용자가 `uv run`으로 backend 테스트 서버를 실행하고, 브라우저의 `/agent-test` 화면에서 `operator_guide` agent 요청을 직접 보낼 수 있도록 초보자용 가이드를 작성한다.

## 포함 범위

- PowerShell 기준 서버 실행 명령어
- `.env`와 `.env.prod` 실행 방식 차이
- `/health`, `/agent-test`, `/ws/agent` 확인 URL
- `operator_guide` 설비 도움말, 레시피 설명, 트러블슈팅, 여러 질문 예시 JSON
- `bash scripts/run_server.sh`가 Windows에서 실패하는 이유
- 자주 나는 오류와 해결 방법

## 제외 범위

- 서버 코드 수정
- agent runtime 로직 수정
- 실제 OpenAI API 호출 테스트
- Postman 사용법 상세 문서

## 작업 로그

- 2026-06-15: 사용자가 첨부한 `run_server.sh` 기반 실행 화면과 Windows bash 오류를 기준으로 가이드 작성 범위를 정했다.

## 트러블슈팅 로그

- 2026-06-15: Windows PowerShell에서 `bash scripts/run_server.sh` 실행 시 `/bin/bash`가 없어 실패할 수 있다. 이 환경에서는 `uv run python scripts/run_server.py` 또는 `uv run --env-file .env.prod python scripts/run_prod_server.py`를 안내한다.
