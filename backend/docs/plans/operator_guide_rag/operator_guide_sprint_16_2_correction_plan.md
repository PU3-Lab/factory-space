# operator_guide Sprint 16.2 보정 계획

## 목적

Sprint 16.2에서 추가한 `agent.progress` 진행 메시지 스트리밍을 실제 시연에 안전하게 사용할 수 있도록 보정한다.

이번 보정의 핵심은 다음 두 가지다.

```text
1. 한 요청에서 같은 progress message가 중복 전송되지 않게 한다.
2. ruff와 관련 테스트가 실제로 통과하도록 테스트와 import를 정리한다.
```

## 현재 문제

`AgentPipeline.build_prompt` 단계에서 `build_prompt`와 `build_prompt_messages`를 모두 호출한다.

두 함수가 같은 `ManualQAService.build_prompt_context`를 호출하면서 `on_progress`를 함께 전달하면, 같은 진행 메시지가 두 번 나갈 수 있다.

또한 기존 테스트는 `len(progress_calls) >= 3`처럼 최소 개수만 확인해서 중복 메시지를 잡지 못한다.

## 구현 범위

- pipeline progress 중복 emit 방지
- pipeline 테스트에서 exact progress sequence 검증
- WebSocket progress 테스트에서 중복 없는 순서 검증
- ruff import 정리

## 제외 범위

- LLM이 직접 progress message를 생성하는 구조
- Unreal UI 실제 구현
- 새로운 progress stage 추가

## 검증 계획

```powershell
cd C:\factory-space\backend
uv run pytest tests/test_operator_guide_progress_streaming.py -q
uv run pytest tests/test_websocket_endpoint.py -q
uv run ruff check .
```

## 작업 로그

- 2026-06-16: Sprint 16.2 리뷰에서 progress 중복 emit 가능성과 ruff 실패를 확인했다.
- 2026-06-16: 보정 작업은 TDD 순서로 진행하기로 했다.
- 2026-06-16: `AgentPipeline.build_prompt`에서 `build_prompt_messages`를 호출할 때 `on_progress=None` 컨텍스트를 사용하도록 수정했다.
- 2026-06-16: LLM fallback 응답 생성 시에도 `on_progress=None` 컨텍스트를 사용해 같은 progress message가 반복되지 않도록 수정했다.
- 2026-06-16: progress 단위 테스트와 WebSocket 테스트를 exact sequence 검증으로 강화했다.
- 2026-06-16: `uv run pytest tests/test_operator_guide_progress_streaming.py tests/test_websocket_endpoint.py -q` 결과 12개 테스트 통과를 확인했다.
- 2026-06-16: `uv run ruff check .` 결과 전체 ruff 통과를 확인했다.

## 트러블슈팅 로그

- 2026-06-16: PowerShell `Get-Content` 출력에서 한글이 깨져 보였으나 `rg` 기준 실제 파일의 주요 한글 문구는 정상으로 확인했다.
