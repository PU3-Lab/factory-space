# operator_guide agent-test progress metric 보정 계획

## 목표

`/agent-test` 화면에서 `agent.progress` 메시지를 최종 응답처럼 품질 채점하지 않도록 보정한다.

현재 `agent.progress`는 플레이어에게 "장비 매뉴얼을 펼쳐보는 중입니다..." 같은 진행 상태를 보여주기 위한 중간 메시지다. 따라서 JSON/스키마/품질/토큰/비교 기록은 최종 `agent.response` 또는 `agent.error`에 대해서만 갱신하는 것이 맞다.

## 작업 범위

- `backend/src/docs_router.py`
  - WebSocket 메시지가 `agent.progress`인지 판별한다.
  - progress 메시지는 실시간 응답창과 응답 로그에는 표시한다.
  - progress 메시지는 `updateMetrics`, `updateAnalysis`, `renderMaterialResult` 호출 대상에서 제외한다.
  - 응답 로그 라벨을 `PROGRESS`로 분리한다.

- `backend/tests/test_docs_router.py`
  - `/agent-test` HTML에 progress metric 제외 로직이 포함되는지 확인한다.

## 검증

```powershell
cd C:\factory-space\backend
uv run pytest tests/test_docs_router.py -q
uv run ruff check .
```

## 작업 로그

- 2026-06-16: agent-test에서 `agent.progress` 메시지가 `70/100`으로 채점되어 최종 답변 품질처럼 오해되는 문제를 확인했다.

## 트러블슈팅 로그

- 2026-06-16: 원인은 최종 응답이 아닌 진행 메시지까지 `updateMetrics`와 `updateAnalysis`에 전달되는 것이었다. progress는 로그로만 남기고 최종 응답만 품질 채점하도록 보정한다.
