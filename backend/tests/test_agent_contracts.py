from __future__ import annotations

from typing import Any

from agents.agent_catalog import (
    AgentCatalogTool,
    RoutingToolResult,
    get_top_level_agent_capabilities,
)
from agents.base import AgentContext
from agents.operator_guide.agent import OperatorGuideAgent
from agents.orchestrator import TOP_LEVEL_AGENT_IDS, OrchestratorAgent
from agents.pipeline.graph_edges import TOP_LEVEL_AGENT_BRANCHES
from agents.quest_generator.agent import QuestGeneratorAgent
from agents.router import create_default_agent_router


class FakeRoutingSupportTool:
    name = "fake.routing_tool"

    def invoke(
        self,
        payload: dict[str, Any],
        context: AgentContext,
    ) -> RoutingToolResult:
        return RoutingToolResult(
            name=self.name,
            section="FAKE_ROUTING_SECTION",
            content=f"request={context.request_id}; keys={sorted(payload)}",
        )


def test_default_agent_router_contains_leaf_agents() -> None:
    router = create_default_agent_router()

    assert router.list_agent_ids() == [
        "new_material_generator",
        "operator_guide.machine_help",
        "operator_guide.recipe_explainer",
        "operator_guide.troubleshooter",
        "process_optimizer",
        "quest_generator.economy_quest",
        "quest_generator.production_quest",
    ]


def test_agents_expose_tools_tuple() -> None:
    router = create_default_agent_router()
    agents = [
        *(router.get(agent_id) for agent_id in router.list_agent_ids()),
        OrchestratorAgent(),
        OperatorGuideAgent(),
        QuestGeneratorAgent(),
    ]

    for agent in agents:
        assert isinstance(agent.tools, tuple)


def test_top_level_agent_catalog_covers_orchestrator_choices() -> None:
    capabilities = get_top_level_agent_capabilities()

    assert tuple(capability.agent_id for capability in capabilities) == TOP_LEVEL_AGENT_IDS
    assert set(TOP_LEVEL_AGENT_IDS) == set(TOP_LEVEL_AGENT_BRANCHES)
    for capability in capabilities:
        assert capability.summary
        assert capability.when_to_use


def test_agent_catalog_tool_returns_routing_prompt_section() -> None:
    context = AgentContext(request_id="request-catalog-tool")

    result = AgentCatalogTool().invoke({}, context)

    assert result.name == "agent_catalog.get_capabilities"
    assert result.section == "AGENT_CAPABILITIES"
    for agent_id in TOP_LEVEL_AGENT_IDS:
        assert f"- {agent_id}:" in result.content


def test_orchestrator_routing_prompt_includes_agent_capabilities() -> None:
    orchestrator = OrchestratorAgent()
    context = AgentContext(
        request_id="request-orchestrator-contract",
        metadata={"screen": "factory-floor"},
    )

    prompt = orchestrator.build_routing_prompt(
        {"question": "설비 병목을 줄이고 싶어"},
        context,
        requested_agent="process_optimizer",
    )

    assert "[AGENT_CAPABILITIES]" in prompt
    for agent_id in TOP_LEVEL_AGENT_IDS:
        assert f"- {agent_id}:" in prompt


def test_orchestrator_calls_routing_support_tools() -> None:
    orchestrator = OrchestratorAgent(tools=(FakeRoutingSupportTool(),))
    context = AgentContext(request_id="request-fake-tool")

    prompt = orchestrator.build_routing_prompt({"question": "route me"}, context)

    assert "[FAKE_ROUTING_SECTION]" in prompt
    assert "request=request-fake-tool; keys=['question']" in prompt
    assert "[AGENT_CAPABILITIES]" not in prompt


def test_orchestrator_accepts_empty_tools_override() -> None:
    orchestrator = OrchestratorAgent(tools=())
    context = AgentContext(request_id="request-empty-tools")

    prompt = orchestrator.build_routing_prompt({"question": "route me"}, context)

    assert "[AGENT_CAPABILITIES]" not in prompt
    assert orchestrator.tools == ()


def test_sub_orchestrators_use_structured_prompt_id_contract() -> None:
    operator_guide = OperatorGuideAgent()
    quest = QuestGeneratorAgent()
    context = AgentContext(
        request_id="request-contract",
        metadata={"screen": "factory-floor"},
    )

    operator_guide_prompt = operator_guide.build_routing_prompt(
        {"question": "How do I use this machine?"},
        context,
    )
    quest_prompt = quest.build_routing_prompt(
        {"message": "create a production quest"},
        context,
    )

    for prompt in (operator_guide_prompt, quest_prompt):
        assert "[ROLE]" in prompt
        assert "[TASK]" in prompt
        assert "[ALLOWED_LEAF_AGENT_IDS]" in prompt
        assert "[REQUEST_CONTEXT]" in prompt
        assert "[REQUEST_PAYLOAD]" in prompt
        assert "[OUTPUT_CONTRACT]" in prompt
        assert "compact JSON" not in prompt
        assert '{"sub_agent"' not in prompt

    assert "operator_guide.machine_help" in operator_guide_prompt
    assert "operator_guide.recipe_explainer" in operator_guide_prompt
    assert "operator_guide.troubleshooter" in operator_guide_prompt
    assert "quest_generator.production_quest" in quest_prompt
    assert "quest_generator.economy_quest" in quest_prompt
    assert "quest_generator.tutorial_quest" not in quest_prompt
    assert "quest_generator.exploration_quest" not in quest_prompt
