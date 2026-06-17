# operator_guide Progress Message Streaming 계획

## 목적

operator_guide가 최종 답변을 생성하는 동안 Unreal UI에 짧은 진행 상태 문구를 스트리밍한다.

이 기능은 LLM의 내부 chain-of-thought를 노출하는 기능이 아니다. 플레이어에게 "답변을 생각중"이라는 정적인 문구 대신, agent pipeline의 실제 단계에 맞춘 안전한 UX 메시지를 보여주는 기능이다.

## 배경

플레이어가 문제 해결 질문을 했을 때 아래처럼 NPC가 작업 중인 느낌을 줄 수 있다.

```text
공장의 전체 흐름을 읽는 중입니다...
선택된 장비 상태를 확인하는 중입니다...
전력과 입력 자원 상태를 대조하는 중입니다...
관련 문제 해결 매뉴얼을 찾는 중입니다...
점검 순서를 정리하는 중입니다...
```

장비 설명 질문에서는 아래처럼 보여줄 수 있다.

```text
장비 매뉴얼을 펼쳐보는 중입니다...
입력과 출력 자원을 확인하는 중입니다...
연결 가능한 장비를 살펴보는 중입니다...
```

## 설계 원칙

```text
- 내부 추론 전문을 노출하지 않는다.
- 시스템 프롬프트, 숨겨진 정책, API 키, chain-of-thought는 절대 포함하지 않는다.
- 메시지는 leaf agent, question type, pipeline stage 기준의 안전한 정적 문구로 만든다.
- WebSocket에서는 최종 agent.response 전에 agent.progress 이벤트를 보낸다.
- Unreal UI는 agent.progress를 NPC 말풍선 또는 상태 라벨로 표시한다.
```

## 이벤트 예시

```json
{
  "type": "agent.progress",
  "request_id": "operator-guide-trouble-001",
  "session_id": "demo-session",
  "client_id": "unreal-client",
  "agent": "operator_guide",
  "payload": {
    "stage": "rag_search",
    "message": "관련 문제 해결 매뉴얼을 찾는 중입니다..."
  }
}
```

## 완료 기준

```text
- operator_guide 요청 처리 중 agent.progress 이벤트를 보낼 수 있다.
- 장비 설명, 레시피 설명, 트러블슈팅 질문별 progress message가 다르게 나온다.
- 최종 agent.response 구조는 기존 계약을 깨지 않는다.
- agent-test 또는 WebSocket 테스트 화면에서 progress event와 최종 response를 모두 확인할 수 있다.
- 문서에 "내부 추론 노출이 아니라 UX용 진행 상태 메시지"임을 명확히 설명한다.
```

## 작업 로그

- 2026-06-16: LLM 내부 생각을 보여주는 기능이 아니라, 플레이어가 보는 진행 상태 말풍선 UX로 범위를 정했다.

## 트러블슈팅 로그

- 2026-06-16: chain-of-thought를 그대로 출력하는 방식은 보안과 제품 안정성 관점에서 제외하고, pipeline stage 기반의 안전한 progress message로 설계한다.
