# AI Agent 아키텍처

Factory Space 백엔드의 AI Agent 실행 구조를 정리한 문서입니다. Unreal 클라이언트가 WebSocket으로 보낸 요청을 목적별 AI agent로 라우팅하고, 응답과 action을 다시 Unreal로 전달합니다.

- Source root: `backend/src`
- 핵심 원칙: agent/sub-agent 선택은 keyword/if-else가 아니라 **프롬프트 기반 LLM 결정** (`backend/AGENTS.md`)

---

## 1. 전체 아키텍처

```mermaid
flowchart TD
    UE["Unreal 클라이언트"] -->|WebSocket JSON| GW["WebSocket 게이트웨이<br/>/ws/agent · async · progress 스트림"]
    GW -->|agent.request| PIPE

    subgraph PIPE["Agent 파이프라인 (LangGraph StateGraph)"]
        direction TB
        CTX["build_context<br/>validate_envelope"] --> ROUTE["route_top_agent"]
        ROUTE --> ORCH{"Orchestrator<br/>프롬프트 기반 LLM 라우팅"}
        CACHE["cache_lookup"]
        CACHE -->|hit| RESP["build_agent_response"]
        CACHE -->|miss| PROMPT["build_prompt"]
        PROMPT --> LLMCHAIN["LLM 폴백 체인<br/>default → fb1 → fb2"]
        LLMCHAIN -->|tool 요청| TOOL["tool_node → followup"]
        TOOL --> PARSE
        LLMCHAIN -->|응답| PARSE["parse_llm_response"]
        LLMCHAIN -.전부 실패.-> DFB["결정론적 fallback()"]
        PARSE --> VAL["validate_schema"]
        DFB --> VAL
        VAL --> CW["cache_write"] --> RESP
    end

    ORCH -->|operator_guide| OG
    ORCH -->|quest_generator| QG
    ORCH -->|process_optimizer| PO
    ORCH -->|material_generation| MG
    ORCH -->|new_material_generator| NMG

    subgraph AGENTS["최상위 에이전트 (5종)"]
        direction LR
        OG["operator_guide<br/>2차 LLM 라우팅"]
        QG["quest_generator<br/>2차 LLM 라우팅"]
        PO["process_optimizer<br/>operation 분기"]
        MG["material_generation"]
        NMG["new_material_generator"]
    end

    OG --> ROUTER["AgentRouter (registry)<br/>id → 구현체"]
    QG --> ROUTER
    NMG --> ROUTER
    ROUTER --> CACHE
    MG -->|synthesize, DB 세션| RESP
    PO -->|operation 서브그래프| RESP

    LLMCHAIN --> ADAPT["LLM 어댑터<br/>Google · OpenAI · Local · Noop"]

    RESP -->|agent.response| GW
    GW -->|WebSocket JSON| UE
```

### 흐름 요약

1. **전송** — Unreal이 WebSocket(`/ws/agent`)으로 `agent.request` 전송. 게이트웨이가 비동기 스레드로 파이프라인을 실행하고 `agent.progress`를 실시간 스트리밍.
2. **파이프라인** — LangGraph `StateGraph`가 `build_context → validate → route → cache → prompt → LLM → parse → validate → cache_write → response` 순으로 진행.
3. **2단계 라우팅** — Orchestrator가 LLM으로 최상위 5종 중 하나 선택 → `operator_guide`/`quest_generator`는 다시 2차 LLM 라우팅으로 리프 선택.
4. **탄력성** — 캐시 히트 시 LLM 생략, LLM은 3-슬롯 폴백 체인, 전부 실패 시 결정론적 `fallback()`으로 무중단.
5. **특수 경로** — `material_generation`은 LLM 체인 대신 DB 세션 기반 자체 서브그래프, `process_optimizer`는 operation별 자체 서브그래프로 직행.

### 주요 파일

| 계층 | 파일 |
|---|---|
| 게이트웨이 | `backend/src/websocket_gateway/gateway.py` |
| 파이프라인 | `backend/src/agents/pipeline/runtime.py`, `graph_edges.py` |
| 오케스트레이터 | `backend/src/agents/orchestrator.py`, `agent_catalog.py` |
| 라우터 | `backend/src/agents/router.py` |
| LLM 어댑터 | `backend/src/llm/adapter.py` |
| 공통 계약 | `backend/src/agents/base.py`, `backend/src/protocol/` |

---

## 2. 리프 라우팅 구조

```mermaid
flowchart TD
    ORCH["Orchestrator<br/>(프롬프트 기반 LLM 라우팅)"]

    ORCH -->|top-level 결정| OG["operator_guide<br/>(2차 LLM 라우팅)"]
    ORCH -->|top-level 결정| QG["quest_generator<br/>(2차 LLM 라우팅)"]
    ORCH -->|단일 리프| PO["process_optimizer<br/>(자체 서브그래프)"]
    ORCH -->|단일 리프| MG["material_generation"]
    ORCH -->|단일 리프| NMG["new_material_generator"]

    subgraph OGLEAF["operator_guide 리프"]
        OG -->|사용법| MH["operator_guide.machine_help"]
        OG -->|레시피 설명| RE["operator_guide.recipe_explainer"]
        OG -->|문제 해결| TS["operator_guide.troubleshooter"]
    end

    subgraph QGLEAF["quest_generator 리프"]
        QG -->|생산 퀘스트| PQ["quest_generator.production_quest"]
        QG -->|경제 퀘스트| EQ["quest_generator.economy_quest"]
    end

    subgraph POOPS["process_optimizer 오퍼레이션"]
        PO --> ANALYZE["analyze / apply / undo / measure"]
        PO --> SU["state_update"]
        PO --> SQ["subquest_check"]
    end

    MH --> ROUTER["AgentRouter<br/>(id → 구현체 registry)"]
    RE --> ROUTER
    TS --> ROUTER
    PQ --> ROUTER
    EQ --> ROUTER
    MG --> ROUTER
    NMG --> ROUTER

    ROUTER --> LLM["build_prompt → LLM 폴백 체인<br/>default → fb1 → fb2 → 결정론적 fallback"]
```

- **2차 LLM 라우팅** — `operator_guide`(3개 리프), `quest_generator`(2개 리프)만 sub-agent를 다시 LLM으로 선택. payload에 `sub_agent`가 명시되면 생략.
- **단일 리프** — `material_generation`, `new_material_generator`는 top-level 결정 즉시 확정. `material_generation`은 `synthesize_material` 노드에서 DB 트랜잭션으로 직접 처리.
- **process_optimizer** — 리프 라우팅이 아니라 payload의 `operation` 값에 따라 분기 (`graph_edges.py`의 `route_process_optimizer`).
- **AgentRouter 등록 대상**은 리프 7종뿐 (`router.py`). `orchestrator`/`operator_guide`/`quest_generator`는 라우팅 전용이라 registry에 없음.

### 리프 클래스 매핑

| Leaf id | 클래스 | 파일 |
|---|---|---|
| `operator_guide.machine_help` | `MachineHelpAgent` | `operator_guide/machine_help.py` |
| `operator_guide.recipe_explainer` | `RecipeExplainerAgent` | `operator_guide/recipe_explainer.py` |
| `operator_guide.troubleshooter` | `TroubleshooterAgent` | `operator_guide/troubleshooter.py` |
| `quest_generator.production_quest` | `ProductionQuestAgent` | `quest_generator/production_quest.py` |
| `quest_generator.economy_quest` | `EconomyQuestAgent` | `quest_generator/economy_quest.py` |
| `material_generation` | `MaterialCreationAgent` | `material_generation/agent.py` |
| `new_material_generator` | `NewMaterialGeneratorAgent` | `new_material_generator.py` |

---

## 3. 리프 에이전트 개별 구조

operator_guide 계열 3종은 동일한 `ManualQAService` RAG 흐름을 `topic`만 바꿔 공유합니다.

### 3.1 `operator_guide.machine_help` — 장비 설명 (topic=machine)

```mermaid
flowchart TD
    Q["payload.question"] --> CLS["ManualQAQuestionClassifier<br/>(CSV 기반 의도 분류)"]
    CLS -->|모호| LLMCLS["LLMIntentClassifier<br/>(LLM 재분류)"]
    CLS -->|명확| CTX
    LLMCLS --> CTX["ManualQAContextBuilder<br/>(CSV evidence 수집)"]
    CTX --> NEED["ContextNeedClassifier<br/>(게임 상태 필요 여부)"]
    NEED --> STATE["CurrentGameStateTool<br/>(scope별 상태 추출)"]
    STATE --> RAG{"RAG runtime<br/>존재?"}
    RAG -->|Yes| RET["pgvector retrieve<br/>(context_text + metadata)"]
    RAG -->|No| CSVONLY["CSV-only context"]
    RET --> PB["ManualQAPromptBuilder<br/>build / build_messages"]
    CSVONLY --> PB
    PB --> OUT["프롬프트 → 파이프라인 LLM 체인"]
    OUT -.LLM 실패.-> FB["fallback: build_manual_qa_agent_result<br/>(CSV 근거만으로 고정 답변)"]
```

> progress: `rag_search` → `state_check` → `logic_format`

### 3.2 `operator_guide.recipe_explainer` — 레시피 설명 (topic=recipe)

```mermaid
flowchart TD
    Q["payload.question<br/>(예: 철괴 제작법?)"] --> CLS["질문 분류 (CSV)"]
    CLS -->|모호| LLMCLS["LLM 재분류"] --> CTX
    CLS -->|명확| CTX["레시피 evidence 수집<br/>(생산 체인·재료·설비)"]
    CTX --> NEED["게임 상태 필요 판정"]
    NEED --> RAG{"RAG runtime?"}
    RAG -->|Yes| RET["pgvector retrieve"]
    RAG -->|No| CSVONLY["CSV-only"]
    RET --> PB["PromptBuilder (topic=recipe)"]
    CSVONLY --> PB
    PB --> OUT["프롬프트 → LLM 체인"]
    OUT -.실패.-> FB["fallback: CSV 레시피 고정 답변"]
```

> progress: `rag_search` → `state_check` → `logic_format`

### 3.3 `operator_guide.troubleshooter` — 문제 해결 (topic=troubleshooting)

```mermaid
flowchart TD
    Q["payload.question<br/>(예: 기계가 왜 안 돌지?)"] --> CLS["질문 분류 (CSV)"]
    CLS -->|모호| LLMCLS["LLM 재분류"] --> CTX
    CLS -->|명확| CTX["evidence 수집<br/>(에러 원인·점검 절차)"]
    CTX --> NEED["게임 상태 필요 판정"]
    NEED --> STATE["전력·입력 자원 상태 대조<br/>(power_check)"]
    STATE --> RAG{"RAG runtime?"}
    RAG -->|Yes| RET["pgvector retrieve<br/>(document_find)"]
    RAG -->|No| CSVONLY["CSV-only"]
    RET --> PB["PromptBuilder (topic=troubleshooting)"]
    CSVONLY --> PB
    PB --> OUT["점검 순서 정리 → LLM 체인"]
    OUT -.실패.-> FB["fallback: 점검 절차 고정 답변"]
```

> progress(5단계): `rag_search` → `state_check` → `power_check` → `document_find` → `step_arrange` (다른 두 리프보다 진단 단계가 세분화됨)

### 3.4 `quest_generator.production_quest` — 생산 퀘스트 (tool 기반)

```mermaid
flowchart TD
    Q["payload.game_state"] --> SVC["QuestAgentService<br/>available_quest_json()"]
    SVC --> P["build_prompt<br/>(GAME_STATE + AVAILABLE_QUESTS)"]
    P --> LLM1["LLM 1차 호출"]
    LLM1 --> TC{"tool_call<br/>반환?"}
    TC -->|Yes| TOOL["ProductionQuestSelectionTool<br/>selected_quest_ids 5개 검증"]
    TC -->|No| ERR["INVALID_LLM_RESPONSE<br/>(tool 필수)"]
    TOOL --> FUP["tool followup 프롬프트 → LLM 2차"]
    FUP --> OUT["선택된 퀘스트 5개 JSON"]
    OUT -.실패.-> FB["fallback: generate_quest_json()<br/>(예시 생산 퀘스트)"]
```

> 유일하게 `tools`를 가진 리프. 파이프라인이 tool 성공 호출을 강제 검증 (`runtime.py`).

### 3.5 `quest_generator.economy_quest` — 경제 퀘스트 (직접 생성)

```mermaid
flowchart TD
    Q["payload.game_state"] --> P["build_prompt<br/>(경제 퀘스트 JSON 스키마 지시)"]
    P --> LLM["LLM 호출"]
    LLM --> PARSE["parse_llm_response<br/>(quest 객체 JSON)"]
    PARSE --> OUT["quest: type=economy, title, objective"]
    OUT -.실패.-> FB["fallback: 재고 흐름 개선 기본 퀘스트"]
```

> tool 없이 단일 LLM 호출로 JSON 직접 생성 (production_quest와 대비되는 단순 경로).

### 3.6 `material_generation` — 신물질 합성 (DB 서브그래프)

파이프라인 LLM 체인을 타지 않고 `synthesize()`가 자체 LangGraph(`material_subgraph`)를 DB 세션과 함께 실행합니다.

```mermaid
flowchart TD
    START(["synthesize (DB 세션)"]) --> NORM["normalize<br/>(입력 정규화)"]
    NORM --> CACHE["lookup_cache"]
    CACHE -->|hit| END(["END: 응답"])
    CACHE -->|miss| RM["recipe_match"]
    RM -->|매칭| END
    RM -->|미매칭| PV["prevalidate"]
    PV -->|반려| END
    PV -->|통과| CLASSIFY["classify"]
    CLASSIFY --> RULE["handle_rule"]
    RULE -->|규칙 처리됨| END
    RULE -->|LLM 필요| SIM["similarity_context<br/>(유사 사례 수집)"]
    SIM --> DERIVE["derive<br/>(속성 파생)"]
    DERIVE --> PROP["llm_propose<br/>(LLM 신물질 제안)"]
    PROP --> VAL["validate_result"]
    VAL -->|재시도| PROP
    VAL -->|무효| END
    VAL -->|유효| DEDUP["deduplicate_material"]
    DEDUP --> REG["register_material<br/>(DB 등록)"]
    REG --> END
```

> 캐시 → 레시피 매칭 → 사전검증 → 분류 → 규칙 우선, 규칙으로 안 되면 LLM 제안 후 검증·중복제거·DB 등록. LLM은 마지막 수단.

### 3.7 `new_material_generator` — 신소재 후보 생성 (제약 기반)

```mermaid
flowchart TD
    Q["payload<br/>(goal·목표 속성·제약)"] --> P["build_prompt<br/>(materials 배열 JSON 스키마)"]
    P --> LLM["LLM 호출"]
    LLM --> PARSE["parse_llm_response"]
    PARSE --> OUT["materials[]<br/>(name·role·rarity·Unreal 컬럼)"]
    OUT -.실패.-> FB["fallback: Composite Catalyst 1종<br/>+ get_unreal_material_column_values()"]
```

> `material_generation`(구체 레시피→단일 합성, DB)와 달리 **구체 레시피 없이 제약만으로 후보 목록**을 LLM 생성. DB 미접근.

---

## 4. 리프별 아키텍처 유형 요약

| 리프 | 추론 방식 | 특징 |
|---|---|---|
| machine_help / recipe_explainer / troubleshooter | RAG + LLM | 공유 `ManualQAService`, `topic`만 상이. troubleshooter만 진단 progress 5단계 |
| production_quest | LLM + Tool | 유일한 tool 보유, tool 호출 강제 |
| economy_quest | LLM 단독 | JSON 직접 생성 |
| material_generation | Rule→LLM 하이브리드 + DB | 자체 서브그래프, LLM은 최후 수단 |
| new_material_generator | LLM 단독 | 제약 기반 후보 생성, DB 미접근 |

세 가지 추론 패턴(RAG / LLM / 규칙-하이브리드)이 동일한 `Agent` 계약(`build_prompt` / `fallback`) 뒤에 통일돼 있어, 파이프라인은 리프의 내부 방식을 몰라도 됩니다. 이것이 `backend/AGENTS.md`의 "추론 방식과 목적 분리" 원칙이 구현된 모습입니다.
