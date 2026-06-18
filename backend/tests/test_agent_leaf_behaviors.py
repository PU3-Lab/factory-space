from __future__ import annotations

from typing import Protocol

import pytest

from agents.base import AgentContext, AgentRunResult
from agents.material_generation.agent import MaterialCreationAgent
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


def test_material_creation_agent_contract(
    context: AgentContext,
) -> None:
    agent = MaterialCreationAgent()

    prompt = agent.build_prompt({}, context)
    result = agent.fallback({}, context)

    assert prompt == "Initiating material synthesis agent."
    assert result.agent == "material_generation"
    assert result.payload["result_type"] == "failed_result"


def test_new_material_generator_prompt_includes_constraints(
    context: AgentContext,
) -> None:
    agent = NewMaterialGeneratorAgent()

    prompt = agent.build_prompt({"goal": "heat-resistant alloy"}, context)

    assert "신소재" in prompt
    assert "heat-resistant alloy" in prompt
    assert "materials" in prompt
    assert context.request_id not in prompt


def test_new_material_generator_fallback_returns_materials_array(
    context: AgentContext,
) -> None:
    agent = NewMaterialGeneratorAgent()

    result = agent.fallback({"goal": "heat-resistant alloy"}, context)

    assert result.agent == "new_material_generator"
    assert result.metadata == {"fallback": True}
    materials = result.payload["materials"]
    assert isinstance(materials, list) and len(materials) >= 1
    assert materials[0]["role"] == "heat-resistant alloy"


@pytest.mark.parametrize(
    ("agent", "expected_topic", "expected_prompt"),
    [
        (RecipeExplainerAgent(), "recipe", "recipe question"),
        (MachineHelpAgent(), "machine", "machine help question"),
        (TroubleshooterAgent(), "troubleshooting", "troubleshooting question"),
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
    assert "[CSV_EVIDENCE]" in prompt
    assert "[OUTPUT_CONTRACT]" in prompt
    prompt_messages = agent.build_prompt_messages(payload, context)
    assert prompt_messages[0]["role"] == "system"
    assert "tutorial operator inside Factory Space" in prompt_messages[0]["content"]
    assert prompt_messages[1]["role"] == "user"
    assert "[CSV_EVIDENCE]" in prompt_messages[1]["content"]
    assert result.agent == "operator_guide"
    assert result.payload["question"] == "How does this work?"
    assert result.payload["topic"] == expected_topic
    assert result.payload["actions"] == []
    assert result.payload["final_answer"]
    assert "answer" not in result.payload
    assert "text" not in result.payload
    assert result.metadata["fallback"] is True
    assert result.metadata["sub_agent"] == agent.agent_id
    assert result.metadata["question_type"] == "unknown_question"
    assert result.metadata["sources"] == []
    assert result.metadata["recommended_actions"][0]["action_id"] == (
        "action_answer_unknown_without_guessing"
    )


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


def test_production_quest_prompt_is_game_state_driven_not_request_driven(
    context: AgentContext,
) -> None:
    """생산 퀘스트는 자연어 '만들어줘' 요청이 아니라 게임 상태로 자동 생성한다.

    초보자용 설명:
        게임에서 퀘스트 요청이 오면 플레이어가 어떤 퀘스트를 원하는지 글로 적는 게 아니라,
        게임 상태(진행도/보유 자원 등)를 보고 시스템이 알아서 퀘스트를 고릅니다.
        게임 상태가 아직 없으면 대표 퀘스트를 자동으로 고릅니다.
    """
    agent = ProductionQuestAgent()

    prompt = agent.build_prompt(
        {"game_state": {"stage": 2, "inventory": ["iron_ore"]}}, context
    )

    assert "[GAME_STATE]" in prompt
    assert "자동" in prompt
    assert "stage" in prompt and "iron_ore" in prompt
    assert "[REQUEST_PAYLOAD]" not in prompt

    empty_prompt = agent.build_prompt({}, context)
    assert "[GAME_STATE]" in empty_prompt


def test_economy_quest_prompt_is_game_state_driven_not_request_driven(
    context: AgentContext,
) -> None:
    """경제 퀘스트도 자연어 요청이 아니라 게임 상태로 자동 생성한다."""
    agent = EconomyQuestAgent()

    prompt = agent.build_prompt({"game_state": {"warehouse_overflow": True}}, context)

    assert "[GAME_STATE]" in prompt
    assert "자동" in prompt
    assert "warehouse_overflow" in prompt
    assert "다음 요청을 바탕으로" not in prompt

    empty_prompt = agent.build_prompt({}, context)
    assert "[GAME_STATE]" in empty_prompt
