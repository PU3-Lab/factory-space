# Factory Space 프로젝트 검토 보고서

## 검토 날짜
2026-05-27

## 프로젝트 상태: ✅ 담당자 기능 구현 전달 준비 완료

---

## 1. 프로젝트 구조

### 1.1 핵심 아키텍처 ✅

- **WebSocket 엔드포인트**: `/ws` 단일 채널, `client_id` JSON 필드 기반 라우팅
- **메시지 라우터**: `type` 필드로 `ping`, `agent_request`, `action_result` 분기
- **Agent Orchestrator**: `agent_id` 기반 에이전트 선택 및 호출
- **Agent Registry**: 초기 4개 에이전트 등록 (`factory_optimization`, `qa_chatbot`, `quest`, `material_generation`)

모든 핵심 계약이 구현되어 있고, 각 에이전트 담당자가 내부 로직만 구현하면 됩니다.

### 1.2 폴더 구조 ✅

```
src/factory_space/
├── core/           # 모든 에이전트가 따르는 공통 계약 (변경 주의)
│   ├── agents/     # BaseAgent 인터페이스, Orchestrator, Registry
│   ├── actions/    # Action 스키마 및 Dispatcher
│   ├── state/      # AgentContext
│   ├── db/         # DB 베이스 클래스
│   └── engines/    # 추론 엔진 인터페이스
├── agents/         # 각 에이전트 폴더 (담당자 독립 개발)
│   ├── factory_optimization/
│   ├── qa_chatbot/
│   ├── quest/
│   └── material_generation/
├── shared/         # 다중 에이전트가 공유하는 구현 (필요시만 사용)
│   ├── repositories/
│   ├── services/
│   ├── schemas/
│   └── vectorstores/
├── messages/       # WebSocket 메시지 프로토콜
├── websocket/      # WebSocket 엔드포인트 및 연결 관리
└── app.py          # FastAPI 앱 진입점
```

### 1.3 각 에이전트 준비 상태 ✅

**factory_optimization** (공장 최적화 에이전트)
- ✅ `agent.py`: 기본 구현 (Stub)
- ✅ `schemas.py`: 스키마 템플릿
- ✅ `service.py`, `repository.py`: 골격 준비
- ✅ `rules.py`: 룰베이스 로직 공간 예약
- ✅ `prompts.py`: LLM 프롬프트 공간 예약
- ✅ `models.py`: DB 모델 공간 예약
- ✅ `tests/`, `scenarios/`: 테스트 공간 예약

**qa_chatbot, quest, material_generation**
- 모두 위와 동일한 구조로 준비됨
- 각 에이전트는 `agent_id`로 고유하게 식별됨

---

## 2. WebSocket 통신 흐름 ✅

1. 클라이언트 → `/ws`로 WebSocket 연결
2. 첫 메시지에 `client_id` 필드 필수 포함
3. 메시지 구조: `type`, `version`, `session_id`, `client_id`, `agent` (필요시), `payload`
4. 메시지 타입별 처리:
   - `ping`: 즉시 `pong` 응답
   - `agent_request`: `agent` 필드로 해당 에이전트 호출
   - `action_result`: 수신 확인 응답
5. 에이전트는 `AgentResponse` 반환 → `MessageEnvelope`로 변환 → Unreal로 전송

**참고:**
- 모든 메시지는 JSON으로 직렬화됨
- 클라이언트는 `client_id`로 추적되지만, 같은 연결 내에서 재등록하지 않음
- 에러는 구조화된 `ErrorMessage`로 반환

---

## 3. Agent 개발 가이드 ✅

### 3.1 기본 계약

모든 에이전트는 다음을 구현해야 함:

```python
class YourAgent:
    agent_id = "your_agent"
    
    async def process(
        self,
        request: AgentRequest,
        context: AgentContext,
    ) -> AgentResponse:
        # 내부 구현
        return AgentResponse(...)
```

### 3.2 응답 구조

모든 에이전트는 다음을 반환:

```python
AgentResponse(
    session_id=context.session_id,
    request_id=context.request_id,
    client_id=context.client_id,
    agent=self.agent_id,
    payload=AgentResponsePayload(
        text="응답 텍스트",
        actions=[...],  # 구조화된 action 목록
        metadata={...}
    )
)
```

### 3.3 Action 정의

모든 action은 구조화됨:

```python
Action(
    name="action_name",
    args={"key": "value", ...}
)
```

Unreal이 예측 가능하게 실행할 수 있는 명시적 구조.

### 3.4 담당자별 개발 범위

각 에이전트 담당자는 **자신의 `agents/{agent_name}/` 폴더 내에서만 작업합니다**:

- ✅ `agent.py`: 메인 로직
- ✅ `service.py`: 도메인 비즈니스 로직
- ✅ `repository.py`: DB 접근
- ✅ `models.py`: DB 모델
- ✅ `rules.py`: 룰베이스 (사용 시)
- ✅ `prompts.py`: LLM 프롬프트 (사용 시)
- ✅ `schemas.py`: 요청/응답 모델
- ✅ `tests/`: 유닛/통합 테스트
- ✅ `scenarios/`: 시나리오 테스트

**다른 에이전트 폴더는 수정하지 않음.**

### 3.5 DB 접근 규칙

```
Agent -> Service -> Repository -> Database
```

- Agent는 raw DB session 직접 사용 금지
- Service를 통해 repository 호출
- 필요 시 `shared/` 에 공통 repository/service 추가

---

## 4. 테스트 상태 ✅

### 4.1 기존 테스트 결과

```
tests/test_agent_contracts.py       3/3 ✅
tests/test_message_router.py        4/4 ✅
tests/test_placeholder.py           1/1 ✅
tests/test_websocket_endpoint.py    3/3 ✅
─────────────────────────────────
Total: 11/11 PASSED
```

모든 핵심 계약 테스트가 통과.

### 4.2 WebSocket 테스트 스크립트 ✅

```bash
# 각 에이전트 테스트
python scripts/ws_test_factory_optimization.py
python scripts/ws_test_material_generation.py
python scripts/ws_test_qa_chatbot.py
python scripts/ws_test_quest.py

# 기본 ping 테스트
python scripts/ws_test_client.py
```

모든 테스트 스크립트가 제공됨.

---

## 5. 문서 상태 ✅

- `AGENTS.md`: 프로젝트 원칙, 개발 규칙 명시
- `docs/architecture.md`: 아키텍처 및 책임 분리 설명
- `docs/message-protocol.md`: WebSocket 메시지 계약 명시
- `docs/agent-development-guide.md`: Agent 개발 가이드
- `README.md`: 프로젝트 및 환경 설정 안내

모든 문서가 최신 상태로 유지됨.

---

## 6. 수정 사항 요약 (이번 세션)

### 6.1 WebSocket 통신 개선

- `/ws/{client_id}` → `/ws` 단일 채널로 통합
- URL 기반 라우팅 → JSON의 `client_id` 필드 기반 라우팅
- `websocket.accept()` 순서 수정 (첫 메시지 수신 전 accept)
- `connection_manager.connect()` 중복 accept 제거

### 6.2 테스트 스크립트 추가

- `scripts/ws_test_factory_optimization.py`
- `scripts/ws_test_material_generation.py`
- `scripts/ws_test_qa_chatbot.py`
- `scripts/ws_test_quest.py`

각 에이전트별 WebSocket 요청 테스트 스크립트 제공.

### 6.3 문서 업데이트

- `docs/architecture.md`: WebSocket 단일 채널 설명 추가
- `docs/message-protocol.md`: `client_id` 필수 표시 및 연결 방식 명시

---

## 7. 담당자에게 전달하기 전 확인 사항 ✅

- [x] 핵심 WebSocket 통신 흐름 정상 작동
- [x] 모든 테스트 통과 (11/11)
- [x] 각 에이전트는 독립적으로 개발 가능한 구조
- [x] 공통 계약 (Agent 인터페이스, Action, Context) 명확
- [x] DB 접근 규칙 (Agent -> Service -> Repository) 정의됨
- [x] 문서가 충분하고 최신 상태
- [x] WebSocket 테스트 스크립트 제공

---

## 8. 담당자 체크리스트

각 에이전트 담당자는 다음을 구현:

### factory_optimization 담당자
- [ ] `service.py`: 공장 최적화 로직
- [ ] `repository.py`: 공장 데이터 DB 접근
- [ ] `models.py`: 공장 관련 DB 테이블
- [ ] `agent.py`: Stub에서 실제 로직으로 변경
- [ ] `rules.py` 또는 `prompts.py`: 규칙/LLM 기반 추론
- [ ] `tests/`: 유닛 테스트 작성
- [ ] `scenarios/`: 시나리오 테스트 작성

### qa_chatbot, quest, material_generation 담당자
- 위와 동일한 체크리스트

---

## 9. 권장 사항

1. **공통 계약 변경 금지**: `core/` 수정은 모든 담당자와 협의 후 진행
2. **독립 개발 강조**: 자신의 에이전트 폴더 외 수정 금지
3. **테스트 우선**: 기능 구현 후 해당 폴더의 `tests/`에 테스트 추가
4. **시나리오 작성**: `scenarios/` 폴더에 시나리오 테스트 추가
5. **DB는 Service 경계**: Agent에서 직접 DB 접근 금지

---

## 10. 결론

**상태: ✅ 담당자 독립 개발 전달 준비 완료**

- 핵심 아키텍처 완성
- 모든 계약 구현됨
- WebSocket 통신 정상 작동
- 테스트 및 문서 완비

**담당자는 각자의 `agents/{agent_name}/` 폴더 내에서만 개발하면 됩니다.**
