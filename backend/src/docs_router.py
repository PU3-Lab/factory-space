"""HTML documentation pages for quest agent explanation."""

from __future__ import annotations

import json
from html import escape
from typing import Any

from fastapi import APIRouter
from fastapi.responses import HTMLResponse

router = APIRouter(tags=["docs"])

QUEST_AGENTS: tuple[dict[str, Any], ...] = (
    {
        "id": "quest_generator.production_quest",
        "name": "생산 퀘스트 에이전트",
        "summary": "채굴, 제작, 생산량 증가처럼 공장 성장에 직접 연결되는 목표를 퀘스트로 만듭니다.",
        "example": {
            "input": "철광석을 모으고 첫 생산 라인을 돌리게 하는 퀘스트가 필요해.",
            "routing": "quest_generator -> quest_generator.production_quest",
            "tools": "build_prompt, LLM 호출, QuestAgentService.generate_quest_json fallback",
            "response": "생산 목표를 JSON 퀘스트로 생성하고, LLM이 없으면 검증된 예시 퀘스트 풀에서 5개를 반환합니다.",
        },
        "tools": (
            {
                "name": "ProductionQuestAgent.build_prompt",
                "input": "생산 목표 payload, AgentContext",
                "output": "생산 퀘스트 생성을 위한 LLM 프롬프트",
                "meaning": "막연한 생산 목표를 채굴/제작/수량 중심의 퀘스트 지시로 바꿉니다.",
            },
            {
                "name": "QuestAgentService.generate_quest_json",
                "input": "count 기본값 5, 서버 예시 퀘스트 풀",
                "output": "QuestResponse 스키마를 통과한 quests JSON",
                "meaning": "LLM 없이도 데모와 테스트에서 안정적으로 생산 퀘스트를 보여주는 안전망입니다.",
            },
            {
                "name": "cache_lookup / cache_write",
                "input": "선택 agent, leaf agent, payload, context",
                "output": "캐시된 응답 또는 새 응답 저장",
                "meaning": "동일한 요청의 반복 처리 비용을 줄이고 응답 일관성을 높입니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["quest_generator"]
    B --> C["production_quest 선택"]
    C --> D["캐시 확인"]
    D -->|miss| E["ProductionQuestAgent.build_prompt"]
    D -->|hit| I["캐시 응답"]
    E --> F{"LLM 사용 가능?"}
    F -->|예| G["JSON 파싱 및 스키마 확인"]
    F -->|아니오| H["QuestAgentService.generate_quest_json"]
    G --> J["최종 생산 퀘스트"]
    H --> J
    I --> J
""",
    },
    {
        "id": "quest_generator.economy_quest",
        "name": "경제 퀘스트 에이전트",
        "summary": "재고 과잉, 부족, 보상, 생산 효율 같은 경제 흐름을 개선하는 퀘스트를 만듭니다.",
        "example": {
            "input": "창고에 쌓인 자원을 줄이도록 유도하는 퀘스트가 필요해.",
            "routing": "quest_generator -> quest_generator.economy_quest",
            "tools": "build_prompt, LLM 호출, fallback",
            "response": "재고 흐름 개선 목표를 만들고, 실패 시 기본 재고 개선 퀘스트를 반환합니다.",
        },
        "tools": (
            {
                "name": "EconomyQuestAgent.build_prompt",
                "input": "경제/재고 요청 payload, AgentContext",
                "output": "경제 퀘스트 생성을 위한 LLM 프롬프트",
                "meaning": "재고 문제를 플레이어가 수행할 수 있는 경제 목표로 바꿉니다.",
            },
            {
                "name": "EconomyQuestAgent.fallback",
                "input": "payload, AgentContext",
                "output": "기본 경제 퀘스트 JSON",
                "meaning": "경제 밸런스 설명이 필요한 상황에서도 최소 응답을 보장합니다.",
            },
            {
                "name": "build_agent_response",
                "input": "responsePayload, responseMetadata, envelope",
                "output": "AgentResponseEnvelope JSON",
                "meaning": "생성된 퀘스트를 Unreal이 이해하는 공통 응답 봉투로 포장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["quest_generator"]
    B --> C["economy_quest 선택"]
    C --> D["EconomyQuestAgent.build_prompt"]
    D --> E{"LLM JSON 응답?"}
    E -->|성공| F["경제 퀘스트 JSON"]
    E -->|실패| G["EconomyQuestAgent.fallback"]
    G --> F
    F --> H["AgentResponseEnvelope 생성"]
""",
    },
)


MIDDLEWARE_ROWS = (
    (
        "append_middleware_log",
        "agent.middleware.before/after/fallback 이벤트를 상태 metadata에 누적합니다.",
        "AgentGraphState, node, event, details",
        "middlewareLogs가 추가된 AgentGraphState",
    ),
    (
        "build_current_model_metadata",
        "마지막으로 사용한 LLM slot/provider/model 정보를 응답 metadata로 정리합니다.",
        "AgentGraphState",
        "현재 모델 metadata 또는 None",
    ),
    (
        "build_tool_node_input",
        "LLM이 반환한 provider-neutral tool_call JSON을 LangGraph ToolNode 입력 메시지로 변환합니다.",
        "AgentRouter",
        "AgentGraphState를 받아 messages/toolCallRequest를 반환하는 함수",
    ),
)

CONCEPT_ROWS = (
    (
        "create_agent",
        "backend/src/agents/pipeline/runtime.py: AgentPipeline._build_graph",
        "현재 코드는 top-level 라우팅, leaf 라우팅, 캐시, fallback, WebSocket 응답 봉투가 필요해 단일 create_agent보다 명시적인 factory가 흐름을 설명하기 쉽습니다.",
    ),
    (
        "@tool",
        "backend/src/agents/pipeline/tool_node.py: _wrap_agent_tool, build_agent_tool_node",
        "각 agent가 가진 read-only tool을 공통 AgentTool 계약으로 먼저 정의한 뒤 StructuredTool로 감싸 provider와 agent 구현을 분리합니다.",
    ),
    (
        "StateGraph",
        "backend/src/agents/pipeline/runtime.py: graph = StateGraph(AgentGraphState)",
        "요청 처리 상태를 TypedDict로 공유하면서 라우팅, LLM, fallback, 응답 생성을 단계별 노드로 표현합니다.",
    ),
    (
        "add_node",
        "backend/src/agents/pipeline/runtime.py: graph.add_node(...)",
        "각 처리 단계를 내부 함수로 정의하고 이름 붙인 노드로 등록해 테스트와 설명에서 같은 이름을 사용할 수 있습니다.",
    ),
    (
        "add_edge",
        "backend/src/agents/pipeline/graph_edges.py: wire_agent_graph",
        "조건부 edge가 많아 runtime에서 분리했습니다. 그래프 구조 변경이 실행 로직 변경과 섞이지 않습니다.",
    ),
    (
        "before_model",
        "backend/src/agents/pipeline/runtime.py: agent.middleware.before, build_prompt",
        "LangChain middleware 훅 대신 그래프 노드로 모델 호출 전 로그와 prompt 생성을 명시해 WebSocket metadata에 남깁니다.",
    ),
)

ALL_SERVICE_AGENTS: tuple[dict[str, Any], ...] = (
    {
        "id": "process_optimizer",
        "name": "공정 최적화 에이전트",
        "summary": "공장 snapshot을 받아 설비 병목과 개선 우선순위를 JSON으로 반환합니다.",
        "parent": None,
        "example": {
            "input": "철강 라인 처리량이 낮아. 뭘 먼저 개선해야 할까?",
            "routing": "orchestrator → process_optimizer",
            "tools": "build_prompt, LLM 호출, fallback",
            "response": "설비 병목 분석 결과와 high/medium/low 우선순위 recommendations JSON을 반환합니다.",
        },
        "tools": (
            {
                "name": "ProcessOptimizerAgent.build_prompt",
                "input": "공장 snapshot payload, AgentContext",
                "output": "병목 분석을 위한 LLM 프롬프트 문자열",
                "meaning": "공장 상태를 LLM이 이해할 수 있는 분석 지시로 변환합니다.",
            },
            {
                "name": "ProcessOptimizerAgent.fallback",
                "input": "payload (machines 목록 포함), AgentContext",
                "output": "AgentRunResult (병목 점검 권고 JSON)",
                "meaning": "LLM 없이도 기본 설비 점검 권고를 보장하는 안전망입니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["process_optimizer 선택"]
    C --> D["캐시 확인"]
    D -->|miss| E["build_prompt"]
    D -->|hit| I["캐시 응답"]
    E --> F{"LLM 사용 가능?"}
    F -->|예| G["JSON 파싱 및 스키마 확인"]
    F -->|아니오| H["fallback: 기본 병목 점검"]
    G --> J["공정 개선 권고 JSON"]
    H --> J
    I --> J
""",
    },
    {
        "id": "operator_guide.machine_help",
        "name": "설비 도움말 에이전트",
        "summary": "설비 상태·조작법·UI 컨텍스트 질문에 답하고 단계별 안내를 제공합니다.",
        "parent": "operator_guide",
        "example": {
            "input": "컨베이어 벨트 속도를 어떻게 조절하나요?",
            "routing": "orchestrator → operator_guide → operator_guide.machine_help",
            "tools": "route_operator_guide_sub_agent, build_prompt, LLM 호출, fallback",
            "response": "설비 조작 방법을 topic=machine JSON으로 반환합니다.",
        },
        "tools": (
            {
                "name": "MachineHelpAgent.build_prompt",
                "input": "설비 관련 질문 payload, AgentContext",
                "output": "설비 안내를 위한 LLM 프롬프트 문자열",
                "meaning": "설비 질문을 구조화된 JSON 답변 지시로 바꿉니다.",
            },
            {
                "name": "MachineHelpAgent.fallback",
                "input": "payload (question 포함), AgentContext",
                "output": "AgentRunResult (설비 점검 기본 안내 JSON)",
                "meaning": "LLM이 없을 때 상태값·연결·레시피 확인 기본 가이드를 보장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["operator_guide 선택"]
    C --> D["sub-agent 라우팅"]
    D --> E["machine_help 선택"]
    E --> F["캐시 확인"]
    F -->|miss| G["build_prompt"]
    F -->|hit| K["캐시 응답"]
    G --> H{"LLM 사용 가능?"}
    H -->|예| I["JSON 파싱"]
    H -->|아니오| J["fallback: 설비 점검 안내"]
    I --> L["설비 도움말 JSON"]
    J --> L
    K --> L
""",
    },
    {
        "id": "operator_guide.recipe_explainer",
        "name": "레시피 설명 에이전트",
        "summary": "레시피 재료·생산 체인·선행 조건을 단계별로 설명합니다.",
        "parent": "operator_guide",
        "example": {
            "input": "강화 합금 레시피가 뭐야?",
            "routing": "orchestrator → operator_guide → operator_guide.recipe_explainer",
            "tools": "route_operator_guide_sub_agent, build_prompt, LLM 호출, fallback",
            "response": "레시피 입력·출력·선행 조건을 topic=recipe JSON으로 반환합니다.",
        },
        "tools": (
            {
                "name": "RecipeExplainerAgent.build_prompt",
                "input": "레시피 질문 payload, AgentContext",
                "output": "레시피 설명을 위한 LLM 프롬프트 문자열",
                "meaning": "레시피 질문을 재료·결과·선행 조건 중심의 답변 지시로 바꿉니다.",
            },
            {
                "name": "RecipeExplainerAgent.fallback",
                "input": "payload (question 포함), AgentContext",
                "output": "AgentRunResult (레시피 확인 기본 안내 JSON)",
                "meaning": "LLM이 없을 때도 재료→결과 확인 절차를 보장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["operator_guide 선택"]
    C --> D["sub-agent 라우팅"]
    D --> E["recipe_explainer 선택"]
    E --> F["캐시 확인"]
    F -->|miss| G["build_prompt"]
    F -->|hit| K["캐시 응답"]
    G --> H{"LLM 사용 가능?"}
    H -->|예| I["JSON 파싱"]
    H -->|아니오| J["fallback: 레시피 확인 안내"]
    I --> L["레시피 설명 JSON"]
    J --> L
    K --> L
""",
    },
    {
        "id": "operator_guide.troubleshooter",
        "name": "트러블슈팅 에이전트",
        "summary": "생산 중단이나 설비 오류 원인을 진단하고 해결 단계를 제안합니다.",
        "parent": "operator_guide",
        "example": {
            "input": "철광석 제련기가 갑자기 멈췄어.",
            "routing": "orchestrator → operator_guide → operator_guide.troubleshooter",
            "tools": "route_operator_guide_sub_agent, build_prompt, LLM 호출, fallback",
            "response": "입력 부족·출력 포화·전력 상태를 순서대로 점검하는 진단 결과 JSON을 반환합니다.",
        },
        "tools": (
            {
                "name": "TroubleshooterAgent.build_prompt",
                "input": "오류 상황 payload, AgentContext",
                "output": "문제 진단을 위한 LLM 프롬프트 문자열",
                "meaning": "현장 오류 상황을 원인 추론과 해결 순서 중심으로 LLM에게 전달합니다.",
            },
            {
                "name": "TroubleshooterAgent.fallback",
                "input": "payload (question 포함), AgentContext",
                "output": "AgentRunResult (기본 점검 순서 안내 JSON)",
                "meaning": "LLM이 없을 때도 재료·저장소·전력·설비 정지 체크리스트를 보장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["operator_guide 선택"]
    C --> D["sub-agent 라우팅"]
    D --> E["troubleshooter 선택"]
    E --> F["캐시 확인"]
    F -->|miss| G["build_prompt"]
    F -->|hit| K["캐시 응답"]
    G --> H{"LLM 사용 가능?"}
    H -->|예| I["JSON 파싱"]
    H -->|아니오| J["fallback: 기본 점검 순서"]
    I --> L["트러블슈팅 진단 JSON"]
    J --> L
    K --> L
""",
    },
    {
        "id": "quest_generator.production_quest",
        "name": "생산 퀘스트 에이전트",
        "summary": "채굴·제작·생산량 증가처럼 공장 성장에 직결되는 목표를 퀘스트 JSON으로 만듭니다.",
        "parent": "quest_generator",
        "example": {
            "input": "철광석을 모아 첫 생산 라인을 가동하게 하는 퀘스트를 만들어줘.",
            "routing": "orchestrator → quest_generator → quest_generator.production_quest",
            "tools": "build_prompt, ProductionQuestSelectionTool, LLM tool_call 흐름, fallback",
            "response": "LLM이 tool_call로 퀘스트 id 5개를 선택하고 ProductionQuestSelectionTool이 JSON을 만듭니다. 실패 시 예시 퀘스트 풀에서 5개를 반환합니다.",
        },
        "tools": (
            {
                "name": "ProductionQuestAgent.build_prompt",
                "input": "생산 목표 payload, AgentContext",
                "output": "퀘스트 id 선택을 위한 LLM tool_call 프롬프트",
                "meaning": "LLM이 기존 퀘스트 풀에서 가장 적합한 5개를 고를 수 있도록 지시합니다.",
            },
            {
                "name": "ProductionQuestSelectionTool.invoke",
                "input": "selected_quest_ids: list[int] (LLM이 고른 퀘스트 id 목록)",
                "output": "선택된 퀘스트 JSON (QuestAgentService 경유)",
                "meaning": "LLM이 고른 id를 검증해 실제 퀘스트 데이터로 바꾸는 핵심 도구입니다.",
            },
            {
                "name": "ProductionQuestAgent.fallback",
                "input": "payload, AgentContext",
                "output": "AgentRunResult (예시 퀘스트 5개 JSON)",
                "meaning": "LLM 없이도 항상 5개의 생산 퀘스트를 보장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["quest_generator 선택"]
    C --> D["production_quest 선택"]
    D --> E["캐시 확인"]
    E -->|hit| M["캐시 응답"]
    E -->|miss| F["build_prompt"]
    F --> G["LLM → tool_call JSON"]
    G --> H["ProductionQuestSelectionTool 실행"]
    H --> I["tool_followup_prompt 생성"]
    I --> J["LLM 최종 JSON 응답"]
    J --> K{"스키마 검증"}
    K -->|실패| L["fallback: 예시 퀘스트 5개"]
    K -->|성공| N["생산 퀘스트 JSON"]
    L --> N
    M --> N
""",
    },
    {
        "id": "quest_generator.economy_quest",
        "name": "경제 퀘스트 에이전트",
        "summary": "재고 과잉·부족·보상·생산 효율 같은 경제 흐름을 개선하는 퀘스트를 만듭니다.",
        "parent": "quest_generator",
        "example": {
            "input": "창고에 쌓인 자원을 줄이는 퀘스트가 필요해.",
            "routing": "orchestrator → quest_generator → quest_generator.economy_quest",
            "tools": "build_prompt, LLM 호출, fallback",
            "response": "재고 흐름 개선 목표를 type=economy JSON 퀘스트로 반환합니다. 실패 시 기본 재고 개선 퀘스트를 반환합니다.",
        },
        "tools": (
            {
                "name": "EconomyQuestAgent.build_prompt",
                "input": "경제/재고 요청 payload, AgentContext",
                "output": "경제 퀘스트 생성을 위한 LLM 프롬프트 문자열",
                "meaning": "재고 문제를 플레이어가 수행할 수 있는 경제 목표로 바꿉니다.",
            },
            {
                "name": "EconomyQuestAgent.fallback",
                "input": "payload, AgentContext",
                "output": "AgentRunResult (기본 경제 퀘스트 JSON)",
                "meaning": "경제 밸런스 설명이 필요한 상황에서도 최소 응답을 보장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["quest_generator 선택"]
    C --> D["economy_quest 선택"]
    D --> E["EconomyQuestAgent.build_prompt"]
    E --> F{"LLM JSON 응답?"}
    F -->|성공| G["경제 퀘스트 JSON"]
    F -->|실패| H["fallback: 기본 경제 퀘스트"]
    H --> G
    G --> I["AgentResponseEnvelope 생성"]
""",
    },
    {
        "id": "new_material_generator",
        "name": "신소재 생성 에이전트",
        "summary": "설계 제약(목표·희귀도·역할)을 바탕으로 공장에서 사용할 신소재 후보를 생성합니다.",
        "parent": None,
        "example": {
            "input": "내열성이 높은 희귀 소재 아이디어가 필요해.",
            "routing": "orchestrator → new_material_generator",
            "tools": "build_prompt, LLM 호출, fallback",
            "response": "name·role·rarity·production_notes 가 담긴 materials 배열 JSON을 반환합니다.",
        },
        "tools": (
            {
                "name": "NewMaterialGeneratorAgent.build_prompt",
                "input": "소재 설계 제약 payload, AgentContext",
                "output": "신소재 후보 생성을 위한 LLM 프롬프트 문자열",
                "meaning": "목표·속성·희귀도 요청을 LLM이 이해하는 소재 생성 지시로 변환합니다.",
            },
            {
                "name": "NewMaterialGeneratorAgent.fallback",
                "input": "payload (goal 포함), AgentContext",
                "output": "AgentRunResult (Composite Catalyst 기본 후보 JSON)",
                "meaning": "LLM이 없을 때도 기본 신소재 후보를 항상 보장합니다.",
            },
        ),
        "graph": """
flowchart TD
    A["사용자 요청"] --> B["Orchestrator"]
    B --> C["new_material_generator 선택"]
    C --> D["캐시 확인"]
    D -->|miss| E["build_prompt"]
    D -->|hit| I["캐시 응답"]
    E --> F{"LLM 사용 가능?"}
    F -->|예| G["JSON 파싱 및 스키마 확인"]
    F -->|아니오| H["fallback: Composite Catalyst"]
    G --> J["신소재 후보 JSON"]
    H --> J
    I --> J
""",
    },
)


@router.get("/quest-agent-docs", response_class=HTMLResponse)
async def quest_agent_docs() -> HTMLResponse:
    """Return a service-oriented quest agent explanation page."""

    return HTMLResponse(
        _render_page("퀘스트 에이전트 서비스 문서", _render_docs_body())
    )


@router.get("/quest-agent-architecture", response_class=HTMLResponse)
async def quest_agent_architecture() -> HTMLResponse:
    """Return a code-oriented quest agent architecture page."""

    return HTMLResponse(
        _render_page("퀘스트 에이전트 아키텍처", _render_architecture_body())
    )


@router.get("/agent-docs", response_class=HTMLResponse)
async def agent_docs() -> HTMLResponse:
    """Return a service-oriented overview of all agents (for non-developers)."""

    return HTMLResponse(
        _render_page("에이전트 서비스 문서", _render_all_agent_docs_body())
    )


@router.get("/architecture", response_class=HTMLResponse)
async def architecture() -> HTMLResponse:
    """Return a developer-oriented architecture page covering all agents."""

    return HTMLResponse(
        _render_page("전체 에이전트 아키텍처", _render_all_architecture_body())
    )


@router.get("/agent-test", response_class=HTMLResponse)
async def agent_test() -> HTMLResponse:
    """Return an interactive WebSocket agent test console."""

    return HTMLResponse(_render_test_page())


def _render_page(title: str, body: str) -> str:
    return f"""<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escape(title)}</title>
  <script src="https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.min.js"></script>
  <script>mermaid.initialize({{ startOnLoad: true, theme: "neutral" }});</script>
  <style>
    :root {{
      color-scheme: light;
      --bg: #f7f8fb;
      --panel: #ffffff;
      --ink: #18202f;
      --muted: #5e6a7d;
      --line: #d9deea;
      --accent: #0f766e;
      --accent-soft: #dff5f0;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: var(--bg);
      color: var(--ink);
      font-family: Arial, "Noto Sans KR", sans-serif;
      line-height: 1.65;
    }}
    main {{ max-width: 1180px; margin: 0 auto; padding: 40px 22px 72px; }}
    header {{ margin-bottom: 28px; }}
    h1 {{ margin: 0 0 10px; font-size: clamp(30px, 4vw, 46px); line-height: 1.18; }}
    h2 {{ margin: 0 0 14px; font-size: 26px; }}
    h3 {{ margin: 22px 0 10px; font-size: 20px; }}
    p {{ margin: 0 0 14px; color: var(--muted); }}
    code {{ font-family: Consolas, "Liberation Mono", monospace; color: #0f4f4a; }}
    .nav {{ display: flex; gap: 10px; flex-wrap: wrap; margin-top: 18px; }}
    .nav a {{
      color: var(--accent);
      background: var(--accent-soft);
      border: 1px solid #b8e4dc;
      border-radius: 6px;
      padding: 8px 12px;
      text-decoration: none;
      font-weight: 700;
    }}
    .panel {{
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 24px;
      margin: 18px 0;
      box-shadow: 0 8px 24px rgba(24, 32, 47, 0.05);
    }}
    .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }}
    .badge {{ display: inline-block; color: var(--accent); font-weight: 700; margin-bottom: 8px; }}
    table {{ width: 100%; border-collapse: collapse; margin: 12px 0 20px; background: #fff; }}
    th, td {{ border: 1px solid var(--line); padding: 10px 12px; vertical-align: top; text-align: left; }}
    th {{ background: #eef3f6; color: #243145; }}
    .mermaid {{
      background: #fbfcfe;
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 16px;
      overflow-x: auto;
    }}
    .workflow {{ margin: 0; padding-left: 20px; }}
  </style>
</head>
<body>
  <main>
    <header>
      <h1>{escape(title)}</h1>
      <p>Factory Space 백엔드의 퀘스트 생성 흐름을 설명하기 위한 HTMLResponse 기반 문서 페이지입니다.</p>
      <nav class="nav">
        <a href="/agent-docs">전체 에이전트 서비스 문서</a>
        <a href="/architecture">전체 에이전트 아키텍처</a>
        <a href="/agent-test">테스트 콘솔</a>
        <a href="/quest-agent-docs">퀘스트 서비스 문서</a>
        <a href="/quest-agent-architecture">퀘스트 아키텍처</a>
        <a href="/health">헬스 체크</a>
      </nav>
    </header>
    {body}
  </main>
</body>
</html>"""


def _render_docs_body() -> str:
    agents = "\n".join(_render_service_agent(agent) for agent in QUEST_AGENTS)
    return f"""
<section class="panel">
  <h2>서비스 관점 요약</h2>
  <p>퀘스트 에이전트는 사용자의 자연어 요청을 생산 또는 경제 퀘스트로 라우팅한 뒤, LLM 또는 deterministic fallback으로 Unreal 클라이언트가 받을 퀘스트 JSON을 만듭니다.</p>
</section>
{agents}
"""


def _render_service_agent(agent: dict[str, Any]) -> str:
    return f"""
<section class="panel">
  <span class="badge">{escape(agent["id"])}</span>
  <h2>{escape(agent["name"])}</h2>
  <p>{escape(agent["summary"])}</p>
  <h3>관련 툴 목록</h3>
  {_render_table(("툴 이름", "input", "output", "서비스에서 가지는 의미"), agent["tools"], ("name", "input", "output", "meaning"))}
  <h3>요청 처리 흐름</h3>
  <pre class="mermaid">{escape(agent["graph"].strip())}</pre>
  <h3>워크플로우 예시</h3>
  {_render_workflow(agent["example"])}
</section>
"""


def _render_workflow(example: dict[str, str]) -> str:
    return f"""
<ol class="workflow">
  <li><strong>사용자 입력 예시:</strong> {escape(example["input"])}</li>
  <li><strong>라우팅 결과:</strong> <code>{escape(example["routing"])}</code></li>
  <li><strong>호출되는 Tool:</strong> {escape(example["tools"])}</li>
  <li><strong>최종 응답 생성 과정:</strong> {escape(example["response"])}</li>
</ol>
"""


def _render_architecture_body() -> str:
    agent_rows = tuple(
        {
            "name": agent["name"],
            "id": agent["id"],
            "summary": agent["summary"],
        }
        for agent in QUEST_AGENTS
    )
    tool_rows = tuple(tool for agent in QUEST_AGENTS for tool in agent["tools"])
    return f"""
<section class="panel">
  <h2>에이전트 표</h2>
  {_render_table(("에이전트 이름", "agent_id", "한 줄 설명"), agent_rows, ("name", "id", "summary"))}
</section>
<section class="panel">
  <h2>관련 툴 표</h2>
  {_render_table(("함수명", "설명", "input", "output"), tool_rows, ("name", "meaning", "input", "output"))}
</section>
<section class="panel">
  <h2>미들웨어 표</h2>
  {_render_tuple_table(("함수명", "설명", "input", "output"), MIDDLEWARE_ROWS)}
</section>
<section class="panel">
  <h2>LangChain/LangGraph 개념 매핑</h2>
  {_render_tuple_table(("내가 배운 개념", "현재 코드에서 해당 역할을 하는 파일/함수", "왜 wrapper/factory/custom function으로 구현했는지"), CONCEPT_ROWS)}
</section>
"""


def _render_table(
    headers: tuple[str, ...],
    rows: tuple[dict[str, Any], ...],
    keys: tuple[str, ...],
) -> str:
    head = "".join(f"<th>{escape(header)}</th>" for header in headers)
    body = "".join(
        "<tr>" + "".join(f"<td>{escape(str(row[key]))}</td>" for key in keys) + "</tr>"
        for row in rows
    )
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"


def _render_tuple_table(
    headers: tuple[str, ...], rows: tuple[tuple[str, ...], ...]
) -> str:
    head = "".join(f"<th>{escape(header)}</th>" for header in headers)
    body = "".join(
        "<tr>" + "".join(f"<td>{escape(value)}</td>" for value in row) + "</tr>"
        for row in rows
    )
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"


# ── /agent-docs 렌더러 ────────────────────────────────────────────────────────


def _render_all_agent_docs_body() -> str:
    top_level = [a for a in ALL_SERVICE_AGENTS if a["parent"] is None]
    grouped: dict[str, list[dict[str, Any]]] = {}
    for agent in ALL_SERVICE_AGENTS:
        if agent["parent"] is not None:
            grouped.setdefault(agent["parent"], []).append(agent)

    sections = []
    for agent in top_level:
        sections.append(_render_service_agent_card(agent))

    for group_name, children in grouped.items():
        group_label = {
            "operator_guide": "운영자 가이드 에이전트 그룹",
            "quest_generator": "퀘스트 생성 에이전트 그룹",
        }.get(group_name, group_name)
        child_cards = "\n".join(_render_service_agent_card(a) for a in children)
        sections.append(f"""
<section class="panel">
  <h2>{escape(group_label)}</h2>
  <p>사용자 요청이 <code>{escape(group_name)}</code> 그룹으로 라우팅된 뒤 아래 에이전트 중 하나가 처리합니다.</p>
  {child_cards}
</section>""")

    return f"""
<section class="panel">
  <h2>서비스 개요</h2>
  <p>Factory Space 백엔드는 Orchestrator가 사용자의 자연어 요청을 분석해 가장 적합한 에이전트로 라우팅합니다.
  모든 에이전트는 LLM 결과를 우선 사용하고, 실패 시 deterministic fallback으로 항상 응답을 보장합니다.</p>
</section>
{"".join(sections)}
"""


def _render_service_agent_card(agent: dict[str, Any]) -> str:
    parent_badge = ""
    if agent["parent"]:
        parent_badge = f' <span style="font-size:13px;color:#5e6a7d;">({escape(agent["parent"])} 그룹)</span>'
    return f"""
<section class="panel" style="margin-left: {"24px" if agent["parent"] else "0"}">
  <span class="badge">{escape(agent["id"])}</span>{parent_badge}
  <h2>{escape(agent["name"])}</h2>
  <p>{escape(agent["summary"])}</p>
  <h3>관련 툴 목록</h3>
  {_render_table(("툴 이름", "input", "output", "서비스에서 가지는 의미"), agent["tools"], ("name", "input", "output", "meaning"))}
  <h3>요청 처리 흐름</h3>
  <pre class="mermaid">{escape(agent["graph"].strip())}</pre>
  <h3>워크플로우 예시</h3>
  {_render_workflow(agent["example"])}
</section>
"""


# ── /architecture 렌더러 ─────────────────────────────────────────────────────


def _render_all_architecture_body() -> str:
    sections = []
    for agent in ALL_SERVICE_AGENTS:
        parent_note = (
            f"<p><strong>소속 그룹:</strong> <code>{escape(agent['parent'])}</code></p>"
            if agent["parent"]
            else ""
        )
        sections.append(f"""
<section class="panel">
  <span class="badge">{escape(agent["id"])}</span>
  <h2>{escape(agent["name"])}</h2>
  <p>{escape(agent["summary"])}</p>
  {parent_note}
  <h3>관련 툴 표</h3>
  {_render_table(("함수명", "설명", "input", "output"), agent["tools"], ("name", "meaning", "input", "output"))}
</section>""")

    return f"""
<section class="panel">
  <h2>공통 미들웨어</h2>
  <p>모든 에이전트 실행 경로에서 공통으로 적용되는 파이프라인 미들웨어입니다.</p>
  {_render_tuple_table(("함수명", "설명", "input", "output"), MIDDLEWARE_ROWS)}
</section>
<section class="panel">
  <h2>LangChain/LangGraph 개념 매핑</h2>
  <p>LangChain·LangGraph 표준 추상화 대신 현재 코드가 선택한 구현 방식과 그 이유입니다.</p>
  {_render_tuple_table(("내가 배운 개념", "현재 코드에서 해당 역할을 하는 파일/함수", "왜 wrapper/factory/custom function으로 구현했는지"), CONCEPT_ROWS)}
</section>
{"".join(sections)}
"""


# ── /agent-test 렌더러 ───────────────────────────────────────────────────────

_TEST_PAGE_TEMPLATE = """<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>에이전트 테스트 콘솔</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f7f8fb; --panel: #fff; --ink: #18202f; --muted: #5e6a7d;
      --line: #d9deea; --accent: #0f766e; --accent-soft: #dff5f0;
    }
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--bg); color: var(--ink);
           font-family: Arial, "Noto Sans KR", sans-serif; line-height: 1.65; }
    main { max-width: 1280px; margin: 0 auto; padding: 36px 22px 80px; }
    header { margin-bottom: 24px; }
    h1 { margin: 0 0 8px; font-size: clamp(28px,4vw,42px); line-height: 1.2; }
    h2 { margin: 0 0 14px; font-size: 20px; }
    p { margin: 0 0 12px; color: var(--muted); }
    .nav { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 16px; }
    .nav a { color: var(--accent); background: var(--accent-soft); border: 1px solid #b8e4dc;
             border-radius: 6px; padding: 8px 12px; text-decoration: none; font-weight: 700; font-size: 14px; }
    .nav a.active { background: var(--accent); color: #fff; border-color: var(--accent); }
    .panel { background: var(--panel); border: 1px solid var(--line); border-radius: 8px;
             padding: 22px; box-shadow: 0 4px 16px rgba(24,32,47,.05); }
    .status-bar { display: flex; align-items: center; gap: 10px; margin-bottom: 16px;
                  padding: 12px 16px; background: var(--panel);
                  border: 1px solid var(--line); border-radius: 8px; flex-wrap: wrap; }
    .dot { width: 10px; height: 10px; border-radius: 50%; background: #9ca3af;
           flex-shrink: 0; transition: background .2s; }
    .dot.connecting { background: #f59e0b; animation: blink .9s infinite; }
    .dot.connected  { background: #10b981; }
    .dot.error      { background: #ef4444; }
    @keyframes blink { 0%,100%{opacity:1} 50%{opacity:.3} }
    #ws-label { font-size: 13px; font-weight: 600; color: var(--muted); min-width: 72px; }
    #ws-url { flex: 1; min-width: 200px; font-family: monospace; font-size: 13px;
              padding: 6px 10px; border: 1px solid var(--line); border-radius: 5px;
              background: #f9fafb; color: var(--ink); }
    .btn { cursor: pointer; border: none; border-radius: 6px; padding: 8px 16px;
           font-size: 14px; font-weight: 600; transition: opacity .15s; }
    .btn:disabled { opacity: .4; cursor: not-allowed; }
    .btn-primary { background: var(--accent); color: #fff; }
    .btn-primary:hover:not(:disabled) { opacity: .82; }
    .btn-ghost { background: #e5e7eb; color: var(--ink); }
    .btn-ghost:hover:not(:disabled) { background: #d1d5db; }
    .btn-sm { padding: 5px 10px; font-size: 12px; }
    .console-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 18px; }
    @media (max-width: 860px) { .console-grid { grid-template-columns: 1fr; } }
    /* ── 요청 설정 ── */
    details.req-settings { margin-bottom: 12px; border: 1px solid var(--line); border-radius: 6px; }
    details.req-settings > summary {
      padding: 8px 12px; cursor: pointer; font-size: 13px; font-weight: 700;
      color: var(--muted); list-style: none; user-select: none;
      display: flex; align-items: center; gap: 6px;
    }
    details.req-settings > summary::before { content: '\\25B6'; font-size: 10px; transition: transform .15s; }
    details.req-settings[open] > summary::before { transform: rotate(90deg); }
    .settings-body { padding: 12px; display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    @media (max-width: 600px) { .settings-body { grid-template-columns: 1fr; } }
    .field { display: flex; flex-direction: column; gap: 4px; }
    .field.full { grid-column: 1 / -1; }
    .field label { font-size: 12px; font-weight: 700; color: var(--muted); }
    .field label span { font-weight: 400; }
    .field input, .field select, .field textarea {
      padding: 6px 8px; font-size: 12px; border: 1px solid var(--line); border-radius: 4px;
      background: #f9fafb; color: var(--ink); font-family: inherit;
    }
    .field textarea { resize: vertical; }
    .temp-row { display: flex; align-items: center; gap: 6px; }
    .temp-row input[type=range] { flex: 1; accent-color: var(--accent); }
    .temp-row span { font-size: 12px; color: var(--muted); min-width: 14px; }
    /* ── 에디터 ── */
    .preset-row { display: flex; align-items: center; gap: 8px; margin-bottom: 10px; flex-wrap: wrap; }
    .preset-row label { font-size: 13px; color: var(--muted); font-weight: 600; }
    #preset-select { padding: 6px 10px; font-size: 13px; border: 1px solid var(--line);
                     border-radius: 5px; background: #f9fafb; color: var(--ink); cursor: pointer; }
    #editor { width: 100%; height: 240px; border: 1px solid var(--line); border-radius: 6px;
              padding: 12px; resize: vertical; font-size: 12.5px;
              font-family: Consolas, "Liberation Mono", monospace;
              background: #fafafa; color: var(--ink); line-height: 1.55; }
    #editor:focus { outline: 2px solid var(--accent); border-color: transparent; }
    .send-row { display: flex; gap: 8px; margin-top: 10px; align-items: center; flex-wrap: wrap; }
    #tag-input { flex: 1; min-width: 60px; max-width: 140px; padding: 6px 8px; font-size: 12px;
                 border: 1px solid var(--line); border-radius: 5px;
                 background: #f9fafb; color: var(--ink); }
    .hint { font-size: 12px; color: var(--muted); margin-left: auto; }
    /* ── 실시간 응답 ── */
    #stream-out { width: 100%; height: 88px; border: 1px solid #2d3748; border-radius: 6px;
                  padding: 10px; resize: vertical; font-size: 11.5px;
                  font-family: Consolas, "Liberation Mono", monospace;
                  background: #0d1117; color: #8b949e; line-height: 1.5; margin-bottom: 10px; }
    /* ── 지표 바 ── */
    .metrics-bar { display: flex; margin-bottom: 10px;
                   border: 1px solid var(--line); border-radius: 6px; overflow: hidden; }
    .metric-item { flex: 1; display: flex; flex-direction: column; align-items: center;
                   padding: 7px 4px; border-right: 1px solid var(--line); background: #f8fafc; }
    .metric-item:last-child { border-right: none; }
    .metric-label { font-size: 10px; font-weight: 700; color: var(--muted);
                    text-transform: uppercase; letter-spacing: .4px; }
    .metric-value { font-size: 13px; font-weight: 700; font-family: monospace;
                    color: var(--ink); margin-top: 2px; }
    /* ── 결과 분석 ── */
    .analysis-strip { display: flex; align-items: center; gap: 10px; flex-wrap: wrap;
                       padding: 8px 12px; background: #f8fafc;
                       border: 1px solid var(--line); border-radius: 6px; margin-bottom: 14px; }
    .an-item { display: flex; align-items: center; gap: 5px; font-size: 12px; }
    .an-label { font-weight: 700; color: var(--muted); }
    .badge-ok   { display: inline-block; padding: 2px 7px; border-radius: 4px;
                  font-size: 11px; font-weight: 700; background: #dcfce7; color: #166534; }
    .badge-warn { display: inline-block; padding: 2px 7px; border-radius: 4px;
                  font-size: 11px; font-weight: 700; background: #fef9c3; color: #854d0e; }
    .badge-err  { display: inline-block; padding: 2px 7px; border-radius: 4px;
                  font-size: 11px; font-weight: 700; background: #fee2e2; color: #991b1b; }
    /* ── 로그 ── */
    .resp-title { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
    .resp-title h2 { margin: 0; }
    #log { display: flex; flex-direction: column; gap: 10px; max-height: 380px; overflow-y: auto; }
    .card { border: 1px solid var(--line); border-radius: 6px; overflow: hidden; }
    .card-head { display: flex; justify-content: space-between; align-items: center;
                 padding: 7px 12px; background: #f1f5f9;
                 border-bottom: 1px solid var(--line); gap: 8px; }
    .card-meta { font-size: 11px; color: var(--muted); font-family: monospace;
                 overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .tag { display: inline-block; font-size: 11px; font-weight: 700;
           padding: 2px 8px; border-radius: 4px; white-space: nowrap; }
    .t-sent { background: #e0f2fe; color: #0369a1; }
    .t-resp { background: var(--accent-soft); color: var(--accent); }
    .t-err  { background: #fee2e2; color: #b91c1c; }
    .card pre { margin: 0; padding: 12px; overflow-x: auto; background: #fff;
                font-family: Consolas, "Liberation Mono", monospace;
                font-size: 12px; line-height: 1.55; }
    .jk { color: #0550ae; } .js { color: #0a7a61; }
    .jn { color: #7c3aed; } .jb { color: #c2410c; } .jz { color: #6b7280; }
    .empty { text-align: center; color: var(--muted); padding: 44px 0; font-size: 14px; }
    /* ── 비교 기록 ── */
    .comparison-section { margin-top: 18px; }
    .hist-filter { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
    .hist-filter select { padding: 4px 8px; font-size: 12px; border: 1px solid var(--line);
                          border-radius: 4px; background: #f9fafb; color: var(--ink); cursor: pointer; }
    .table-wrap { overflow-x: auto; }
    #history-table { width: 100%; border-collapse: collapse; font-size: 13px; }
    #history-table th { text-align: left; padding: 8px 10px; border-bottom: 2px solid var(--line);
                        font-size: 11px; color: var(--muted); font-weight: 700;
                        white-space: nowrap; text-transform: uppercase; letter-spacing: .4px; }
    #history-table td { padding: 7px 10px; border-bottom: 1px solid var(--line); vertical-align: middle; }
    #history-table tr:last-child td { border-bottom: none; }
    #history-table tr:hover td { background: #f8fafc; }
    /* ── 합성 결과 ── */
    .mat-result { border: 1px solid var(--line); border-radius: 8px; margin-bottom: 14px; overflow: hidden; }
    .mr-head { display: flex; align-items: center; gap: 10px; padding: 10px 14px;
               background: var(--accent-soft); border-bottom: 1px solid var(--line); }
    .mr-kind { font-size: 13px; font-weight: 800; color: var(--accent); }
    .mr-hash { margin-left: auto; font-size: 11px; font-family: monospace; color: var(--muted); }
    .mr-body { padding: 12px 14px; display: flex; flex-direction: column; gap: 8px; }
    .mr-row { display: flex; gap: 8px; font-size: 13px; }
    .mr-row .mr-k { min-width: 96px; font-weight: 700; color: var(--muted); }
    .mr-row .mr-v { color: var(--ink); }
    .mr-outputs { display: flex; flex-wrap: wrap; gap: 6px; }
    .mr-chip { font-size: 12px; font-weight: 700; padding: 3px 9px; border-radius: 5px;
               background: #eef2ff; color: #3730a3; font-family: monospace; }
    .mr-props { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
    .mr-prop { text-align: center; padding: 7px 4px; background: #f8fafc;
               border: 1px solid var(--line); border-radius: 6px; }
    .mr-prop .mp-l { font-size: 10px; font-weight: 700; color: var(--muted);
                     text-transform: uppercase; letter-spacing: .4px; }
    .mr-prop .mp-v { font-size: 14px; font-weight: 700; font-family: monospace; color: var(--ink); margin-top: 2px; }
    .mr-fail { color: #991b1b; font-size: 13px; }
  </style>
</head>
<body>
<main>
  <header>
    <h1>에이전트 테스트 콘솔</h1>
    <p>WebSocket으로 에이전트에 직접 요청을 보내고 응답을 분석합니다.</p>
    <nav class="nav">
      <a href="/agent-docs">전체 에이전트 서비스 문서</a>
      <a href="/architecture">전체 에이전트 아키텍처</a>
      <a href="/agent-test" class="active">테스트 콘솔</a>
      <a href="/quest-agent-docs">퀘스트 서비스 문서</a>
      <a href="/quest-agent-architecture">퀘스트 아키텍처</a>
      <a href="/health">헬스 체크</a>
    </nav>
  </header>

  <div class="status-bar">
    <div id="dot" class="dot"></div>
    <span id="ws-label">연결 안됨</span>
    <input id="ws-url" type="text" placeholder="ws://host/ws/agent">
    <button id="btn-connect" class="btn btn-ghost" onclick="toggleConn()">연결</button>
  </div>

  <div class="console-grid">
    <!-- 왼쪽: 요청 설정 + 에디터 -->
    <section class="panel">
      <h2>요청</h2>

      <details class="req-settings">
        <summary>요청 설정</summary>
        <div class="settings-body">
          <div class="field">
            <label>모델</label>
            <select id="cfg-model">
              <option value="">기본값</option>
              <option value="claude-sonnet-4-6">claude-sonnet-4-6</option>
              <option value="claude-opus-4-8">claude-opus-4-8</option>
              <option value="claude-haiku-4-5-20251001">claude-haiku-4-5-20251001</option>
            </select>
          </div>
          <div class="field">
            <label>Max Tokens</label>
            <input type="number" id="cfg-maxtokens" min="1" max="8192" placeholder="기본값">
          </div>
          <div class="field full">
            <label>Temperature &nbsp;<span id="cfg-temp-val">1.0</span></label>
            <div class="temp-row">
              <span>0</span>
              <input type="range" id="cfg-temp" min="0" max="2" step="0.1" value="1"
                     oninput="document.getElementById('cfg-temp-val').textContent=parseFloat(this.value).toFixed(1)">
              <span>2</span>
            </div>
          </div>
          <div class="field full">
            <label>시스템 프롬프트 오버라이드 <span style="color:var(--muted)">(context.system_prompt)</span></label>
            <textarea id="cfg-system" rows="2" placeholder="비워두면 에이전트 기본 프롬프트 사용"></textarea>
          </div>
          <div class="field full">
            <label>사용자 프롬프트 오버라이드 <span style="color:var(--muted)">(context.user_prompt)</span></label>
            <textarea id="cfg-user-prompt" rows="2" placeholder="비워두면 payload 그대로 사용"></textarea>
          </div>
        </div>
      </details>

      <div class="preset-row">
        <label>프리셋</label>
        <select id="preset-select" onchange="applyPreset()">
          <optgroup label="process_optimizer">
            <option value="process_optimizer">공정 최적화</option>
          </optgroup>
          <optgroup label="operator_guide">
            <option value="operator_guide.machine_help">설비 도움말</option>
            <option value="operator_guide.recipe_explainer">레시피 설명</option>
            <option value="operator_guide.troubleshooter">트러블슈팅</option>
          </optgroup>
          <optgroup label="quest_generator">
            <option value="quest_generator.production_quest">생산 퀘스트</option>
            <option value="quest_generator.economy_quest">경제 퀘스트</option>
          </optgroup>
          <optgroup label="material_generation">
            <option value="material_generation.recipe_match">레시피 매칭 (기존 레시피)</option>
            <option value="material_generation.new_material">신물질 합성 (새 물질)</option>
          </optgroup>
        </select>
        <button class="btn btn-ghost btn-sm" onclick="newId()">ID 갱신</button>
      </div>
      <textarea id="editor" spellcheck="false" oninput="onInput()"></textarea>
      <div class="send-row">
        <button id="btn-send" class="btn btn-primary" onclick="doSend()" disabled>전송</button>
        <input id="tag-input" type="text" placeholder="태그 (선택)">
        <span id="hint" class="hint"></span>
      </div>
    </section>

    <!-- 오른쪽: 실시간 응답 + 지표 + 분석 + 로그 -->
    <section class="panel">
      <h2 style="margin-bottom:8px">실시간 응답</h2>
      <textarea id="stream-out" readonly placeholder="전송하면 응답 원문이 여기에 표시됩니다..."></textarea>

      <div class="metrics-bar">
        <div class="metric-item">
          <span class="metric-label">TTFT</span>
          <span id="m-ttft" class="metric-value">—</span>
        </div>
        <div class="metric-item">
          <span class="metric-label">Latency</span>
          <span id="m-latency" class="metric-value">—</span>
        </div>
        <div class="metric-item">
          <span class="metric-label">Tokens</span>
          <span id="m-tokens" class="metric-value">—</span>
        </div>
        <div class="metric-item">
          <span class="metric-label">Tokens/s</span>
          <span id="m-tps" class="metric-value">—</span>
        </div>
        <div class="metric-item">
          <span class="metric-label">비용</span>
          <span id="m-cost" class="metric-value">—</span>
        </div>
      </div>

      <div class="analysis-strip">
        <div class="an-item">
          <span class="an-label">JSON</span>
          <span id="an-json">—</span>
        </div>
        <div class="an-item">
          <span class="an-label">스키마</span>
          <span id="an-schema">—</span>
        </div>
        <div class="an-item">
          <span class="an-label">품질</span>
          <span id="an-quality">—</span>
        </div>
        <button id="btn-retry" class="btn btn-ghost btn-sm" onclick="doRetry()"
                style="margin-left:auto" disabled>오류 재시도</button>
      </div>

      <div id="mat-result" class="mat-result" style="display:none"></div>

      <div class="resp-title">
        <h2>응답 로그</h2>
        <button class="btn btn-ghost btn-sm" onclick="clearLog()">지우기</button>
      </div>
      <div id="log"><div class="empty">연결 후 전송하면 응답이 여기에 표시됩니다.</div></div>
    </section>
  </div>

  <!-- 비교 기록 -->
  <section class="panel comparison-section">
    <div class="resp-title">
      <h2>비교 기록</h2>
      <div class="hist-filter">
        <select id="hist-filter-agent" onchange="renderHistory()">
          <option value="">전체 에이전트</option>
        </select>
        <select id="hist-filter-model" onchange="renderHistory()">
          <option value="">전체 모델</option>
        </select>
        <button class="btn btn-ghost btn-sm" onclick="clearHistory()">기록 지우기</button>
      </div>
    </div>
    <div class="table-wrap">
      <table id="history-table">
        <thead>
          <tr>
            <th>#</th><th>시간</th><th>태그</th><th>에이전트</th><th>모델</th>
            <th>Latency</th><th>Tokens</th><th>비용</th><th>품질</th><th>상태</th>
          </tr>
        </thead>
        <tbody id="history-body">
          <tr><td colspan="10" class="empty">아직 기록이 없습니다.</td></tr>
        </tbody>
      </table>
    </div>
  </section>
</main>
<script>
const PRESETS = __PRESETS_JSON__;

let ws = null, sendTs = 0, firstMsgTs = 0;
let lastRequest = null, lastAgent = '', runCounter = 0, runHistory = [];

function uuid() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
    var r = Math.random() * 16 | 0;
    return (c === 'x' ? r : (r & 0x3 | 0x8)).toString(16);
  });
}

function wsDefault() {
  return (location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + location.host + '/ws/agent';
}

function setStatus(cls, label) {
  document.getElementById('dot').className = 'dot ' + cls;
  document.getElementById('ws-label').textContent = label;
  document.getElementById('btn-send').disabled = (cls !== 'connected');
  document.getElementById('btn-connect').textContent =
    (cls === 'connected' || cls === 'connecting') ? '연결 해제' : '연결';
}

function toggleConn() {
  if (ws && ws.readyState <= 1) { ws.close(); ws = null; setStatus('', '연결 안됨'); return; }
  var url = document.getElementById('ws-url').value.trim() || wsDefault();
  setStatus('connecting', '연결 중...');
  try { ws = new WebSocket(url); } catch(e) { setStatus('error', '연결 실패: ' + e.message); return; }
  ws.onopen  = function() { setStatus('connected', '연결됨'); };
  ws.onclose = function() { setStatus('', '연결 안됨'); ws = null; };
  ws.onerror = function() { setStatus('error', '오류'); };
  ws.onmessage = function(e) {
    if (!firstMsgTs) firstMsgTs = Date.now();
    document.getElementById('stream-out').value = e.data;
    addCard(e.data, false);
    updateMetrics(e.data);
    updateAnalysis(e.data);
    renderMaterialResult(e.data);
  };
}

function getReqSettings() {
  var model  = document.getElementById('cfg-model').value;
  var temp   = document.getElementById('cfg-temp').value;
  var maxTok = document.getElementById('cfg-maxtokens').value;
  var sys    = document.getElementById('cfg-system').value.trim();
  var userP  = document.getElementById('cfg-user-prompt').value.trim();
  var out = {};
  if (model)  out.model = model;
  if (temp !== '') out.temperature = parseFloat(temp);
  if (maxTok !== '') out.max_tokens = parseInt(maxTok, 10);
  if (sys)    out.system_prompt = sys;
  if (userP)  out.user_prompt = userP;
  return out;
}

function doSend() {
  if (!ws || ws.readyState !== 1) return;
  var raw = document.getElementById('editor').value.trim();
  var msg;
  try { msg = JSON.parse(raw); } catch(e) { addErrCard('JSON 파싱 오류: ' + e.message); return; }
  if (!msg.request_id) msg.request_id = uuid();
  var ov = getReqSettings();
  if (Object.keys(ov).length > 0) msg.context = Object.assign({}, msg.context || {}, ov);
  document.getElementById('editor').value = JSON.stringify(msg, null, 2);
  sendTs = Date.now(); firstMsgTs = 0;
  lastRequest = JSON.stringify(msg); lastAgent = msg.agent || '';
  resetMetrics(); resetAnalysis();
  document.getElementById('stream-out').value = '';
  document.getElementById('btn-retry').disabled = false;
  ws.send(JSON.stringify(msg));
  addCard(JSON.stringify(msg), true);
}

function applyPreset() {
  var key = document.getElementById('preset-select').value;
  var p = PRESETS[key]; if (!p) return;
  var msg = JSON.parse(JSON.stringify(p));
  msg.request_id = uuid(); msg.session_id = 'test-session'; msg.client_id = 'test-console';
  document.getElementById('editor').value = JSON.stringify(msg, null, 2);
  onInput();
}

function newId() {
  try {
    var msg = JSON.parse(document.getElementById('editor').value);
    msg.request_id = uuid();
    document.getElementById('editor').value = JSON.stringify(msg, null, 2);
  } catch(_) {}
}

function onInput() {
  var n = document.getElementById('editor').value.length;
  document.getElementById('hint').textContent = n.toLocaleString() + ' chars';
}

function resetMetrics() {
  ['m-ttft','m-latency','m-tokens','m-tps','m-cost'].forEach(function(id) {
    document.getElementById(id).textContent = '—';
  });
}

function resetAnalysis() {
  document.getElementById('an-json').innerHTML = '—';
  document.getElementById('an-schema').innerHTML = '—';
  document.getElementById('an-quality').innerHTML = '—';
  clearMatResult();
}

function clearMatResult() {
  var box = document.getElementById('mat-result');
  box.style.display = 'none';
  box.innerHTML = '';
}

const MR_KIND_LABEL = {
  existing_recipe: '레시피 결과 (기존 레시피)',
  cached_experiment: '레시피 결과 (캐시된 실험)',
  new_material: '합성 물질 결과 (신물질)',
  failed_result: '합성 실패',
  invalid_input: '잘못된 입력'
};

function mrRow(k, v) {
  return '<div class="mr-row"><span class="mr-k">' + esc(k) + '</span>' +
         '<span class="mr-v">' + esc(v) + '</span></div>';
}

function renderMaterialResult(rawJson) {
  var parsed; try { parsed = JSON.parse(rawJson); } catch(_) { clearMatResult(); return; }
  var p = parsed && parsed.payload;
  if (!p || typeof p.result_type !== 'string') { clearMatResult(); return; }
  var kind = p.result_type;
  var label = MR_KIND_LABEL[kind] || kind;
  var rows = '';

  if (kind === 'existing_recipe' || kind === 'cached_experiment') {
    if (p.recipe_name) rows += mrRow('레시피', p.recipe_name);
    if (Array.isArray(p.outputs) && p.outputs.length) {
      var chips = p.outputs.map(function(o) {
        return '<span class="mr-chip">' + esc(o.item_id) + ' &times; ' + esc(o.qty) + '</span>';
      }).join('');
      rows += '<div class="mr-row"><span class="mr-k">산출물</span>' +
              '<span class="mr-v"><span class="mr-outputs">' + chips + '</span></span></div>';
    }
    if (p.cached) rows += mrRow('캐시', '예');
  } else if (kind === 'new_material') {
    if (p.name) rows += mrRow('이름', p.name);
    if (p.rarity) rows += mrRow('희귀도', p.rarity);
    if (p.material_id) rows += mrRow('물질 ID', p.material_id);
    if (p.generation_status) rows += mrRow('생성 상태', p.generation_status);
    if (p.visual_status) rows += mrRow('비주얼 상태', p.visual_status);
    if (p.fallback_icon) rows += mrRow('대체 아이콘', p.fallback_icon);
    if (p.message) rows += mrRow('메시지', p.message);
    if (p.properties && typeof p.properties === 'object') {
      var order = [['strength','강도'],['conductivity','전도도'],['stability','안정성'],['reactivity','반응성']];
      var grid = order.map(function(pair) {
        var val = p.properties[pair[0]];
        return '<div class="mr-prop"><div class="mp-l">' + pair[1] + '</div>' +
               '<div class="mp-v">' + (val == null ? '—' : esc(val)) + '</div></div>';
      }).join('');
      rows += '<div class="mr-row"><span class="mr-k">속성</span>' +
              '<span class="mr-v" style="flex:1"><div class="mr-props">' + grid + '</div></span></div>';
    }
  } else {
    rows += '<div class="mr-fail">' + esc(p.failure_reason || p.message || '결과를 생성하지 못했습니다.') + '</div>';
  }

  var box = document.getElementById('mat-result');
  box.innerHTML =
    '<div class="mr-head"><span class="mr-kind">' + esc(label) + '</span>' +
    (p.experiment_hash ? '<span class="mr-hash">' + esc(p.experiment_hash) + '</span>' : '') +
    '</div><div class="mr-body">' + rows + '</div>';
  box.style.display = 'block';
}

function updateMetrics(rawJson) {
  var latMs = firstMsgTs - sendTs;
  document.getElementById('m-ttft').textContent    = latMs + ' ms';
  document.getElementById('m-latency').textContent = latMs + ' ms';
  var parsed; try { parsed = JSON.parse(rawJson); } catch(_) {}
  var tokens = 0, known = false;
  if (parsed && Array.isArray(parsed.streams)) {
    parsed.streams.forEach(function(s) {
      if (s && s.usage && typeof s.usage.output_tokens === 'number') {
        tokens += s.usage.output_tokens; known = true;
      }
    });
  }
  if (!known) tokens = Math.round(rawJson.length / 4);
  document.getElementById('m-tokens').textContent = (known ? '' : '~') + tokens;
  document.getElementById('m-tps').textContent =
    latMs > 0 ? Math.round(tokens / (latMs / 1000)) + '/s' : '—';
  var model = '';
  if (parsed && parsed.payload && parsed.payload.metadata &&
      parsed.payload.metadata.currentModel) {
    model = parsed.payload.metadata.currentModel.model || '';
  }
  if (!model && parsed && parsed.payload && parsed.payload.metadata) {
    model = parsed.payload.metadata.llmModel || '';
  }
  if (!model) model = document.getElementById('cfg-model').value || '기본값';
  var cost = calcCost(model, tokens);
  document.getElementById('m-cost').textContent = cost;
  var tag  = document.getElementById('tag-input').value.trim();
  var qual = calcQuality(parsed);
  var isErr = !!(parsed && parsed.type === 'agent.error');
  runCounter++;
  runHistory.unshift({ n: runCounter, time: new Date().toLocaleTimeString('ko-KR'),
    tag: tag, agent: (parsed && parsed.agent) || lastAgent, model: model,
    latency: latMs, tokens: tokens, known: known, cost: cost, quality: qual,
    status: isErr ? 'error' : 'ok' });
  updateHistoryFilters();
  renderHistory();
}

function calcCost(model, tokens) {
  if (!model || model === '기본값') return '—';
  if (model.indexOf('claude') === -1) return '로컬';
  var rate = model.indexOf('haiku') !== -1 ? 4 : model.indexOf('opus') !== -1 ? 75 : 15;
  var usd = (tokens / 1e6) * rate;
  return usd < 1e-7 ? '<$0.0000001' : '$' + usd.toFixed(7);
}

function calcQuality(parsed) {
  if (!parsed || typeof parsed !== 'object') return 0;
  var s = 0;
  if (parsed.type === 'agent.response') s += 30;
  if (typeof parsed.request_id === 'string') s += 20;
  if (typeof parsed.agent === 'string') s += 20;
  if (parsed.payload && typeof parsed.payload === 'object' &&
      Object.keys(parsed.payload).length > 0) s += 30;
  return s;
}

function updateAnalysis(rawJson) {
  var parsed, ok = false;
  try { parsed = JSON.parse(rawJson); ok = true; } catch(_) {}
  document.getElementById('an-json').innerHTML =
    ok ? '<span class="badge-ok">PASS</span>' : '<span class="badge-err">FAIL</span>';
  if (ok && parsed) {
    var hasT = typeof parsed.type === 'string' &&
               (parsed.type === 'agent.response' || parsed.type === 'agent.error');
    var hasR = typeof parsed.request_id === 'string';
    var hasA = typeof parsed.agent === 'string';
    if (hasT && hasR && hasA) {
      document.getElementById('an-schema').innerHTML = '<span class="badge-ok">PASS</span>';
    } else {
      var miss = [];
      if (!hasT) miss.push('type'); if (!hasR) miss.push('request_id'); if (!hasA) miss.push('agent');
      document.getElementById('an-schema').innerHTML =
        '<span class="badge-warn">WARN: ' + esc(miss.join(', ')) + ' 누락</span>';
    }
  } else {
    document.getElementById('an-schema').innerHTML = '<span class="badge-err">FAIL</span>';
  }
  var q = ok ? calcQuality(parsed) : 0;
  var qc = q >= 80 ? 'badge-ok' : q >= 40 ? 'badge-warn' : 'badge-err';
  document.getElementById('an-quality').innerHTML = '<span class="' + qc + '">' + q + '/100</span>';
}

function doRetry() {
  if (!lastRequest || !ws || ws.readyState !== 1) return;
  var msg; try { msg = JSON.parse(lastRequest); } catch(_) { return; }
  msg.request_id = uuid();
  lastRequest = JSON.stringify(msg);
  document.getElementById('editor').value = JSON.stringify(msg, null, 2);
  sendTs = Date.now(); firstMsgTs = 0;
  resetMetrics(); resetAnalysis();
  document.getElementById('stream-out').value = '';
  ws.send(JSON.stringify(msg));
  addCard(JSON.stringify(msg), true);
}

function updateHistoryFilters() {
  var agents = new Set(), models = new Set();
  runHistory.forEach(function(r) { if (r.agent) agents.add(r.agent); if (r.model) models.add(r.model); });
  function refresh(selId, cur, values, allLabel) {
    var sel = document.getElementById(selId);
    sel.innerHTML = '<option value="">' + allLabel + '</option>';
    values.forEach(function(v) {
      var o = document.createElement('option');
      o.value = v; o.textContent = v; if (v === cur) o.selected = true;
      sel.appendChild(o);
    });
  }
  refresh('hist-filter-agent', document.getElementById('hist-filter-agent').value, agents, '전체 에이전트');
  refresh('hist-filter-model', document.getElementById('hist-filter-model').value, models, '전체 모델');
}

function renderHistory() {
  var af = document.getElementById('hist-filter-agent').value;
  var mf = document.getElementById('hist-filter-model').value;
  var rows = runHistory.filter(function(r) {
    return (!af || r.agent === af) && (!mf || r.model === mf);
  });
  var tbody = document.getElementById('history-body');
  if (!rows.length) {
    tbody.innerHTML = '<tr><td colspan="10" class="empty">기록이 없습니다.</td></tr>'; return;
  }
  tbody.innerHTML = rows.map(function(r) {
    var sc = r.status === 'ok' ? 'badge-ok' : 'badge-err';
    var sl = r.status === 'ok' ? 'OK' : 'ERR';
    var qc = r.quality >= 80 ? 'badge-ok' : r.quality >= 40 ? 'badge-warn' : 'badge-err';
    return '<tr>' +
      '<td>' + r.n + '</td>' +
      '<td style="white-space:nowrap">' + r.time + '</td>' +
      '<td style="color:var(--muted)">' + esc(r.tag || '') + '</td>' +
      '<td style="font-size:11px;font-family:monospace">' + esc(r.agent) + '</td>' +
      '<td style="font-size:11px;font-family:monospace">' + esc(r.model) + '</td>' +
      '<td style="font-family:monospace">' + r.latency + ' ms</td>' +
      '<td style="font-family:monospace">' + (r.known ? '' : '~') + r.tokens + '</td>' +
      '<td style="font-size:11px;font-family:monospace">' + esc(r.cost) + '</td>' +
      '<td><span class="' + qc + '">' + r.quality + '</span></td>' +
      '<td><span class="' + sc + '">' + sl + '</span></td>' +
      '</tr>';
  }).join('');
}

function clearHistory() {
  runHistory = []; runCounter = 0;
  document.getElementById('hist-filter-agent').innerHTML = '<option value="">전체 에이전트</option>';
  document.getElementById('hist-filter-model').innerHTML = '<option value="">전체 모델</option>';
  renderHistory();
}

function clearLog() {
  document.getElementById('log').innerHTML =
    '<div class="empty">연결 후 전송하면 응답이 여기에 표시됩니다.</div>';
}

function addCard(rawJson, isSent) {
  var log = document.getElementById('log');
  if (log.querySelector('.empty')) log.innerHTML = '';
  var ts = new Date().toLocaleTimeString('ko-KR');
  var parsed, isError = false;
  try { parsed = JSON.parse(rawJson); isError = !!(parsed && parsed.type === 'agent.error'); }
  catch(_) { parsed = rawJson; }
  var rid = (parsed && typeof parsed === 'object' && parsed.request_id) ? parsed.request_id : '';
  var cls = isSent ? 't-sent' : (isError ? 't-err' : 't-resp');
  var lbl = isSent ? '&#x2192; SENT' : (isError ? '&#x2190; ERROR' : '&#x2190; RESPONSE');
  var card = document.createElement('div');
  card.className = 'card';
  card.innerHTML =
    '<div class="card-head">' +
      '<span class="tag ' + cls + '">' + lbl + '</span>' +
      '<span class="card-meta">' + ts + (rid ? ' &middot; ' + esc(rid) : '') + '</span>' +
    '</div>' +
    '<pre>' + hlJson(parsed) + '</pre>';
  log.insertBefore(card, log.firstChild);
}

function addErrCard(msg) {
  var log = document.getElementById('log');
  if (log.querySelector('.empty')) log.innerHTML = '';
  var card = document.createElement('div');
  card.className = 'card';
  card.innerHTML =
    '<div class="card-head"><span class="tag t-err">오류</span>' +
    '<span class="card-meta">' + new Date().toLocaleTimeString('ko-KR') + '</span></div>' +
    '<pre style="color:#b91c1c">' + esc(msg) + '</pre>';
  log.insertBefore(card, log.firstChild);
}

function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function hlJson(obj) {
  var s = (typeof obj === 'string') ? obj : JSON.stringify(obj, null, 2);
  var out = '', i = 0, n = s.length;
  while (i < n) {
    var c = s[i];
    if (c === '"') {
      var j = i + 1;
      while (j < n) {
        if (s[j] === '\\\\') { j += 2; }
        else if (s[j] === '"') { j++; break; }
        else { j++; }
      }
      var tok = s.slice(i, j), k = j;
      while (k < n && s[k] === ' ') { k++; }
      out += '<span class="' + (s[k] === ':' ? 'jk' : 'js') + '">' + esc(tok) + '</span>';
      i = j;
    } else if (s.substring(i, i+4) === 'true')  { out += '<span class="jb">true</span>';  i += 4; }
    else if (s.substring(i, i+5) === 'false') { out += '<span class="jb">false</span>'; i += 5; }
    else if (s.substring(i, i+4) === 'null')  { out += '<span class="jz">null</span>';  i += 4; }
    else if (c === '-' || (c >= '0' && c <= '9')) {
      var j2 = i; if (s[j2] === '-') { j2++; }
      while (j2 < n) {
        var d = s[j2];
        if ((d >= '0' && d <= '9') || d === '.') { j2++; }
        else if ((d === 'e' || d === 'E') && j2 < n - 1) {
          j2++;
          if (s[j2] === '+' || s[j2] === '-') { j2++; }
        } else { break; }
      }
      out += '<span class="jn">' + esc(s.slice(i, j2)) + '</span>'; i = j2;
    } else { out += esc(c); i++; }
  }
  return out;
}

document.getElementById('ws-url').value = wsDefault();
applyPreset();

(function loadOllamaModels() {
  fetch('http://localhost:11434/api/tags')
    .then(function(r) { return r.json(); })
    .then(function(data) {
      var models = (data && Array.isArray(data.models)) ? data.models : [];
      if (!models.length) return;
      var sel = document.getElementById('cfg-model');
      var grp = document.createElement('optgroup');
      grp.label = 'Ollama (로컬)';
      models.forEach(function(m) {
        var o = document.createElement('option');
        o.value = m.name; o.textContent = m.name;
        grp.appendChild(o);
      });
      sel.appendChild(grp);
    })
    .catch(function() {});
})();
</script>
</body>
</html>"""


def _render_test_page() -> str:
    """Render an interactive WebSocket test console for agent requests."""
    presets: dict[str, Any] = {
        "process_optimizer": {
            "type": "agent.request",
            "agent": "process_optimizer",
            "payload": {
                "machines": [
                    {"id": "iron-smelter-1", "throughput": 45, "capacity": 100},
                    {"id": "assembler-2", "throughput": 80, "capacity": 100},
                    {"id": "conveyor-3", "throughput": 30, "capacity": 60},
                ],
            },
        },
        "operator_guide.machine_help": {
            "type": "agent.request",
            "agent": "operator_guide",
            "payload": {
                "question": "컨베이어 벨트 속도를 어떻게 조절하나요?",
                "sub_agent": "operator_guide.machine_help",
            },
        },
        "operator_guide.recipe_explainer": {
            "type": "agent.request",
            "agent": "operator_guide",
            "payload": {
                "question": "강화 합금 레시피가 뭐야?",
                "sub_agent": "operator_guide.recipe_explainer",
            },
        },
        "operator_guide.troubleshooter": {
            "type": "agent.request",
            "agent": "operator_guide",
            "payload": {
                "question": "철광석 제련기가 갑자기 멈췄어.",
                "sub_agent": "operator_guide.troubleshooter",
            },
        },
        "quest_generator.production_quest": {
            "type": "agent.request",
            "agent": "quest_generator",
            "payload": {
                "request": "철광석을 모아 첫 생산 라인을 가동하게 하는 퀘스트를 만들어줘.",
                "sub_agent": "quest_generator.production_quest",
            },
        },
        "quest_generator.economy_quest": {
            "type": "agent.request",
            "agent": "quest_generator",
            "payload": {
                "request": "창고에 쌓인 자원을 줄이는 퀘스트가 필요해.",
                "sub_agent": "quest_generator.economy_quest",
            },
        },
        "material_generation.recipe_match": {
            "type": "agent.request",
            "agent": "material_generation",
            "payload": {
                "machine_type": "Smelter",
                "inputs": [
                    {"item_id": "iron_ore", "qty": 2},
                ],
                "generate_visual_asset": True,
            },
        },
        "material_generation.new_material": {
            "type": "agent.request",
            "agent": "material_generation",
            "payload": {
                "machine_type": "Synthesizer",
                "inputs": [
                    {"item_id": "iron_ingot", "qty": 2},
                    {"item_id": "copper_ingot", "qty": 1},
                ],
                "process_conditions": {
                    "temperature": "1200C",
                    "pressure": "5atm",
                    "catalyst": "palladium",
                },
                "generate_visual_asset": True,
            },
        },
    }
    presets_js = json.dumps(presets, ensure_ascii=False, indent=2)
    return _TEST_PAGE_TEMPLATE.replace("__PRESETS_JSON__", presets_js, 1)
