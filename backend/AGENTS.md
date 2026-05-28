# AGENTS.md

## 프로젝트 목표

Factory Space는 Unreal Engine과 WebSocket으로 통신하는 Python 백엔드 프로젝트입니다. 백엔드는 Unreal에서 들어오는 요청을 받아 목적별 AI agent로 라우팅하고, agent의 응답과 action을 다시 Unreal로 전달합니다.

각 agent는 공통 입력/출력 계약을 지켜야 합니다. 내부 구현은 agent 담당자가 선택할 수 있으며, 룰베이스, LLM, RAG, 시뮬레이션, DB 기반 추론, 하이브리드 추론 등으로 자유롭게 확장할 수 있습니다. 중요한 것은 Unreal과 통신하는 외부 프로토콜과 agent 응답 구조를 안정적으로 유지하는 것입니다.

초기 agent 후보:

- 공장 최적화 Agent
- Q&A 챗봇 Agent
- 퀘스트 Agent
- 신물질 생성 Agent

이후 agent는 추가될 수 있습니다.

## 핵심 원칙

- 각 agent는 담당자가 독립적으로 개발하기 쉬운 구조를 유지합니다.
- agent 내부 구현 방식은 담당자가 선택합니다.
- 룰베이스, LLM, RAG, 시뮬레이션, 하이브리드 추론 등은 같은 agent 입출력 계약 뒤에 둡니다.
- WebSocket 메시지 프로토콜은 안정적으로 유지하고, 변경 시 버전을 고려합니다.
- 공통 코드는 작고 명확하게 유지합니다.
- 한 agent가 다른 agent의 내부 파일에 직접 의존하지 않도록 합니다.
- agent가 DB에 접근해야 할 때는 service/repository 계층을 통해 접근합니다.

## 담당 영역 구조

대부분의 기능 작업은 각 agent 폴더 안에서 이루어져야 합니다.

```text
src/factory_space/agents/{agent_name}/
```

각 agent는 자신의 구현 세부사항을 최대한 직접 소유합니다.

```text
agent.py        # agent 진입점
schemas.py      # agent 전용 요청/응답 모델
rules.py        # 룰베이스 POC 로직
service.py      # agent 도메인 로직
repository.py   # agent 전용 DB 접근
models.py       # agent가 소유하는 DB 모델
prompts.py      # 향후 LLM prompt
tests/          # agent 테스트
scenarios/      # agent 시나리오
```

`core/`는 모든 agent가 따라야 하는 공통 계약에만 사용합니다. `shared/`는 둘 이상의 agent가 실제로 같은 구현을 공유해야 할 때만 사용합니다.

## 공통 영역

```text
src/factory_space/core/
```

모든 agent가 공유하는 계약과 런타임 기본 요소를 둡니다.

- Base agent 인터페이스
- Agent orchestrator
- Agent registry
- Reasoning engine 인터페이스
- Action schema
- Runtime context/state 계약

`core/` 변경은 모든 agent에 영향을 줄 수 있으므로 신중하게 진행합니다.

```text
src/factory_space/shared/
```

여러 agent가 재사용하는 구현을 둡니다.

- 공통 repository
- 공통 service
- Vector store adapter
- 공통 schema
- cross-agent memory 유틸리티

언젠가 재사용될 것 같다는 이유만으로 `shared/`에 올리지 않습니다. 실제로 둘 이상의 agent가 필요로 할 때 공통화합니다.

## Agent 계약

모든 agent는 개념적으로 다음 형태의 안정적인 진입점을 가져야 합니다.

```python
class SomeAgent:
    agent_id = "some_agent"

    async def process(self, request, context):
        ...
```

agent는 Unreal로 직렬화해서 보낼 수 있는 구조화된 응답을 반환해야 합니다.

agent가 WebSocket 동작을 직접 결정하지 않습니다. agent는 응답과 action을 만들고, 전송은 transport 계층이 담당합니다.

## 추론 Engine

agent의 목적과 추론 방식을 분리합니다. 공통부는 agent의 입력/출력 계약만 강하게 요구하고, 내부 추론 방식은 각 agent가 선택합니다.

좋은 예:

```text
QuestAgent + RuleBasedEngine
QuestAgent + LLMEngine
QuestAgent + HybridEngine
```

특정 구현 방식에만 맞는 가정이 agent 계약에 섞이지 않도록 합니다.

룰베이스를 사용하는 agent는 다음 위치에 규칙 로직을 둘 수 있습니다.

```text
src/factory_space/agents/{agent_name}/rules.py
```

LLM을 사용하는 agent는 다음 위치에 prompt를 둘 수 있습니다.

```text
src/factory_space/agents/{agent_name}/prompts.py
```

## DB 접근

Q&A 문서, 퀘스트 진행 상태, 공장 상태, 신물질 데이터, 세션 상태, 로그 등 agent별로 DB가 필요할 수 있습니다.

의존 방향은 다음을 따릅니다.

```text
Agent -> Service -> Repository -> Database
```

규칙:

- agent 코드는 raw database session 대신 service를 호출합니다.
- repository 코드는 DB query를 담당합니다.
- agent 전용 table은 우선 해당 agent의 `models.py`에서 시작합니다.
- session, agent log 같은 공통 table은 shared/core 쪽 DB 모듈에 둘 수 있습니다.
- vector DB 접근은 store 또는 repository 인터페이스 뒤에 둡니다.

## WebSocket 프로토콜

Unreal과 주고받는 메시지는 계약입니다. 메시지 구조를 임의로 깨지 않습니다.

예상 메시지 종류:

- `ping`
- `pong`
- `agent_request`
- `agent_response`
- `action_request`
- `action_result`
- `error`

호환되지 않는 방식으로 프로토콜을 변경해야 한다면 version field를 추가하거나 갱신하고 문서화합니다.

## Action

agent output은 임의의 텍스트가 아니라 구조화된 action으로 표현합니다.

예시:

```json
{
  "name": "highlight_object",
  "args": {
    "object_id": "packaging_machine_01"
  }
}
```

Unreal이 예측 가능하게 실행할 수 있도록 backend의 action schema는 명시적으로 유지합니다.

## 개발 규칙

- 요청받은 agent 또는 공통 계약 범위 안에서만 수정합니다.
- 한 agent 작업 중 관련 없는 다른 agent를 리팩터링하지 않습니다.
- agent 동작이 바뀌면 테스트나 시나리오도 함께 추가/수정합니다.
- schema는 Pydantic model로 명시적으로 정의합니다.
- 모듈은 작고 책임이 분명하게 유지합니다.
- 임시 데이터나 생성물은 의도된 fixture가 아니라면 source control에 넣지 않습니다.
- 새로운 message type, action type, shared contract를 추가하면 문서화합니다.

## 새 Agent 추가 절차

1. `src/factory_space/agents/{agent_name}/` 폴더를 만듭니다.
2. `agent.py`, `schemas.py`, `rules.py`, `service.py`, `repository.py`, `tests/`를 추가합니다.
3. 공통 agent 계약을 구현합니다.
4. 중앙 registry에 agent를 등록합니다.
5. 최소 1개의 scenario 또는 test를 추가합니다.
6. 사용자나 다른 담당자가 알아야 하는 agent라면 README 또는 docs를 갱신합니다.

## POC 기준

첫 버전에서는 다음을 우선합니다.

- FastAPI WebSocket backend
- 로컬 구조화 데이터용 SQLite
- 단순한 repository/service 경계
- scenario 기반 테스트

각 agent의 내부 구현은 담당자가 선택합니다. 공통부는 WebSocket 메시지, agent request/response, action schema, 테스트 시나리오를 안정적으로 유지하는 데 집중합니다.
