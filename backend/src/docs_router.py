"""HTML documentation pages for quest agent explanation."""

from __future__ import annotations

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


@router.get("/quest-agent-docs", response_class=HTMLResponse)
async def quest_agent_docs() -> HTMLResponse:
    """Return a service-oriented quest agent explanation page."""

    return HTMLResponse(_render_page("퀘스트 에이전트 서비스 문서", _render_docs_body()))


@router.get("/quest-agent-architecture", response_class=HTMLResponse)
async def quest_agent_architecture() -> HTMLResponse:
    """Return a code-oriented quest agent architecture page."""

    return HTMLResponse(
        _render_page("퀘스트 에이전트 아키텍처", _render_architecture_body())
    )


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
        <a href="/quest-agent-docs">서비스 문서</a>
        <a href="/quest-agent-architecture">아키텍처 문서</a>
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
    tool_rows = tuple(
        tool
        for agent in QUEST_AGENTS
        for tool in agent["tools"]
    )
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
        "<tr>"
        + "".join(f"<td>{escape(str(row[key]))}</td>" for key in keys)
        + "</tr>"
        for row in rows
    )
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"


def _render_tuple_table(headers: tuple[str, ...], rows: tuple[tuple[str, ...], ...]) -> str:
    head = "".join(f"<th>{escape(header)}</th>" for header in headers)
    body = "".join(
        "<tr>"
        + "".join(f"<td>{escape(value)}</td>" for value in row)
        + "</tr>"
        for row in rows
    )
    return f"<table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table>"
