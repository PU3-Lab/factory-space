from __future__ import annotations

from typing import Protocol

import pytest

from agents.base import AgentContext, AgentRunResult
from agents.new_material_generator import NewMaterialGeneratorAgent
from agents.operator_guide.machine_help import MachineHelpAgent
from agents.operator_guide.recipe_explainer import RecipeExplainerAgent
from agents.operator_guide.troubleshooter import TroubleshooterAgent
from agents.process_optimizer import ProcessOptimizerAgent
from agents.quest_generator.economy_quest import EconomyQuestAgent
from agents.quest_generator.production_quest import ProductionQuestAgent
from agents.quest_generator.tools import PRODUCTION_QUEST_SELECTION_TOOL_NAME


class LeafAgent(Protocol):
    agent_id: str

    def build_prompt(self, payload: dict[str, object], context: AgentContext) -> str:
        """Build a leaf-agent prompt."""

    def fallback(
        self,
        payload: dict[str, object],
        context: AgentContext,
    ) -> AgentRunResult:
        """Build a fallback result."""


@pytest.fixture
def context() -> AgentContext:
    return AgentContext(
        request_id="request-test",
        session_id="session-test",
        client_id="client-test",
        metadata={"screen": "factory"},
    )


def test_process_optimizer_prompt_excludes_request_id(context: AgentContext) -> None:
    agent = ProcessOptimizerAgent()

    prompt = agent.build_prompt({"machines": [{"id": "m-1"}]}, context)

    assert "공장 snapshot에서 공정 병목" in prompt
    assert "m-1" in prompt
    assert context.request_id not in prompt


def test_process_optimizer_fallback_counts_direct_machine_snapshot(
    context: AgentContext,
) -> None:
    agent = ProcessOptimizerAgent()

    result = agent.fallback({"machines": [{"id": "m-1"}, {"id": "m-2"}]}, context)

    assert result.agent == "process_optimizer"
    assert result.metadata == {"fallback": True}
    assert "2개 설비" in result.payload["recommendations"][0]["reason"]


def test_process_optimizer_fallback_counts_nested_factory_state(
    context: AgentContext,
) -> None:
    agent = ProcessOptimizerAgent()

    result = agent.fallback({"factory_state": {"machines": [{"id": "m-1"}]}}, context)

    assert "1개 설비" in result.payload["recommendations"][0]["reason"]


def test_new_material_generator_uses_goal_as_material_role(
    context: AgentContext,
) -> None:
    agent = NewMaterialGeneratorAgent()

    prompt = agent.build_prompt({"goal": "heat resistant plate"}, context)
    result = agent.fallback({"goal": "heat resistant plate"}, context)

    assert "공장 신소재 후보" in prompt
    assert context.request_id not in prompt
    assert result.agent == "new_material_generator"
    assert result.payload["materials"][0]["role"] == "heat resistant plate"
    assert result.metadata == {"fallback": True}


@pytest.mark.parametrize(
    ("agent", "expected_topic", "expected_prompt"),
    [
        (RecipeExplainerAgent(), "recipe", "레시피 질문에 답변"),
        (MachineHelpAgent(), "machine", "설비 도움말 질문에 답변"),
        (TroubleshooterAgent(), "troubleshooting", "공장 문제를 진단"),
    ],
)
def test_operator_guide_leaf_agents_return_normalized_fallbacks(
    agent: LeafAgent,
    expected_topic: str,
    expected_prompt: str,
    context: AgentContext,
) -> None:
    payload = {"question": "How does this work?"}

    prompt = agent.build_prompt(payload, context)
    result = agent.fallback(payload, context)

    assert expected_prompt in prompt
    assert result.agent == "operator_guide"
    assert result.payload["question"] == "How does this work?"
    assert result.payload["topic"] == expected_topic
    assert result.metadata == {"fallback": True, "sub_agent": agent.agent_id}


@pytest.mark.parametrize(
    ("agent", "expected_type", "expected_prompt"),
    [
        (ProductionQuestAgent(), "production", "생산 퀘스트 선택 에이전트"),
        (EconomyQuestAgent(), "economy", "경제 퀘스트 생성 에이전트"),
    ],
)
def test_quest_leaf_agents_return_normalized_fallbacks(
    agent: LeafAgent,
    expected_type: str,
    expected_prompt: str,
    context: AgentContext,
) -> None:
    prompt = agent.build_prompt({"quest_type": expected_type}, context)
    result = agent.fallback({"quest_type": expected_type}, context)

    assert expected_prompt in prompt
    assert result.agent == "quest_generator"
    if isinstance(agent, ProductionQuestAgent):
        assert len(result.payload["quests"]) == 5
        assert result.payload["quests"][0]["type"] == expected_type
    else:
        assert result.payload["quest"]["type"] == expected_type
    assert result.metadata == {"fallback": True, "sub_agent": agent.agent_id}


def test_production_quest_fallback_returns_five_example_quests(
    context: AgentContext,
) -> None:
    agent = ProductionQuestAgent()

    result = agent.fallback({}, context)

    assert result.agent == "quest_generator"
    assert len(result.payload["quests"]) == 5
    assert all(isinstance(quest["id"], int) for quest in result.payload["quests"])
    assert set(result.payload["quests"][0]["objectives"][0]) == {
        "target_item_id",
        "quantity",
    }
    assert result.metadata == {"fallback": True, "sub_agent": agent.agent_id}


def test_production_quest_agent_exposes_selection_tool(
    context: AgentContext,
) -> None:
    agent = ProductionQuestAgent()

    prompt = agent.build_prompt({}, context)

    assert [tool.name for tool in agent.tools] == [PRODUCTION_QUEST_SELECTION_TOOL_NAME]
    assert PRODUCTION_QUEST_SELECTION_TOOL_NAME in prompt
    assert "tool_call" in prompt
