from __future__ import annotations

from agents.base import AgentContext
from agents.operator_guide.agent import OperatorGuideAgent
from agents.quest_generator.agent import QuestGeneratorAgent
from agents.router import create_default_agent_router


def test_default_agent_router_contains_leaf_agents() -> None:
    router = create_default_agent_router()

    assert router.list_agent_ids() == [
        "new_material_generator",
        "operator_guide.machine_help",
        "operator_guide.recipe_explainer",
        "operator_guide.troubleshooter",
        "process_optimizer",
        "quest_generator.economy_quest",
        "quest_generator.exploration_quest",
        "quest_generator.production_quest",
        "quest_generator.tutorial_quest",
    ]


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
    assert "quest_generator.tutorial_quest" in quest_prompt
    assert "quest_generator.exploration_quest" in quest_prompt
    assert "quest_generator.economy_quest" in quest_prompt
