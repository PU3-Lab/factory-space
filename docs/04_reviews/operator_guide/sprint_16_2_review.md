# 코드 리뷰: operator_guide RAG Sprint 16.2 (진행 상태 메시지 실시간 스트리밍)

| 항목 | 내용 |
| --- | --- |
| 브랜치 | `feature/operator-guide-rag-runtime-docs` |
| 리뷰 일자 | 2026-06-16 |
| 리뷰 범위 | WebSocket `agent.progress` 규격 수립, LangGraph 파이프라인 콜백 연동, 진행 상태 메시지 카탈로그 구현, WebSocket Gateway `asyncio.to_thread` 비동기 최적화 및 유닛/통합 테스트 검증 |
| 리뷰어 | kimkyungpyo |

## 1. 변경 요약

- **`agent.progress` 프로토콜 스키마 정의**:
  - [messages.py](file:///c:/factory-space/backend/src/protocol/messages.py)에 `AgentProgressEnvelope` 모델을 신설하여 Unreal 클라이언트 및 프론트엔드가 수신할 진행 상태 메시지의 규격(`request_id`, `session_id`, `client_id`, `agent`, `payload`)을 확정했습니다.
- **LangGraph 파이프라인 콜백 전파**:
  - [base.py](file:///c:/factory-space/backend/src/agents/base.py) 및 [runtime.py](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py)에서 파이프라인 실행 시 `RunnableConfig`로부터 `on_progress` 콜백을 추출하여 `AgentContext`로 전달하는 흐름을 연동했습니다.
  - 세션 메모리 복구 및 갱신 노드([runtime.py:L368-411](file:///c:/factory-space/backend/src/agents/pipeline/runtime.py#L368-L411))에서도 콜백 참조가 유실되지 않도록 컨텍스트 바인딩 로직을 보완했습니다.
- **진행 메시지 카탈로그 및 마일스톤 연동**:
  - [service.py](file:///c:/factory-space/backend/src/agents/operator_guide/service.py)에 토픽별(recipe, machine, troubleshooting) 진행 메시지 카탈로그(`PROGRESS_CATALOG`)를 구축했습니다.
  - RAG 검색 진입(`rag_search`), 상태 확인(`state_check`), 트러블슈팅 추가 점검(`power_check`, `document_find`), 최종 답변 정돈(`logic_format`, `step_arrange`) 등 핵심 마일스톤 진입부에서 콜백을 호출하여 클라이언트에 단계별 정황을 실시간 보고하도록 하였습니다.
- **WebSocket Gateway 실시간 비동기 스트리밍 (`asyncio.to_thread`)**:
  - [gateway.py](file:///c:/factory-space/backend/src/websocket_gateway/gateway.py)에서 기존 동기식 차단(blocking) 방식으로 실행되던 `pipeline.run`을 `asyncio.to_thread`를 사용하여 별도 스레드에서 구동하도록 변경했습니다.
  - 이를 통해 파이프라인 연산 중에도 ASGI 메인 이벤트 루프가 차단되지 않고 자유롭게 `agent.progress` 전송 태스크를 수행할 수 있어, 진행 상태 메시지가 최종 답변(`agent.response`)보다 먼저 완벽하게 실시간 스트리밍되는 구조적 기틀을 완성했습니다.

---

## 2. 검증 결과

### 2.1. 자동화 테스트 결과
총 265개의 백엔드 전체 테스트 케이스가 성공적으로 통과하였습니다.
- `uv run pytest tests/test_operator_guide_progress_streaming.py` 통과
- `uv run pytest tests/test_websocket_endpoint.py` 통과
- `uv run pytest` 전체 백엔드 테스트 suite 통과 (265 passed)
- `uv run ruff check` 전체 코드 포맷 및 린트 검사 통과

### 2.2. 신규 및 보완 테스트 상세
```text
tests/test_operator_guide_progress_streaming.py::test_progress_streaming_for_recipe_topic PASSED
tests/test_operator_guide_progress_streaming.py::test_progress_streaming_for_machine_topic PASSED
tests/test_operator_guide_progress_streaming.py::test_progress_streaming_for_troubleshooting_topic PASSED
tests/test_operator_guide_progress_streaming.py::test_pipeline_propagates_on_progress PASSED
tests/test_websocket_endpoint.py::test_agent_websocket_streams_progress_for_operator_guide PASSED

============================== 5 passed in 2.24s ==============================
```

---

## 3. 종합 평가

이번 Sprint 16.2 작업을 통해 대기 시간이 비교적 긴 RAG 답변 생성 및 상태 진단 기간 동안, 플레이어에게 NPC의 사고 과정을 단계적으로 노출하는 실시간 진행률 스트리밍 기능이 완성되었습니다.
특히 LangGraph의 동기식 컴파일 그래프 실행 구조를 훼손하지 않으면서 `asyncio.to_thread`를 연계하여 스레드 안전하게 비동기 웹소켓 채널을 활용하는 고성능 동시성 패턴이 성공적으로 안착되었습니다. 신설된 유닛 및 통합 테스트가 완벽한 전파 순서를 검증하고 전체 265개 테스트가 모두 성공을 기록하여 안정성을 증명하였기에, 본 변경 사항의 최종 머지 및 반영을 승인합니다.
