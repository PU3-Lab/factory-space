from __future__ import annotations

from agents.base import AgentContext
from agents.manual_qa.agent import ManualQaAgent
from agents.quest_generator.agent import QuestGeneratorAgent
from agents.router import create_default_agent_router


def test_default_agent_router_contains_leaf_agents() -> None:
    router = create_default_agent_router()

    assert router.list_agent_ids() == [
        "manual_qa.machine_help",
        "manual_qa.recipe_explainer",
        "manual_qa.troubleshooter",
        "new_material_generator",
        "process_optimizer",
        "quest_generator.economy_quest",
        "quest_generator.exploration_quest",
        "quest_generator.production_quest",
        "quest_generator.tutorial_quest",
    ]


def test_sub_orchestrators_use_structured_prompt_id_contract() -> None:
    manual = ManualQaAgent()
    quest = QuestGeneratorAgent()
    context = AgentContext(
        request_id="request-contract",
        metadata={"screen": "factory-floor"},
    )

    manual_prompt = manual.build_routing_prompt(
        {"question": "How do I use this machine?"},
        context,
    )
    quest_prompt = quest.build_routing_prompt(
        {"message": "create a production quest"},
        context,
    )

    for prompt in (manual_prompt, quest_prompt):
        assert "[ROLE]" in prompt
        assert "[TASK]" in prompt
        assert "[ALLOWED_LEAF_AGENT_IDS]" in prompt
        assert "[REQUEST_CONTEXT]" in prompt
        assert "[REQUEST_PAYLOAD]" in prompt
        assert "[OUTPUT_CONTRACT]" in prompt
        assert "compact JSON" not in prompt
        assert '{"sub_agent"' not in prompt

    assert "manual_qa.machine_help" in manual_prompt
    assert "manual_qa.recipe_explainer" in manual_prompt
    assert "manual_qa.troubleshooter" in manual_prompt
    assert "quest_generator.production_quest" in quest_prompt
    assert "quest_generator.tutorial_quest" in quest_prompt
    assert "quest_generator.exploration_quest" in quest_prompt
    assert "quest_generator.economy_quest" in quest_prompt
