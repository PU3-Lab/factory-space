"""HTML documentation routes for the Manual Q&A operator guide prototype."""

from __future__ import annotations

from fastapi import APIRouter
from fastapi.responses import HTMLResponse

router = APIRouter(tags=["manual-qa-docs"])


def _page(title: str, body: str) -> str:
    return f"""<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{title}</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script type="module">
    import mermaid from "https://cdn.jsdelivr.net/npm/mermaid@10/dist/mermaid.esm.min.mjs";
    mermaid.initialize({{ startOnLoad: true, theme: "default" }});
  </script>
</head>
<body class="bg-slate-50 text-slate-900">
  <main class="mx-auto max-w-6xl px-6 py-10">
    <nav class="mb-8 flex flex-wrap gap-3 text-sm">
      <a class="rounded-md bg-slate-900 px-3 py-2 text-white" href="/manual-qa-docs">Manual Q&A Docs</a>
      <a class="rounded-md border border-slate-300 px-3 py-2 text-slate-700" href="/manual-qa-architecture">Architecture</a>
    </nav>
    {body}
  </main>
</body>
</html>"""


@router.get("/manual-qa-docs", response_class=HTMLResponse)
async def manual_qa_docs() -> HTMLResponse:
    """Return the service-facing Manual Q&A documentation page."""

    body = """
    <section class="mb-10 rounded-xl bg-white p-8 shadow-sm ring-1 ring-slate-200">
      <p class="mb-3 text-sm font-semibold uppercase tracking-wide text-blue-700">Manual Q&A Agent</p>
      <h1 class="mb-4 text-3xl font-bold">operator_guide 서비스 설명</h1>
      <p class="text-lg leading-8 text-slate-700">
        <strong>operator_guide</strong>는 플레이어가 장비, 자원, 레시피, 문제 해결 질문을 하면
        CSV 기반 매뉴얼 데이터를 조회해 답변을 반환하는 안내 에이전트입니다.
      </p>
      <p class="mt-4 rounded-lg bg-blue-50 p-4 text-sm leading-7 text-blue-950">
        현재 프로토는 CSV 기반 규칙형 답변을 사용합니다. 최종 단계에서는 router, tool, PostgreSQL,
        pgvector, LLM, 필요한 경우에만 조회하는 player state를 붙여 더 상황 친화적인 답변으로 고도화합니다.
      </p>
    </section>

    <section class="mb-10">
      <h2 class="mb-4 text-2xl font-bold">현재 프로토에서 하는 일</h2>
      <ol class="grid gap-3 md:grid-cols-2">
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">1. Unreal UI / Front에서 질문을 받습니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">2. AgentPipeline에서 요청 context를 정리합니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">3. Orchestrator가 <code>operator_guide</code>를 선택합니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">4. operator_guide 내부 leaf agent를 선택합니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">5. leaf agent fallback에서 <code>ManualQAService</code>를 호출합니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">6. 질문 유형을 장비, 자원, 레시피, 문제 해결, unknown으로 분류합니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">7. <code>CsvManualQARepository</code>가 5개 CSV를 조회합니다.</li>
        <li class="rounded-lg bg-white p-4 shadow-sm ring-1 ring-slate-200">8. <code>ManualQAResponseBuilder</code>가 응답 JSON을 구성합니다.</li>
      </ol>
    </section>

    <section class="mb-10">
      <h2 class="mb-4 text-2xl font-bold">관련 Tool / 내부 모듈</h2>
      <div class="grid gap-4 md:grid-cols-2">
        <article class="rounded-lg bg-white p-5 shadow-sm ring-1 ring-slate-200">
          <h3 class="mb-2 text-lg font-semibold">ManualQAService</h3>
          <p><strong>input:</strong> question: str</p>
          <p><strong>output:</strong> ManualQAResult</p>
          <p class="mt-2 text-slate-700">질문 분류, CSV 조회, 응답 생성을 연결하는 Manual Q&A 프로토의 중심 서비스입니다.</p>
        </article>
        <article class="rounded-lg bg-white p-5 shadow-sm ring-1 ring-slate-200">
          <h3 class="mb-2 text-lg font-semibold">ManualQAQuestionClassifier</h3>
          <p><strong>input:</strong> question: str</p>
          <p><strong>output:</strong> ManualQAIntent, question_type, primary_manual, supporting_manuals, target_ids</p>
          <p class="mt-2 text-slate-700">질문이 장비, 자원, 레시피, 문제 해결, unknown 중 어디에 해당하는지 판단합니다.</p>
        </article>
        <article class="rounded-lg bg-white p-5 shadow-sm ring-1 ring-slate-200">
          <h3 class="mb-2 text-lg font-semibold">CsvManualQARepository</h3>
          <p><strong>input:</strong> equipment_id, resource_id, recipe_id, issue_id, action_id</p>
          <p><strong>output:</strong> CSV row 기반 도메인 데이터</p>
          <p class="mt-2 text-slate-700">현재 프로토에서 사용하는 매뉴얼 지식 저장소입니다.</p>
        </article>
        <article class="rounded-lg bg-white p-5 shadow-sm ring-1 ring-slate-200">
          <h3 class="mb-2 text-lg font-semibold">ManualQAResponseBuilder</h3>
          <p><strong>input:</strong> question, intent, CSV 조회 결과</p>
          <p><strong>output:</strong> final_answer, sources, recommended_actions, metadata</p>
          <p class="mt-2 text-slate-700">플레이어에게 보여줄 답변과 Unreal/Front가 활용할 metadata를 구성합니다.</p>
        </article>
        <article class="rounded-lg bg-white p-5 shadow-sm ring-1 ring-slate-200 md:col-span-2">
          <h3 class="mb-2 text-lg font-semibold">build_manual_qa_agent_result</h3>
          <p><strong>input:</strong> payload, AgentContext, topic, sub_agent</p>
          <p><strong>output:</strong> AgentRunResult</p>
          <p class="mt-2 text-slate-700">operator_guide leaf agent fallback 결과를 AgentPipeline이 받을 수 있는 공통 형식으로 변환합니다.</p>
        </article>
      </div>
    </section>

    <section class="mb-10 rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">프로토 Mermaid 흐름</h2>
      <pre class="mermaid">
flowchart TD
  A([Unreal UI / Front]) --> B[AgentPipeline]
  B --> C[Top-level Orchestrator Router]
  C --> D[operator_guide Agent]
  D --> E{Leaf agent router}
  E --> F[machine_help]
  E --> G[recipe_explainer]
  E --> H[troubleshooter]
  F --> I[ManualQAService fallback]
  G --> I
  H --> I
  I --> J[ManualQAQuestionClassifier]
  J --> K{question_type}
  K --> L[equipment_question]
  K --> M[resource_question]
  K --> N[recipe_question]
  K --> O[troubleshooting_question]
  K --> P[unknown_question]
  L --> Q[CsvManualQARepository]
  M --> Q
  N --> Q
  O --> Q
  P --> R[ManualQAResponseBuilder]
  Q -.-> S[equipment.csv]
  Q -.-> T[resources.csv]
  Q -.-> U[recipes.csv]
  Q -.-> V[troubleshooting_rules.csv]
  Q -.-> W[action_policy.csv]
  Q --> R
  R --> X[ManualQAResult]
  X --> Y[agent.response JSON]
  Y --> Z([Player screen])
      </pre>
    </section>

    <section class="rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">워크플로우 예시</h2>
      <div class="grid gap-4 md:grid-cols-2">
        <div class="rounded-lg bg-slate-50 p-4">
          <h3 class="font-semibold">사용자 입력</h3>
          <p class="mt-2 text-slate-700">“기어 만들려면 뭐가 필요해?”</p>
        </div>
        <div class="rounded-lg bg-slate-50 p-4">
          <h3 class="font-semibold">라우팅 결과</h3>
          <p class="mt-2 text-slate-700">Orchestrator는 operator_guide를 선택하고, 내부에서는 recipe_explainer leaf agent로 이동합니다.</p>
        </div>
        <div class="rounded-lg bg-slate-50 p-4">
          <h3 class="font-semibold">호출되는 모듈</h3>
          <p class="mt-2 text-slate-700">ManualQAService, ManualQAQuestionClassifier, CsvManualQARepository, ManualQAResponseBuilder</p>
        </div>
        <div class="rounded-lg bg-slate-50 p-4">
          <h3 class="font-semibold">최종 응답</h3>
          <p class="mt-2 text-slate-700">Unreal에는 agent.response JSON을 반환하고, 플레이어 화면에는 payload.final_answer를 표시합니다.</p>
        </div>
      </div>
    </section>
    """
    return HTMLResponse(_page("Manual Q&A Docs", body))


@router.get("/manual-qa-architecture", response_class=HTMLResponse)
async def manual_qa_architecture() -> HTMLResponse:
    """Return the developer-facing Manual Q&A architecture page."""

    body = """
    <section class="mb-10 rounded-xl bg-white p-8 shadow-sm ring-1 ring-slate-200">
      <p class="mb-3 text-sm font-semibold uppercase tracking-wide text-indigo-700">Architecture</p>
      <h1 class="mb-4 text-3xl font-bold">Manual Q&A / operator_guide 코드 구조</h1>
      <p class="text-lg leading-8 text-slate-700">
        이 페이지는 현재 프로토 코드가 LangChain/LangGraph 개념과 어떻게 대응되는지 설명하는 개발자용 문서입니다.
      </p>
    </section>

    <section class="mb-10 rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">에이전트 구조</h2>
      <div class="overflow-x-auto">
        <table class="w-full border-collapse text-left text-sm">
          <thead class="bg-slate-100">
            <tr><th class="border p-3">에이전트</th><th class="border p-3">한 줄 설명</th><th class="border p-3">관련 파일</th></tr>
          </thead>
          <tbody>
            <tr><td class="border p-3">operator_guide</td><td class="border p-3">Manual Q&A 질문을 leaf agent로 라우팅하는 상위 에이전트입니다.</td><td class="border p-3">agents/operator_guide/agent.py</td></tr>
            <tr><td class="border p-3">machine_help</td><td class="border p-3">장비/설비 질문을 ManualQAService fallback으로 연결합니다.</td><td class="border p-3">machine_help.py</td></tr>
            <tr><td class="border p-3">recipe_explainer</td><td class="border p-3">자원/레시피 질문을 ManualQAService fallback으로 연결합니다.</td><td class="border p-3">recipe_explainer.py</td></tr>
            <tr><td class="border p-3">troubleshooter</td><td class="border p-3">문제 해결 질문을 ManualQAService fallback으로 연결합니다.</td><td class="border p-3">troubleshooter.py</td></tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="mb-10 rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">Tool / 함수 표</h2>
      <div class="overflow-x-auto">
        <table class="w-full border-collapse text-left text-sm">
          <thead class="bg-slate-100">
            <tr><th class="border p-3">함수/클래스명</th><th class="border p-3">설명</th><th class="border p-3">input</th><th class="border p-3">output</th></tr>
          </thead>
          <tbody>
            <tr><td class="border p-3">ManualQAService.answer</td><td class="border p-3">질문 분류, CSV 조회, 응답 생성을 순서대로 실행합니다.</td><td class="border p-3">question: str</td><td class="border p-3">ManualQAResult</td></tr>
            <tr><td class="border p-3">ManualQAQuestionClassifier.classify</td><td class="border p-3">질문 유형과 대상 id를 판단합니다.</td><td class="border p-3">question: str</td><td class="border p-3">ManualQAIntent</td></tr>
            <tr><td class="border p-3">CsvManualQARepository</td><td class="border p-3">5개 CSV에서 장비, 자원, 레시피, 트러블슈팅, 행동 정책을 조회합니다.</td><td class="border p-3">domain id</td><td class="border p-3">CSV row 데이터</td></tr>
            <tr><td class="border p-3">ManualQAResponseBuilder.build</td><td class="border p-3">플레이어 답변과 metadata를 구성합니다.</td><td class="border p-3">question, intent, repository data</td><td class="border p-3">ManualQAResult</td></tr>
            <tr><td class="border p-3">build_manual_qa_agent_result</td><td class="border p-3">ManualQAResult를 AgentRunResult로 변환합니다.</td><td class="border p-3">payload, AgentContext, topic, sub_agent</td><td class="border p-3">AgentRunResult</td></tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="mb-10 rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">미들웨어 표</h2>
      <div class="overflow-x-auto">
        <table class="w-full border-collapse text-left text-sm">
          <thead class="bg-slate-100">
            <tr><th class="border p-3">함수명</th><th class="border p-3">설명</th><th class="border p-3">input</th><th class="border p-3">output</th></tr>
          </thead>
          <tbody>
            <tr><td class="border p-3">append_middleware_log</td><td class="border p-3">각 graph node의 판단 과정과 실행 로그를 state에 추가합니다.</td><td class="border p-3">AgentGraphState, node, event, details</td><td class="border p-3">middlewareLogs가 추가된 state patch</td></tr>
            <tr><td class="border p-3">build_current_model_metadata</td><td class="border p-3">현재 선택된 LLM slot/provider/model 정보를 응답 metadata로 정리합니다.</td><td class="border p-3">AgentGraphState</td><td class="border p-3">model metadata 또는 None</td></tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="mb-10 rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">LangChain / LangGraph 개념 매핑</h2>
      <div class="overflow-x-auto">
        <table class="w-full border-collapse text-left text-sm">
          <thead class="bg-slate-100">
            <tr><th class="border p-3">배운 개념</th><th class="border p-3">현재 코드 역할</th><th class="border p-3">wrapper/factory/custom function을 쓴 이유</th></tr>
          </thead>
          <tbody>
            <tr><td class="border p-3">create_agent</td><td class="border p-3">agent.py의 operator_guide agent factory와 leaf agent 생성 코드</td><td class="border p-3">프로젝트의 AgentPipeline 계약에 맞는 AgentRunResult를 반환해야 하기 때문입니다.</td></tr>
            <tr><td class="border p-3">@tool</td><td class="border p-3">CsvManualQARepository, ManualQAService 같은 내부 모듈</td><td class="border p-3">프로토에서는 외부 tool 호출보다 CSV 조회 모듈이 더 단순하고 검증하기 쉽습니다.</td></tr>
            <tr><td class="border p-3">StateGraph</td><td class="border p-3">agents/pipeline graph runtime과 state 흐름</td><td class="border p-3">현재 프로젝트는 자체 AgentPipeline state 계약을 먼저 사용합니다.</td></tr>
            <tr><td class="border p-3">add_node</td><td class="border p-3">Orchestrator, leaf agent, fallback service 단계</td><td class="border p-3">각 단계를 명시적 함수와 모듈로 나누어 테스트하기 위해서입니다.</td></tr>
            <tr><td class="border p-3">add_edge</td><td class="border p-3">Top-level agent 선택, leaf agent 선택, ManualQAService fallback 연결</td><td class="border p-3">라우팅 결과를 프로젝트 고유 message router와 연결해야 합니다.</td></tr>
            <tr><td class="border p-3">before_model</td><td class="border p-3">append_middleware_log 같은 pipeline middleware</td><td class="border p-3">모델 호출 전후의 판단 로그와 metadata를 남기기 위해서입니다.</td></tr>
          </tbody>
        </table>
      </div>
    </section>

    <section class="rounded-xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
      <h2 class="mb-4 text-2xl font-bold">프로토에서 최종으로 고도화</h2>
      <div class="overflow-x-auto">
        <table class="w-full border-collapse text-left text-sm">
          <thead class="bg-slate-100">
            <tr><th class="border p-3">구분</th><th class="border p-3">현재 프로토</th><th class="border p-3">최종 계획</th></tr>
          </thead>
          <tbody>
            <tr><td class="border p-3">Router</td><td class="border p-3">Orchestrator + leaf agent + 질문 유형 분류</td><td class="border p-3">LangGraph router로 질문 유형과 상태 필요 여부를 분리합니다.</td></tr>
            <tr><td class="border p-3">Tool</td><td class="border p-3">CSV repository 모듈</td><td class="border p-3">equipment/resource/recipe/troubleshooting/action/player state/RAG tool로 분리합니다.</td></tr>
            <tr><td class="border p-3">문서 관리</td><td class="border p-3">5개 CSV</td><td class="border p-3">PostgreSQL 구조화 데이터와 pgvector 기반 Markdown chunk 검색으로 확장합니다.</td></tr>
            <tr><td class="border p-3">답변 생성</td><td class="border p-3">템플릿 기반</td><td class="border p-3">LLM이 검색 근거와 필요한 player state만 사용해 자연스러운 답변을 생성합니다.</td></tr>
          </tbody>
        </table>
      </div>
    </section>
    """
    return HTMLResponse(_page("Manual Q&A Architecture", body))
